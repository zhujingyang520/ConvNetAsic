/*
 * Filename: adder_model.hpp
 * --------------------------
 * This file exports the AdderModel, which models the area, energy metrics of a
 * specified adder.
 */

#ifndef __ADDER_MODEL_HPP__
#define __ADDER_MODEL_HPP__

#include "proto/config.pb.h"

class AdderModel {
  public:
    // constructor: provide the adder bit width, the technology node [nm]
    AdderModel(int bit_width, int tech_node=28);
    ~AdderModel() {}

    // Area & Power Metric of the adder
    // Return the Area of adder [um2]
    double Area() const;
    double Power() const;

  private:
    // adder bit width
    int bit_width_;
    // technology node
    int tech_node_;
};

#endif
