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

MemoryModel::MemoryModel(int memory_width,  int memory_depth, int tech_node,
    ConfigParameter_MemoryType memory_type, double clk_freq) :
  Model(tech_node, clk_freq) {
  memory_width_ = memory_width;
  memory_depth_ = memory_depth;
  memory_type_ = memory_type;

  // infer the memory size [bit]
  memory_size_ = memory_width_ * memory_depth_;
}

/*
 * Implementation notes: setters
 * ------------------------------
 * It can be used to re-configure the memory depth and memory width of the
 * memory.
 */
void MemoryModel::SetMemoryWidth(int memory_width) {
  memory_width_ = memory_width;
  // update meomory size
  memory_size_ = memory_width_ * memory_depth_;
}

void MemoryModel::SetMemoryDepth(int memory_depth) {
  memory_depth_ = memory_depth;
  // update meomory size
  memory_size_ = memory_width_ * memory_depth_;
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
    double fitted_area;
    switch(memory_type_) {
      case ConfigParameter_MemoryType_ROM:
        fitted_area = 0.378*memory_depth_ + 95.543*memory_width_ +
          0.041*memory_size_;
        return fitted_area > 0 ? fitted_area : 0;
      case ConfigParameter_MemoryType_RAM:
        fitted_area = 0.199*memory_depth_ + 165.75*memory_width_ +
          0.19*memory_size_;
        return fitted_area > 0 ? fitted_area : 0;
      default:
        cerr << "undefined memory type: " << memory_type_ << endl;
        exit(1);
    }
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

/*
 * Implementation notes: StaticPower
 * ----------------------------------
 * We use the memory model generated from the memory compiler.
 */
double MemoryModel::StaticPower() const {
  if (memory_size_ <= 0) {
    return 0.;
  }

  if (tech_node_ == 28) {
    double fitted_power;
    switch(memory_type_) {
      case ConfigParameter_MemoryType_ROM:
        fitted_power = 55.076 + 0.0456 * memory_depth_ + 1.198 * memory_width_;
        return fitted_power > 0 ? fitted_power : 0;
      case ConfigParameter_MemoryType_RAM:
        fitted_power = -68.11 + 0.017 * memory_depth_ + 5.10 * memory_width_;
        return fitted_power > 0 ? fitted_power : 0;
      default:
        cerr << "undefined memory type: " << memory_type_ << endl;
        exit(1);
    }
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

/*
 * Implementation notes: dynamic power
 * ------------------------------------
 * Dynamic power depends on the freqency: P = CV^2f. We should scale the power
 * based on the clock frequency.
 */
double MemoryModel::DynamicEnergyOfReadOperation() const {
  if (memory_size_ <= 0) {
    return 0.;
  }

  if (tech_node_ == 28) {
    double fitted_power;
    switch(memory_type_) {
      case ConfigParameter_MemoryType_ROM:
        // dynamic power is evalauted under frequency of 0.5GHz
        fitted_power = 65.82 + 0.042 * memory_depth_ + 85.22 * memory_width_;
        fitted_power = fitted_power * 2 / clk_freq_;
        return fitted_power > 0 ? fitted_power : 0;
      case ConfigParameter_MemoryType_RAM:
        fitted_power = -2.25 + 0.04 * memory_depth_ + 120.78 * memory_width_;
        fitted_power = fitted_power * 2 / clk_freq_;
        return fitted_power > 0 ? fitted_power : 0;
      default:
        cerr << "undefined memory type: " << memory_type_ << endl;
        exit(1);
    }
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}

double MemoryModel::DynamicEnergyOfWriteOperation() const {
  if (memory_size_ <= 0) {
    return 0.;
  }

  if (tech_node_ == 28) {
    double fitted_power;
    switch(memory_type_) {
      case ConfigParameter_MemoryType_ROM:
        // unexpected write to ROM
        cerr << "write operation to ROM" << endl;
        exit(1);
      case ConfigParameter_MemoryType_RAM:
        fitted_power = -89.43 + 0.043 * memory_depth_ + 133.51 * memory_width_;
        fitted_power = fitted_power * 2 / clk_freq_;
        return fitted_power > 0 ? fitted_power : 0;
      default:
        cerr << "undefined memory type: " << memory_type_ << endl;
        exit(1);
    }
  } else {
    cerr << "undefined technology node: " << tech_node_ << endl;
    exit(1);
  }
}
