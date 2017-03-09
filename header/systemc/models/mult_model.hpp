/*
 * Filename: mult_model.hpp
 * -------------------------
 * This file exports the MultModel, which models the area, energy metrics of a
 * specified multiplier.
 */

#ifndef __MULT_MODEL_HPP__
#define __MULT_MODEL_HPP__

#include "proto/config.pb.h"
#include "header/systemc/models/model.hpp"

class MultModel : public Model {
  public:
    // constructor: provide the multiplier bit width, the technology node [nm]
    MultModel(int bit_width, int tech_node=28);
    // destructor
    virtual ~MultModel() {}

    // Area & Power Metric of the multiplier
    // Returns the Area of multiplier [um2]
    virtual double Area() const;
    // Return the static power consumption of the multiplier
    virtual double StaticPower() const;
    // Dynamic energy of one operation
    double DynamicEnergyOfOneOperation() const;

  private:
    // multiplier bit width
    int bit_width_;
};

#endif
