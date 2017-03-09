/*
 * Filename: memory_model.hpp
 * ---------------------------
 * This file exports the MemoryModel, which models the area, energy metrics of a
 * specified memory type and memory size.
 */

#ifndef __MEMORY_MODEL_HPP__
#define __MEMORY_MODEL_HPP__

#include "proto/config.pb.h"
#include "header/systemc/models/model.hpp"

class MemoryModel : public Model {
  public:
    // constructor: specify the memory width [bit], memory depth [no.], the
    // technology node [nm], the memory type is in {ROM, RAM}
    MemoryModel(int memory_width, int memory_depth, int tech_node=28,
        config::ConfigParameter_MemoryType memory_type=config::
        ConfigParameter_MemoryType_ROM);
    // destructor
    virtual ~MemoryModel() {}

    // setters of memory width and depth
    void SetMemoryWidth(int memory_width);
    void SetMemoryDepth(int memory_depth);
    // getters of memory width and depth
    inline int memory_width() const { return memory_width_; }
    inline int memory_depth() const { return memory_depth_; }

    // Area & Power Metric of the memory
    // Returns the Area of the memory [um2]
    virtual double Area() const;
    // Returns the static power consumption of the memory
    virtual double StaticPower() const;
    // Dynamic energy of read operation
    double DynamicEnergyOfReadOperation() const;
    // Dynamic energy of write operation
    double DynamicEnergyOfWriteOperation() const;

  private:
    // memory size
    int memory_width_;
    int memory_depth_;
    int memory_size_;
    // memory type {ROM, RAM}
    config::ConfigParameter_MemoryType memory_type_;
};

#endif
