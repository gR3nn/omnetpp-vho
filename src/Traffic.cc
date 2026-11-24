#include <omnetpp.h>
#include "Request_m.h"

using namespace omnetpp;

class Traffic : public cSimpleModule {
    cMessage *arrival = nullptr;
    int remaining = 0;

    void initialize() override
    {
        remaining = par("files").intValue();
        if (remaining < 0 || par("interval").doubleValue() <= 0 ||
            par("fileSize").doubleValue() <= 0 || par("deadline").doubleValue() <= 0)
            throw cRuntimeError("Invalid traffic parameters");
        arrival = new cMessage("generate file");
        if (remaining > 0)
            scheduleAt(SIMTIME_ZERO, arrival);
    }

    void handleMessage(cMessage *) override
    {
        auto *request = new Request("file request");
        request->setSizeBits(par("fileSize").doubleValueInUnit("B") * 8);
        request->setDeadline(simTime() + par("deadline").doubleValueInUnit("s"));
        send(request, "out");
        if (--remaining > 0)
            scheduleAfter(par("interval").doubleValueInUnit("s"), arrival);
    }

  public:
    ~Traffic() override { cancelAndDelete(arrival); }
};

Define_Module(Traffic);
