/*
 * Filename: mult_model.cpp
 * -------------------------
 * This file implements the class MultModel.
 */

#include "header/systemc/models/mult_model.hpp"
#include <iostream>

using namespace std;
using namespace config;

MultModel::MultModel(int bit_width, int tech_node) : Model(tech_node) {
  bit_width_ = bit_width;
}

/*
 * Implementation notes: Area
 * ---------------------------
 * We use TSMC library and design compiler to synthesize a 16-bit multiplier.
 * The rule of thumb of the area of a general N-bit multiplier is quadratically
 * proportional to the bit width of multiplier.
 */
double MultModel::Area() const {
  if (bit_width_ <= 0) {
    return 0.;
  }

  if (tech_node_ == 28) {
    const double fitted_area = -271.18 + 146.95 * bit_width_ +
      11.83 * bit_width_ * bit_width_;
    return fitted_area > 0 ? fitted_area : 0;
      (static_cast<double>(bit_width_) / 16.);
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

double MultModel::StaticPower() const {
  if (bit_width_ <= 0) {
    return 0.;
  }

  if (tech_node_ == 28) {
    const double fitted_power = -0.244 + 0.141 * bit_width_ +
      0.00519 * bit_width_ * bit_width_;
    return fitted_power > 0 ? fitted_power : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

double MultModel::DynamicEnergyOfOneOperation() const {
  if (bit_width_ <= 0) {
    return 0.;
  }

  if (tech_node_ == 28) {
    const double fitted_power = -66.51 + 27.11 * bit_width_ +
      8.95 * bit_width_ * bit_width_;
    return fitted_power > 0 ? fitted_power * clk_freq_ : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}
