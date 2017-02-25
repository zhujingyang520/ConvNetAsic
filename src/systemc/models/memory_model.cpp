/*
 * Filename: memory_model.cpp
 * ---------------------------
 * This file implements the MemoryModel. The area and power metrics are fitted
 * from the TSMC memory compiler.
 */

#include "header/systemc/models/memory_model.hpp"
#include <iostream>

using namespace std;
using namespace config;

MemoryModel::MemoryModel(double memory_size, int tech_node,
    ConfigParameter_MemoryType memory_type) {
  memory_size_ = memory_size;
  tech_node_ = tech_node;
  memory_type_ = memory_type;
}

/*
 * Implementation notes: Area
 * ---------------------------
 * We use the linear regression to mode the data exported from the TSMC memory 
 * compiler under a wide range of memory sizes.
 */
double MemoryModel::Area() const {
  if (memory_size_ <= 0) {
    return 0;
  }
  if (tech_node_ == 28) {
    switch(memory_type_) {
      case ConfigParameter_MemoryType_ROM:
        return 62. * memory_size_ + 1578.;
      case ConfigParameter_MemoryType_RAM:
        return 211. * memory_size_ + 3056.;
      default:
        cerr << "undefined memory type: " << memory_type_ << endl;
        exit(1);
    }
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

// TODO: add the power value
double MemoryModel::Power() const {
  return 0.;
}
