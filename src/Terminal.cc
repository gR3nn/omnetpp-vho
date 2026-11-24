#include "Access.h"
#include "Request_m.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>

using namespace omnetpp;

class Terminal : public cSimpleModule {
    struct File {
        double remainingBits;
        simtime_t arrival;
        simtime_t deadline;
    };

    std::array<Access *, 2> access{};
    std::deque<File> files;
    cMessage *timer = nullptr;
    simtime_t tick;
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
        timer = new cMessage("sample and transfer");
        scheduleAfter(tick, timer);
    }

    void handleMessage(cMessage *message) override
    {
        if (message != timer) {
            auto *request = check_and_cast<Request *>(message);
            files.push_back({request->getSizeBits(), simTime(), request->getDeadline()});
            ++submitted;
            delete request;
            return;
        }
        double length = par("routeLength").doubleValueInUnit("m");
        double distance = std::fmod((simTime() - tick).dbl() *
                                    par("speed").doubleValueInUnit("mps"), 2 * length);
        double position = std::min(distance, 2 * length - distance);
        std::array<Offer, 2> offers{access[0]->sample(position), access[1]->sample(position)};
        auto fastest = std::max_element(offers.begin(), offers.end(),
            [](const Offer& a, const Offer& b) { return a.bitsPerSecond < b.bitsPerSecond; });
        if (!files.empty()) {
            File& file = files.front();
            simtime_t start = std::max(file.arrival, simTime() - tick);
            simtime_t duration = std::max(SIMTIME_ZERO, std::min(simTime(), file.deadline) - start);
            double bits = std::min(file.remainingBits, fastest->bitsPerSecond * duration.dbl());
            file.remainingBits -= bits;
            credits += fastest->pricePerMB * bits / 8e6;
            if (file.remainingBits <= 0) {
                ++completed;
                files.pop_front();
            }
        }
        auto firstExpired = std::remove_if(files.begin(), files.end(),
            [this](const File& file) { return file.deadline <= simTime(); });
        expired += std::distance(firstExpired, files.end());
        files.erase(firstExpired, files.end());
        scheduleAfter(tick, timer);
    }

    void finish() override
    {
        recordScalar("submitted", submitted);
        recordScalar("completed", completed);
        recordScalar("expired", expired);
        recordScalar("pending", files.size());
        recordScalar("credits", credits);
    }

  public:
    ~Terminal() override { cancelAndDelete(timer); }
};

Define_Module(Terminal);
