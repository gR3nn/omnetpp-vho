#include "ModularLink.h"
#include <algorithm>
#include <cmath>
#include <numeric>

using namespace omnetpp;

class LoadConditions : public cSimpleModule {
    cMessage *update = nullptr;

    void initialize() override
    {
        if (par("interval").doubleValue() <= 0 || par("variation").doubleValue() < 0 ||
            par("variation").doubleValue() >= 1)
            throw cRuntimeError("Invalid load conditions");
        update = new cMessage("sample load");
        update->setSchedulingPriority(-2);
        scheduleAt(simTime(), update);
    }

    void handleMessage(cMessage *) override
    {
        auto *sample = new LinkCondition("load factor");
        sample->setSchedulingPriority(-1);
        sample->setFactor(1 - par("variation").doubleValue() * getRNG(0)->doubleRand());
        send(sample, "out");
        scheduleAfter(par("interval").doubleValueInUnit("s"), update);
    }

  public:
    ~LoadConditions() override { cancelAndDelete(update); }
};

class RadioConditions : public cSimpleModule {
    cMessage *update = nullptr;

    void initialize() override
    {
        if (par("interval").doubleValue() <= 0 || par("radius").doubleValue() <= 0 ||
            par("routeLength").doubleValue() <= 0 || par("speed").doubleValue() < 0 ||
            par("initialPosition").doubleValue() < 0 ||
            par("initialPosition").doubleValue() > par("routeLength").doubleValue())
            throw cRuntimeError("Invalid radio conditions");
        update = new cMessage("sample radio");
        update->setSchedulingPriority(-2);
        scheduleAt(simTime(), update);
    }

    void handleMessage(cMessage *) override
    {
        double length = par("routeLength").doubleValueInUnit("m");
        double distance = std::fmod(par("initialPosition").doubleValueInUnit("m") +
            simTime().dbl() * par("speed").doubleValueInUnit("mps"), 2 * length);
        double position = std::min(distance, 2 * length - distance);
        double ratio = std::abs(position - par("center").doubleValueInUnit("m")) /
            par("radius").doubleValueInUnit("m");
        auto *sample = new LinkCondition("radio factor");
        sample->setSchedulingPriority(-1);
        sample->setFactor(ratio > 1 ? 0 : 1 - 0.6 * ratio);
        send(sample, "out");
        scheduleAfter(par("interval").doubleValueInUnit("s"), update);
    }

  public:
    ~RadioConditions() override { cancelAndDelete(update); }
};

class DataBuffer : public cSimpleModule {
    cQueue files{"buffered files"};
    cMessage *forward = nullptr;

    void initialize() override { forward = new cMessage("forward file"); }

    void handleMessage(cMessage *message) override
    {
        if (message == forward) {
            send(check_and_cast<cMessage *>(files.pop()), "out");
        } else {
            files.insert(message);
        }
        if (!files.isEmpty() && !forward->isScheduled())
            scheduleAt(simTime(), forward);
    }

  public:
    ~DataBuffer() override { cancelAndDelete(forward); }
};

class TransferLoop : public cSimpleModule {
    void handleMessage(cMessage *message) override
    {
        auto *file = check_and_cast<TransferFile *>(message);
        double remaining = file->getRemainingBits() - file->getChunkBits();
        file->setRemainingBits(remaining);
        file->setDelivered(remaining == 0);
        send(file, remaining == 0 || simTime() >= file->getDeadline() ? "done" : "again");
    }
};

Define_Module(LoadConditions);
Define_Module(RadioConditions);
Define_Module(DataBuffer);
Define_Module(TransferLoop);
Define_Module(ChunkDelay);

void ChunkDelay::initialize()
{
    if (par("peakRate").doubleValue() <= 0 || par("price").doubleValue() < 0 ||
        par("chunkSize").doubleValue() <= 0 || par("pollInterval").doubleValue() <= 0)
        throw cRuntimeError("Invalid link parameters");
    WATCH(load);
    WATCH(radio);
    WATCH(chunks);
}

Offer ChunkDelay::offer() const
{
    return {par("peakRate").doubleValueInUnit("bps") * load * radio, par("price").doubleValue()};
}

double ChunkDelay::predictedThroughput() const
{
    if (throughputHistory.empty())
        return 0;
    return std::accumulate(throughputHistory.begin(), throughputHistory.end(), 0.0) /
        throughputHistory.size();
}

void ChunkDelay::handleMessage(cMessage *message)
{
    if (message->arrivedOn("loadIn") || message->arrivedOn("radioIn")) {
        auto *sample = check_and_cast<LinkCondition *>(message);
        (message->arrivedOn("loadIn") ? load : radio) = sample->getFactor();
        delete sample;
        return;
    }

    auto *file = check_and_cast<TransferFile *>(message);
    if (file->isSelfMessage()) {
        active = nullptr;
        if (file->getChunkBits() > 0) {
            busyTime += simTime() - busySince;
            ++chunks;
            bitsSent += file->getChunkBits();
            file->setCredits(file->getCredits() + offer().pricePerMB * file->getChunkBits() / 8e6);
            if (file->getChunkBits() == file->getRemainingBits()) {
                double duration = (simTime() - file->getTransmissionStart()).dbl();
                throughputHistory.push_back(file->getSizeBits() / duration);
                if (throughputHistory.size() > 5)
                    throughputHistory.pop_front();
            }
            send(file, "out");
            return;
        }
    }

    file->setChunkBits(0);
    if (simTime() >= file->getDeadline()) {
        send(file, "out");
        return;
    }

    if (active)
        throw cRuntimeError("Link received a second file while busy");
    active = file;
    if (simTime() < file->getReadyAt()) {
        scheduleAt(std::min(file->getReadyAt(), file->getDeadline()), file);
        return;
    }
    double rate = offer().bitsPerSecond;
    if (rate <= 0) {
        SimTime nextPoll(par("pollInterval").doubleValueInUnit("s"));
        scheduleAt(std::min(simTime() + nextPoll, file->getDeadline()), file);
        return;
    }

    double bits = std::min(file->getRemainingBits(), par("chunkSize").doubleValueInUnit("B") * 8);
    simtime_t duration = std::max(SimTime::fromRaw(1), SimTime(bits / rate));
    if (simTime() + duration > file->getDeadline()) {
        scheduleAt(file->getDeadline(), file);
        return;
    }
    if (file->getTransmissionStart() < SIMTIME_ZERO)
        file->setTransmissionStart(simTime());
    file->setChunkBits(bits);
    busySince = simTime();
    scheduleAfter(duration, file);
}

void ChunkDelay::finish()
{
    simtime_t used = busyTime;
    if (active && active->getChunkBits() > 0)
        used += simTime() - busySince;
    recordScalar("busySeconds", used.dbl());
    recordScalar("utilization", simTime() > SIMTIME_ZERO ? used.dbl() / simTime().dbl() : 0);
    recordScalar("chunks", chunks);
    recordScalar("bytesSent", bitsSent / 8);
}

ChunkDelay::~ChunkDelay() { cancelAndDelete(active); }
