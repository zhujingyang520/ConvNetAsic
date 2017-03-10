/*
 * Filename: comparator_model.cpp
 * -------------------------------
 * This file implements the class ComparatorModel.
 */

#include "header/systemc/models/comparator_model.hpp"
#include <iostream>

using namespace std;
using namespace config;

ComparatorModel::ComparatorModel(int bit_width, int tech_node, double clk_freq)
  : Model(tech_node, clk_freq) {
    bit_width_ = bit_width;
  }

double ComparatorModel::Area() const {
  if (bit_width_ <= 0) {
    return 0.;
  }

  if (tech_node_ == 28) {
    const double fitted_area = -10.45 + 13.62 * bit_width_;
    return fitted_area > 0 ? fitted_area : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

double ComparatorModel::StaticPower() const {
  if (bit_width_ <= 0) {
    return 0.;
  }

  if (tech_node_ == 28) {
    const double fitted_power = -0.0026 + 0.0087 * bit_width_;
    return fitted_power > 0 ? fitted_power : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

double ComparatorModel::DynamicEnergyOfOneOperation() const {
  if (bit_width_ <= 0) {
    return 0.;
  }

  if (tech_node_ == 28) {
    const double fitted_power = -2.96 + 4.33 * bit_width_;
    return fitted_power > 0 ? fitted_power : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}
