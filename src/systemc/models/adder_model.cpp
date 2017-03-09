/*
 * Filename: adder_model.cpp
 * --------------------------
 * This file implements the class AdderModel.
 */

#include "header/systemc/models/adder_model.hpp"
#include <iostream>

using namespace std;
using namespace config;

AdderModel::AdderModel(int bit_width, int tech_node) : Model(tech_node) {
  bit_width_ = bit_width;
}

/*
 * Implementation notes: Area
 * ---------------------------
 * We use TSMC library and design compiler to synthesize a 16-bit adder. The
 * rule of thumb of the area of a general N-bit adder is linearly proportional
 * to the bit width of the adder.
 */
double AdderModel::Area() const {
  if (bit_width_ <= 0) {
    return 0.;
  }

  if (tech_node_ == 28) {
    const double fitted_area = -50.07 + 37.86 * bit_width_;
    return fitted_area > 0 ? fitted_area : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

/*
 * Implementation notes: StaticPower
 * ----------------------------------
 * We use the synthesized N-bit adder to generate the static power consumption
 * of N-bit adder.
 */
double AdderModel::StaticPower() const {
  if (bit_width_ <= 0) {
    return 0.;
  }

  if (tech_node_ == 28) {
    // static power is fitted from the linear regression of the bit width of
    // operands
    const double fitted_power = -0.012 + 0.024 * bit_width_;
    return fitted_power > 0 ? fitted_power : 0;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

double AdderModel::DynamicEnergyOfOneOperation() const {
  if (bit_width_ <= 0) {
    return 0.;
  }

  if (tech_node_ == 28) {
    // dynamic power is fitted from the linear regression of the bit width of
    // operands @ 1GHz
    const double fitted_power = -14.62 + 14.48 * bit_width_;
    return fitted_power > 0 ? fitted_power * clk_freq_ : 0;
    return 0.;
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}
