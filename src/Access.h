#pragma once
#include <omnetpp.h>

struct Offer {
    double bitsPerSecond = 0;
    double pricePerMB = 0;
};

class Access : public omnetpp::cSimpleModule {
  protected:
    void initialize() override;

  public:
    Offer sample(double position);
};
