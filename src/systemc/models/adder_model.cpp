/*
 * Filename: adder_model.cpp
 * --------------------------
 * This file implements the class AdderModel.
 */

#include "header/systemc/models/adder_model.hpp"
#include <iostream>

using namespace std;
using namespace config;

AdderModel::AdderModel(int bit_width, int tech_node) {
  bit_width_ = bit_width;
  tech_node_ = tech_node;
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
    return 557. * (static_cast<double>(bit_width_) / 16.);
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

// TODO: add the power model
double AdderModel::Power() const {
  return 0.;
}
