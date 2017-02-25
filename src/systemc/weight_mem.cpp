/*
 * Filename: weight_mem.cpp
 * -------------------------
 * This file implements the WeightMem class.
 */

#include "header/systemc/weight_mem.hpp"
using namespace std;

WeightMem::WeightMem(sc_module_name module_name, int Kh, int Kw, int Pin,
    int Pout, int Nin, int Nout) : sc_module(module_name), Kh_(Kh), Kw_(Kw),
    Pin_(Pin), Pout_(Pout), Nin_(Nin), Nout_(Nout) {
    // the memory read data is of width Pout*Pin*Kh*Kw
    mem_rd_data = new sc_out<Payload>[Pout_*Pin_*Kh_*Kw_];

    // memory with
    mem_width_ = Pout_*Pin_*Kh_*Kw_;
    // memory depth can be inferred from channel depth & parallelism
    mem_depth_ = ceil(static_cast<double>(Nout_)/Pout_) *
      ceil(static_cast<double>(Nin_)/Pin_);

    // TODO: for now, we simply assume the elements in memory are initialized as
    // full ones
    for (int i = 0; i < mem_width_*mem_depth_; ++i) {
      mem_.push_back(Payload(1));
    }

    // WeightMemProc: synchronous with clock and reset
    SC_METHOD(WeightMemProc);
    sensitive << clock.pos() << reset;
  }

WeightMem::~WeightMem() {
  delete [] mem_rd_data;
  mem_.clear();
}

void WeightMem::WeightMemProc() {
  if (reset.read()) {
    // reset
    for (int i = 0; i < mem_width_; ++i) {
      mem_rd_data[i].write(Payload(0));
    }
  } else if (mem_rd_en.read()) {
#ifdef DATA_PATH
    const int addr_start_idx = mem_rd_addr.read() * mem_width_;
    for (int i = 0; i < mem_width_; ++i) {
      mem_rd_data[i].write(mem_[i+addr_start_idx]);
    }
#endif
  }
}

/*
 * Implementation notes: Area
 * ---------------------------
 * Area model of weight memory. We can specify the memory type (RAM, ROM) of the
 * current memory.
 */
double WeightMem::Area(int bit_width, int tech_node,
    config::ConfigParameter_MemoryType memory_type) const {
  // memory size: width x depth
  const int memory_size = mem_width_ * mem_depth_ * bit_width;
  MemoryModel memory_model(memory_size/1024., tech_node, memory_type);
  return memory_model.Area();
}
