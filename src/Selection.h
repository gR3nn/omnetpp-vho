#pragma once
#include <array>
#include "Access.h"

enum class Policy { Fastest, Cheapest, Surplus };

int selectAccess(const std::array<Offer, 2>& offers, Policy policy, int current,
                 double bits, double timeLeft, double handoverSeconds,
                 double valuePerMB, double margin);
