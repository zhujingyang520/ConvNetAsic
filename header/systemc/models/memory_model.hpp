/*
 * Filename: memory_model.hpp
 * ---------------------------
 * This file exports the MemoryModel, which models the area, energy metrics of a
 * specified memory type and memory size.
 */

#ifndef __MEMORY_MODEL_HPP__
#define __MEMORY_MODEL_HPP__

#include "proto/config.pb.h"

class MemoryModel {
  public:
    // constructor: the memory size is provided in [kbit], the technology node
    // is in [nm], the memory type is in {ROM, RAM}
    MemoryModel(double memory_size, int tech_node=28,
        config::ConfigParameter_MemoryType memory_type=config::
        ConfigParameter_MemoryType_ROM);
    // destructor
    ~MemoryModel() {}

    // Area & Power Metric of Memory Module
    // Returns the Area of the memory [um2]
    double Area() const;
    double Power() const;

  private:
    // memory size
    double memory_size_;
    // technology node
    int tech_node_;
    // memory type {ROM, RAM}
    config::ConfigParameter_MemoryType memory_type_;
};

#endif
