#include "ModularLink.h"
#include "Selection.h"
#include <algorithm>
#include <string>

using namespace omnetpp;

namespace {
constexpr int NoCoverage = -1;
constexpr int NoFeasibleNetwork = -2;

double delayUtility(double delay, double idealTime, double deadline, double deadlineValue)
{
    if (delay > deadline)
        return 0;
    if (delay <= idealTime)
        return 1;
    return deadlineValue + (1 - deadlineValue) * (deadline - delay) / (deadline - idealTime);
}
}

class FileServer : public cSimpleModule {
    cQueue queue{"queued files"};
    TransferFile *selecting = nullptr;
    cMessage *retry = nullptr;
    bool awaitingAck = false;
    int current = -1;
    long handovers = 0;
    long submitted = 0;

    void initialize() override
    {
        if (par("scanInterval").doubleValue() <= 0 || par("handoverDelay").doubleValue() < 0)
            throw cRuntimeError("Invalid server timing");
        retry = new cMessage("rescan access");
        WATCH(current);
        WATCH(handovers);
        WATCH(awaitingAck);
    }

    void dispatch()
    {
        if (awaitingAck || selecting || retry->isScheduled() || queue.isEmpty())
            return;
        selecting = check_and_cast<TransferFile *>(queue.pop());
        if (selecting->getDeadline() <= simTime()) {
            awaitingAck = true;
            send(selecting, "expiredOut");
            selecting = nullptr;
            return;
        }
        auto *query = new SelectionQuery("choose network");
        query->setSizeBits(selecting->getSizeBits());
        query->setDeadline(selecting->getDeadline());
        query->setFileArrival(selecting->getCreationTime());
        query->setCurrentAccess(current);
        send(query, "selection$o");
    }

    void handleMessage(cMessage *message) override
    {
        if (message == retry) {
            dispatch();
            return;
        }
        if (message->arrivedOn("ack")) {
            awaitingAck = false;
            delete message;
            dispatch();
            return;
        }
        if (message->arrivedOn("selection$i")) {
            auto *query = check_and_cast<SelectionQuery *>(message);
            int selected = query->getSelectedAccess();
            delete query;
            if (selected == NoFeasibleNetwork) {
                selecting->setDeclined(true);
                awaitingAck = true;
                send(selecting, "expiredOut");
                selecting = nullptr;
                return;
            }
            if (selected == NoCoverage) {
                scheduleAt(std::min(selecting->getDeadline(),
                    simTime() + par("scanInterval").doubleValueInUnit("s")), retry);
                if (queue.isEmpty())
                    queue.insert(selecting);
                else
                    queue.insertBefore(queue.front(), selecting);
                selecting = nullptr;
                return;
            }
            simtime_t setup = SIMTIME_ZERO;
            if (current >= 0 && current != selected) {
                ++handovers;
                setup = par("handoverDelay").doubleValueInUnit("s");
            }
            current = selected;
            selecting->setReadyAt(simTime() + setup);
            selecting->setRemainingBits(selecting->getSizeBits());
            awaitingAck = true;
            send(selecting, "networkOut", selected);
            selecting = nullptr;
            return;
        }
        ++submitted;
        queue.insert(check_and_cast<TransferFile *>(message));
        dispatch();
    }

    void finish() override
    {
        recordScalar("submitted", submitted);
        recordScalar("pending", queue.getLength() + (awaitingAck || selecting ? 1 : 0));
        recordScalar("handovers", handovers);
    }

  public:
    ~FileServer() override
    {
        cancelAndDelete(retry);
        delete selecting;
    }
};

class HandoverAlgorithm : public cSimpleModule {
    std::array<ChunkDelay *, 2> links{};
    Policy policy = Policy::Fastest;
    bool ormond = false;
    std::array<int, 2> staleDecisions{};

    void initialize() override
    {
        auto *top = getParentModule();
        links = {
            check_and_cast<ChunkDelay *>(top->getSubmodule("network1")->getSubmodule("dly")),
            check_and_cast<ChunkDelay *>(top->getSubmodule("network2")->getSubmodule("dly"))};
        std::string name = par("policy").stringValue();
        if (name == "fastest")
            policy = Policy::Fastest;
        else if (name == "cheapest")
            policy = Policy::Cheapest;
        else if (name == "surplus")
            policy = Policy::Surplus;
        else if (name == "ormond")
            ormond = true;
        else
            throw cRuntimeError("Unknown selection policy: %s", name.c_str());
        if (par("valuePerMB").doubleValue() < 0 || par("surplusMargin").doubleValue() < 0 ||
            par("idealTime").doubleValue() < 0 || par("deadlineValueFraction").doubleValue() < 0 ||
            par("deadlineValueFraction").doubleValue() > 1)
            throw cRuntimeError("Invalid value or surplus margin");
    }

    int selectOrmond(const SelectionQuery& query, double handoverSeconds)
    {
        double deadline = (query.getDeadline() - query.getFileArrival()).dbl();
        double age = (simTime() - query.getFileArrival()).dbl();
        double ideal = par("idealTime").doubleValueInUnit("s");
        double bits = query.getSizeBits();
        double valuePerMB = par("valuePerMB").doubleValue();
        double deadlineValue = par("deadlineValueFraction").doubleValue();
        int best = NoFeasibleNetwork;
        double bestScore = 0;
        for (int i = 0; i < static_cast<int>(links.size()); ++i) {
            auto offer = links[i]->offer();
            if (offer.bitsPerSecond <= 0)
                continue;
            double setup = query.getCurrentAccess() >= 0 && i != query.getCurrentAccess()
                ? handoverSeconds : 0;
            double remaining = deadline - age - setup;
            if (remaining <= 0)
                continue;
            double predicted = links[i]->predictedThroughput();
            if (predicted > 0 && bits / predicted > remaining) {
                if (++staleDecisions[i] <= 6)
                    continue;
                links[i]->resetPrediction();
                predicted = 0;
            }
            staleDecisions[i] = 0;
            double eta = predicted > 0 ? std::min(deadline, age + setup + bits / predicted) : deadline;
            double score = bits / 8e6 * (valuePerMB * delayUtility(eta, ideal, deadline, deadlineValue) -
                offer.pricePerMB);
            if (best < 0 || score > bestScore || (score == bestScore && i == query.getCurrentAccess())) {
                best = i;
                bestScore = score;
            }
        }
        return best;
    }

    void handleMessage(cMessage *message) override
    {
        auto *query = check_and_cast<SelectionQuery *>(message);
        double setup = getParentModule()->getSubmodule("svr")->par("handoverDelay").doubleValueInUnit("s");
        query->setSelectedAccess(ormond ? selectOrmond(*query, setup) :
            selectAccess({links[0]->offer(), links[1]->offer()}, policy,
            query->getCurrentAccess(), query->getSizeBits(), (query->getDeadline() - simTime()).dbl(),
            setup, par("valuePerMB").doubleValue(), par("surplusMargin").doubleValue()));
        query->setName("selected network");
        send(query, "server$o");
    }
};

class FileDestination : public cSimpleModule {
    long completed = 0;
    long expired = 0;
    long declined = 0;
    double completionSeconds = 0;
    double realizedValue = 0;
    double credits = 0;
    double deliveredBytes = 0;

    void initialize() override
    {
        WATCH(completed);
        WATCH(expired);
    }

    void handleMessage(cMessage *message) override
    {
        auto *file = check_and_cast<TransferFile *>(message);
        credits += file->getCredits();
        if (file->getDelivered()) {
            ++completed;
            double delay = (simTime() - file->getCreationTime()).dbl();
            double budget = (file->getDeadline() - file->getCreationTime()).dbl();
            completionSeconds += delay;
            deliveredBytes += file->getSizeBits() / 8;
            double utility = delayUtility(delay, par("idealTime").doubleValueInUnit("s"), budget,
                par("deadlineValueFraction").doubleValue());
            realizedValue += file->getSizeBits() / 8e6 * par("valuePerMB").doubleValue() * utility;
        } else if (file->getDeclined()) {
            ++declined;
        } else {
            ++expired;
        }
        delete file;
        send(new cMessage("file acknowledged"), "ack");
    }

    void finish() override
    {
        recordScalar("completed", completed);
        recordScalar("expired", expired);
        recordScalar("declined", declined);
        recordScalar("completionSeconds", completionSeconds);
        if (completed > 0)
            recordScalar("meanCompletionSeconds", completionSeconds / completed);
        recordScalar("deliveredBytes", deliveredBytes);
        recordScalar("credits", credits);
        recordScalar("realizedSurplus", realizedValue - credits);
    }
};

Define_Module(FileServer);
Define_Module(HandoverAlgorithm);
Define_Module(FileDestination);
