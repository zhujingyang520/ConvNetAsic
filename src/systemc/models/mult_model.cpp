/*
 * Filename: mult_model.cpp
 * -------------------------
 * This file implements the class MultModel.
 */

#include "header/systemc/models/mult_model.hpp"
#include <iostream>

using namespace std;
using namespace config;

MultModel::MultModel(int bit_width, int tech_node) {
  bit_width_ = bit_width;
  tech_node_ = tech_node;
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
    return 5020. * (static_cast<double>(bit_width_) / 16.) *
      (static_cast<double>(bit_width_) / 16.);
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

// TODO: add the power model
double MultModel::Power() const {
  return 0.;
}
