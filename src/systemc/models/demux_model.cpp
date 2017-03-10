/*
 * Filename: demux_model.cpp
 * --------------------------
 * This file implements the class DemuxModel.
 */

#include "header/systemc/models/demux_model.hpp"
#include <iostream>
#include <math.h>

using namespace std;
using namespace config;

DemuxModel::DemuxModel(int bit_width, int num_outputs, int tech_node,
    double clk_freq) : Model(tech_node, clk_freq) {
  bit_width_ = bit_width;
  num_outputs_ = num_outputs;
}

double DemuxModel::Area() const {
  if (bit_width_ <= 0 || num_outputs_ <= 1) {
    return 0.;
  }

  const double log2_num_outputs = log2(num_outputs_);

  if (tech_node_ == 28) {
    // the N-bit Demux is the scaling of 1-input
    const double fitted_area = bit_width_ * (-126.01 + 3.26 * num_outputs_ +
        53.38 * log2_num_outputs);
    return fitted_area > 0 ? fitted_area : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

double DemuxModel::StaticPower() const {
  if (bit_width_ <= 0 || num_outputs_ <= 1) {
    return 0.;
  }

  const double log2_num_outputs = log2(num_outputs_);

  if (tech_node_ == 28) {
    const double fitted_power = bit_width_ * (-0.055 + 0.000938 * num_outputs_ +
        0.0269 * log2_num_outputs);
    return fitted_power > 0 ? fitted_power : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

double DemuxModel::DynamicEnergyOfOneOperation() const {
  if (bit_width_ <= 0 || num_outputs_ <= 1) {
    return 0.;
  }

  const double log2_num_outputs = log2(num_outputs_);

  if (tech_node_ == 28) {
    // dynamic power @ 1GHz
    const double fitted_power = bit_width_ * (-39.99 + 0.157 * num_outputs_ +
        18.927 * log2_num_outputs);
    return fitted_power > 0 ? fitted_power * clk_freq_ : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}
