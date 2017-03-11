/*
 * Filename: weight_mem.cpp
 * -------------------------
 * This file implements the WeightMem class.
 */

#include "header/systemc/weight_mem.hpp"
using namespace std;
using namespace config;

WeightMem::WeightMem(sc_module_name module_name, int Kh, int Kw, int Pin,
    int Pout, int Nin, int Nout, ConfigParameter_MemoryType memory_type,
    int bit_width, int tech_node, double clk_freq) :
  sc_module(module_name), Kh_(Kh), Kw_(Kw), Pin_(Pin), Pout_(Pout), Nin_(Nin),
  Nout_(Nout) {
    // the memory read data is of width Pout*Pin*Kh*Kw
    mem_rd_data = new sc_out<Payload>[Pout_*Pin_*Kh_*Kw_];

    // memory with
    mem_width_ = Pout_*Pin_*Kh_*Kw_;
    // memory depth can be inferred from channel depth & parallelism
    mem_depth_ = static_cast<int>(ceil(static_cast<double>(Nout_)/Pout_) *
      ceil(static_cast<double>(Nin_)/Pin_));

    // TODO: for now, we simply assume the elements in memory are initialized as
    // full ones
    for (int i = 0; i < mem_width_*mem_depth_; ++i) {
      mem_.push_back(Payload(1));
    }

    // WeightMemProc: synchronous with clock and reset
    SC_METHOD(WeightMemProc);
    sensitive << clock.pos() << reset;

    // allocate the memory model
    const int memory_width = mem_width_ * bit_width;
    const int memory_depth = mem_depth_;
    memory_model_ = new MemoryModel(memory_width, memory_depth, tech_node,
        memory_type, clk_freq);
    dynamic_energy_ = 0.;
  }

WeightMem::~WeightMem() {
  delete [] mem_rd_data;
  mem_.clear();
  delete memory_model_;
}

void WeightMem::WeightMemProc() {
  if (reset.read()) {
    // reset
    for (int i = 0; i < mem_width_; ++i) {
      mem_rd_data[i].write(Payload(0));
    }
  } else if (mem_rd_en.read()) {
    // update the dynamic energy for one read operation
    dynamic_energy_ += memory_model_->DynamicEnergyOfReadOperation();
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
double WeightMem::Area() const {
  return memory_model_->Area();
}

double WeightMem::StaticPower() const {
  return memory_model_->StaticPower();
}

double WeightMem::DynamicPower() const {
  sc_time clock_period = dynamic_cast<const sc_clock*>(clock.get_interface())->
    period();
  sc_time sim_time = sc_time_stamp();
  double total_cycles = sim_time / clock_period;
  return dynamic_energy_ / total_cycles;
}

double WeightMem::TotalPower() const {
  return StaticPower() + DynamicPower();
}
