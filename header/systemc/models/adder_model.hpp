/*
 * Filename: adder_model.hpp
 * --------------------------
 * This file exports the AdderModel, which models the area, energy metrics of a
 * specified adder.
 */

#ifndef __ADDER_MODEL_HPP__
#define __ADDER_MODEL_HPP__

#include "proto/config.pb.h"
#include "header/systemc/models/model.hpp"

class AdderModel : public Model {
  public:
    // constructor: provide the adder bit width, the technology node [nm]
    AdderModel(int bit_width, int tech_node=28, double clk_freq=1.);
    virtual ~AdderModel() {}

    // Area & Power Metric of the adder
    // Return the Area of adder [um2]
    virtual double Area() const;
    // Return the static power consumption of the adder
    virtual double StaticPower() const;
    // Dynamic energy of one operation
    double DynamicEnergyOfOneOperation() const;

  private:
    // adder bit width
    int bit_width_;
};

#endif
