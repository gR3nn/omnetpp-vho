#include "Access.h"
#include "Request_m.h"
#include "Selection.h"
#include <string>
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>

using namespace omnetpp;

class Terminal : public cSimpleModule {
    struct File {
        double remainingBits;
        double totalBits;
        simtime_t arrival;
        simtime_t deadline;
    };

    std::array<Access *, 2> access{};
    std::deque<File> files;
    cMessage *timer = nullptr;
    simtime_t tick;
    Policy policy;
    int current = -1;
    simtime_t readyAt;
    simtime_t handoverDelay;
    long handovers = 0;
    double interruptedSeconds = 0;
    double completedBytes = 0;
    double completionSeconds = 0;
    double realizedValue = 0;
    double transferredBytes = 0;
    double position = 0;
    long submitted = 0;
    long completed = 0;
    long expired = 0;
    double credits = 0;

    void initialize() override
    {
        tick = par("tick").doubleValueInUnit("s");
        if (tick <= SIMTIME_ZERO || par("routeLength").doubleValue() <= 0 ||
            par("speed").doubleValue() < 0)
            throw cRuntimeError("Invalid terminal timing or route");
        access = {check_and_cast<Access *>(getParentModule()->getSubmodule("cellular")),
                  check_and_cast<Access *>(getParentModule()->getSubmodule("wifi"))};
        std::string name = par("policy").stringValue();
        if (name == "fastest")
            policy = Policy::Fastest;
        else if (name == "cheapest")
            policy = Policy::Cheapest;
        else if (name == "surplus")
            policy = Policy::Surplus;
        else
            throw cRuntimeError("Unknown selection policy: %s", name.c_str());
        handoverDelay = par("handoverDelay").doubleValueInUnit("s");
        position = par("initialPosition").doubleValueInUnit("m");
        if (handoverDelay < SIMTIME_ZERO || position < 0 || position > par("routeLength").doubleValue() ||
            par("valuePerMB").doubleValue() < 0 || par("surplusMargin").doubleValue() < 0)
            throw cRuntimeError("Invalid handover, position or utility parameters");
        WATCH(position);
        WATCH(current);
        WATCH(handovers);
        WATCH(completed);
        timer = new cMessage("sample and transfer");
        scheduleAfter(tick, timer);
    }

    void handleMessage(cMessage *message) override
    {
        if (message != timer) {
            auto *request = check_and_cast<Request *>(message);
            files.push_back({request->getSizeBits(), request->getSizeBits(), simTime(), request->getDeadline()});
            ++submitted;
            delete request;
            return;
        }
        double length = par("routeLength").doubleValueInUnit("m");
        simtime_t intervalStart = simTime() - tick;
        double distance = std::fmod(par("initialPosition").doubleValueInUnit("m") +
                                    intervalStart.dbl() * par("speed").doubleValueInUnit("mps"),
                                    2 * length);
        position = std::min(distance, 2 * length - distance);
        std::array<Offer, 2> offers{access[0]->sample(position), access[1]->sample(position)};
        if (!files.empty()) {
            File& file = files.front();
            simtime_t start = std::max(file.arrival, intervalStart);
            simtime_t end = std::min(simTime(), file.deadline);
            if (end > start)
                transfer(file, offers, start, end);
        }
        auto firstExpired = std::remove_if(files.begin(), files.end(),
            [this](const File& file) { return file.deadline <= simTime(); });
        expired += std::distance(firstExpired, files.end());
        files.erase(firstExpired, files.end());
        scheduleAfter(tick, timer);
    }

    void transfer(File& file, const std::array<Offer, 2>& offers, simtime_t start, simtime_t end)
    {
        if (start >= readyAt) {
            int selected = selectAccess(offers, policy, current, file.remainingBits,
                (file.deadline - start).dbl(), handoverDelay.dbl(),
                par("valuePerMB").doubleValue(), par("surplusMargin").doubleValue());
            if (selected < 0)
                return;
            if (current >= 0 && selected != current) {
                ++handovers;
                readyAt = start + handoverDelay;
            }
            current = selected;
        }
        interruptedSeconds += std::max(SIMTIME_ZERO, std::min(end, readyAt) - start).dbl();
        double duration = std::max(SIMTIME_ZERO, end - std::max(start, readyAt)).dbl();
        double rate = offers[current].bitsPerSecond;
        double bits = std::min(file.remainingBits, rate * duration);
        file.remainingBits -= bits;
        transferredBytes += bits / 8;
        credits += offers[current].pricePerMB * bits / 8e6;
        if (file.remainingBits <= 0) {
            ++completed;
            completedBytes += file.totalBits / 8;
            double latency = (std::max(start, readyAt) - file.arrival).dbl() + bits / rate;
            completionSeconds += latency;
            double budget = (file.deadline - file.arrival).dbl();
            realizedValue += file.totalBits / 8e6 * par("valuePerMB").doubleValue() *
                             std::max(0.0, 1 - latency / budget);
            files.pop_front();
        }
    }

    void refreshDisplay() const override
    {
        const char *label = current < 0 ? "Unattached" : access[current]->par("technology").stringValue();
        getDisplayString().setTagArg("t", 0, label);
        getDisplayString().setTagArg("p", 0, 180 + 0.5 * position);
    }

    void finish() override
    {
        recordScalar("submitted", submitted);
        recordScalar("completed", completed);
        recordScalar("expired", expired);
        recordScalar("pending", files.size());
        recordScalar("credits", credits);
        recordScalar("handovers", handovers);
        recordScalar("handoverInterruptionSeconds", interruptedSeconds);
        recordScalar("transferredBytes", transferredBytes);
        recordScalar("completedBytes", completedBytes);
        recordScalar("completionSeconds", completionSeconds);
        recordScalar("realizedValue", realizedValue);
        recordScalar("realizedSurplus", realizedValue - credits);
    }

  public:
    ~Terminal() override { cancelAndDelete(timer); }
};

Define_Module(Terminal);
