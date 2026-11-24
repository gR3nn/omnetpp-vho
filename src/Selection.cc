#include "Selection.h"
#include <algorithm>
#include <limits>

int selectAccess(const std::array<Offer, 2>& offers, Policy policy, int current,
                 double bits, double timeLeft, double handoverSeconds,
                 double valuePerMB, double margin)
{
    std::array<double, 2> scores;
    scores.fill(-std::numeric_limits<double>::infinity());
    int best = -1;
    for (int i = 0; i < static_cast<int>(offers.size()); ++i) {
        const auto& offer = offers[i];
        if (offer.bitsPerSecond <= 0)
            continue;
        double setup = current >= 0 && i != current ? handoverSeconds : 0;
        double eta = setup + bits / offer.bitsPerSecond;
        switch (policy) {
            case Policy::Fastest: scores[i] = offer.bitsPerSecond; break;
            case Policy::Cheapest: scores[i] = -offer.pricePerMB; break;
            case Policy::Surplus:
                scores[i] = bits / 8e6 *
                    (valuePerMB * std::max(0.0, 1 - eta / timeLeft) - offer.pricePerMB);
                break;
        }
        if (best < 0 || scores[i] > scores[best] ||
            (scores[i] == scores[best] && i == current))
            best = i;
    }
    if (policy == Policy::Surplus && best >= 0 && current >= 0 &&
        offers[current].bitsPerSecond > 0 && scores[best] - scores[current] <= margin)
        return current;
    return best;
}
