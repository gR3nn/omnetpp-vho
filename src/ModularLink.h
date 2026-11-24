#pragma once
#include <deque>
#include "Access.h"
#include "Request_m.h"

class ChunkDelay : public omnetpp::cSimpleModule {
    double load = 1;
    double radio = 0;
    double bitsSent = 0;
    std::deque<double> throughputHistory;
    long chunks = 0;
    TransferFile *active = nullptr;
    omnetpp::simtime_t busyTime;
    omnetpp::simtime_t busySince;

    void initialize() override;
    void handleMessage(omnetpp::cMessage *message) override;
    void finish() override;

  public:
    Offer offer() const;
    double predictedThroughput() const;
    void resetPrediction() { throughputHistory.clear(); }
    ~ChunkDelay() override;
};
