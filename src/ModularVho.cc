#include "ModularLink.h"
#include "Selection.h"
#include <algorithm>
#include <string>

using namespace omnetpp;

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
            if (selected < 0) {
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
    Policy policy;

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
        else
            throw cRuntimeError("Unknown selection policy: %s", name.c_str());
        if (par("valuePerMB").doubleValue() < 0 || par("surplusMargin").doubleValue() < 0)
            throw cRuntimeError("Invalid value or surplus margin");
    }

    void handleMessage(cMessage *message) override
    {
        auto *query = check_and_cast<SelectionQuery *>(message);
        double setup = getParentModule()->getSubmodule("svr")->par("handoverDelay").doubleValueInUnit("s");
        query->setSelectedAccess(selectAccess({links[0]->offer(), links[1]->offer()}, policy,
            query->getCurrentAccess(), query->getSizeBits(), (query->getDeadline() - simTime()).dbl(),
            setup, par("valuePerMB").doubleValue(), par("surplusMargin").doubleValue()));
        query->setName("selected network");
        send(query, "server$o");
    }
};

class FileDestination : public cSimpleModule {
    long completed = 0;
    long expired = 0;
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
            realizedValue += file->getSizeBits() / 8e6 * par("valuePerMB").doubleValue() *
                             std::max(0.0, 1 - delay / budget);
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
