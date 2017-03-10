/*
 * Filename: mux_model.cpp
 * ------------------------
 * This file implements the class MuxModel.
 */

#include "header/systemc/models/mux_model.hpp"
#include <iostream>

using namespace std;
using namespace config;

MuxModel::MuxModel(int bit_width, int num_inputs, int tech_node,
    double clk_freq) : Model(tech_node, clk_freq) {
  bit_width_ = bit_width;
  num_inputs_ = num_inputs;
}

double MuxModel::Area() const {
  if (bit_width_ <= 0 || num_inputs_ <= 1) {
    return 0.;
  }

  if (tech_node_ == 28) {
    // the N-bit MUX is just the linear scaling of 1-input
    const double fitted_area = bit_width_ * (34.68 + 5.25 * num_inputs_);
    return fitted_area > 0 ? fitted_area : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

double MuxModel::StaticPower() const {
  if (bit_width_ <= 0 || num_inputs_ <= 1) {
    return 0.;
  }

  if (tech_node_ == 28) {
    const double fitted_power = bit_width_ * (0.0588 + 0.0025 * num_inputs_);
    return fitted_power > 0 ? fitted_power : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

double MuxModel::DynamicEnergyOfOneOperation() const {
  if (bit_width_ <= 0 || num_inputs_ <= 1) {
    return 0.;
  }

  if (tech_node_ == 28) {
    // dynamic powre @ 1GHz
    const double fitted_power = bit_width_ * (21.84 + 1.14 * num_inputs_);
    return fitted_power > 0 ? fitted_power * clk_freq_ : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}
