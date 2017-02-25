/*
 * Filename: mult_model.hpp
 * -------------------------
 * This file exports the MultModel, which models the area, energy metrics of a
 * specified multiplier.
 */

#ifndef __MULT_MODEL_HPP__
#define __MULT_MODEL_HPP__

#include "proto/config.pb.h"

class MultModel {
  public:
    // constructor: provide the multiplier bit width, the technology node [nm]
    MultModel(int bit_width, int tech_node=28);
    // destructor
    ~MultModel() {}

    // Area & Power Metric of the multiplier
    // Returns the Area of multiplier [um2]
    double Area() const;
    double Power() const;

  private:
    // multiplier bit width
    int bit_width_;
    // technology node
    int tech_node_;
};

#endif
