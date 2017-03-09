/*
 * Filename: mux_model.hpp
 * ------------------------
 * This file exports the class MuxModel, which models the area, energy metrics
 * of a specified mux.
 */

#ifndef __MUX_MODEL_HPP__
#define __MUX_MODEL_HPP__

#include "proto/config.pb.h"
#include "header/systemc/models/model.hpp"

class MuxModel : public Model {
  public:
    // constructor: provide the bit width & number of inputs
    MuxModel(int bit_width, int num_inputs, int tech_node=28);
    virtual ~MuxModel() {}

    // Area & Power Metrics of the Mux
    // Return the area of mux [um2]
    virtual double Area() const;
    // Return the static power of the Mux
    virtual double StaticPower() const;
    // Dynamic energy of one operation
    double DynamicEnergyOfOneOperation() const;

  private:
    // mux input data bit width
    int bit_width_;
    // mux number of inputs
    int num_inputs_;
};

#endif
