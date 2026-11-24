#include "Access.h"
#include <cmath>

using namespace omnetpp;
Define_Module(Access);

void Access::initialize()
{
    if (par("radius").doubleValue() <= 0 || par("peakRate").doubleValue() <= 0 ||
        par("price").doubleValue() < 0 || par("variation").doubleValue() < 0 ||
        par("variation").doubleValue() >= 1)
        throw cRuntimeError("Invalid access radius, peak rate, price or variation");
}

Offer Access::sample(double position)
{
    double relativeDistance = std::abs(position - par("center").doubleValueInUnit("m")) /
                              par("radius").doubleValueInUnit("m");
    double variation = par("variation").doubleValue();
    // Consume one draw per tick, even outside coverage, for paired policy comparisons.
    double loadFactor = 1 - variation * getRNG(0)->doubleRand();
    if (relativeDistance > 1)
        return {};
    return {par("peakRate").doubleValueInUnit("bps") * (1 - 0.6 * relativeDistance) * loadFactor,
            par("price").doubleValue()};
}
