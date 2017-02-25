/*
 * Filename: weight_mem.hpp
 * -------------------------
 * This file exports the class WeightMem, which mimics the access pattern of
 * weight memory.
 */

#ifndef __WEIGHT_MEM_HPP__
#define __WEIGHT_MEM_HPP__

#include "header/systemc/data_type.hpp"
#include "header/systemc/models/memory_model.hpp"
#include "proto/config.pb.h"
#include <systemc.h>
#include <vector>

class WeightMem : public sc_module {
  public:
    sc_in<bool> clock;
    sc_in<bool> reset;
    // memory read enable
    sc_in<bool> mem_rd_en;
    // memory read address
    sc_in<int> mem_rd_addr;
    // memory read data
    sc_out<Payload>* mem_rd_data;

    SC_HAS_PROCESS(WeightMem);

    // area model of weight memory
    double Area(int bit_width, int tech_node,
        config::ConfigParameter_MemoryType memory_type) const;

  public:
    // constructor
    explicit WeightMem(sc_module_name module_name, int Kh, int Kw, int Pin,
        int Pout, int Nin, int Nout);
    // destructor
    ~WeightMem();

    // main process of the weight memory
    void WeightMemProc();

  private:
    int Kh_, Kw_;     // spatial dimension of the kernel
    int Pin_, Pout_;  // input parallelism & output parallelism
    int Nin_, Nout_;  // input & output feature map number (channel depth)
    int mem_width_;   // inferred memory depth & width
    int mem_depth_;

    // real memory storage
    std::vector<Payload> mem_;
};

#endif
