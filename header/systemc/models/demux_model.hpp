/*
 * Filename: demux_model.hpp
 * --------------------------
 * This file exports the class DemuxModel, which models the area, energy metrics
 * of a specified demux.
 */

#ifndef __DEMUX_MODEL_HPP__
#define __DEMUX_MODEL_HPP__

#include "proto/config.pb.h"
#include "header/systemc/models/model.hpp"

class DemuxModel : public Model {
  public:
    // constructor: provide the bit width & number of outputs
    DemuxModel(int bit_width, int num_outputs, int tech_node=28);
    virtual ~DemuxModel() {}

    // Area & Power Metrics of the Demux
    // Return the area of demux [um2]
    virtual double Area() const;
    // Return the static power of the Demux
    virtual double StaticPower() const;
    // Dynamic energy of one operation
    double DynamicEnergyOfOneOperation() const;

  private:
    // demux input data bit width
    int bit_width_;
    // demux output number
    int num_outputs_;
};

#endif
