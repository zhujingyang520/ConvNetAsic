/*
 * Filename: demux_out_reg.cpp
 * ----------------------------
 * This file implements the class DemuxOutReg.
 */

#include "header/systemc/demux_out_reg.hpp"
using namespace std;

DemuxOutReg::DemuxOutReg(sc_module_name module_name, int Nout, int Pout,
    int bit_width, int tech_node, double clk_freq)
  : sc_module(module_name), Nout_(Nout), Pout_(Pout) {
    // allocate the input data port
    in_data = new sc_in<Payload> [Pout_];
    // allocate the output data port
    out_data = new sc_out<Payload> [Nout_];

    // DemuxOutReg: synchronous with clock and reset
    SC_METHOD(DemuxOutRegProc);
    sensitive << clock.pos() << reset;

    // one demux model: the demux after ALU is not fully broadcast to all the
    // locations in the output registers
    const int num_outputs = static_cast<int>(ceil(static_cast<double>(Nout) /
          Pout));
    demux_model_ = new DemuxModel(bit_width, num_outputs, tech_node, clk_freq);
    dynamic_energy_ = 0.;
  }

DemuxOutReg::~DemuxOutReg() {
  delete [] in_data;
  delete [] out_data;
  delete demux_model_;
}

void DemuxOutReg::DemuxOutRegProc() {
  if (reset.read() || demux_out_reg_clear.read()) {
    for (int i = 0; i < Nout_; ++i) {
      out_data[i].write(Payload(0));
    }
  } else if (demux_out_reg_enable.read()) {
    // manage the dynamic power: infer the active demux number
    const int active_demux_num = (Nout_ - (demux_select.read() + Pout_)) >= 0
      ? Pout_ : Nout_ - demux_select.read();
    dynamic_energy_ += active_demux_num * demux_model_->
      DynamicEnergyOfOneOperation();

#ifdef DATA_PATH
    // demux the Pout input data to the corresponding location in the total Nout
    // data
    const int output_feat_start_idx = demux_select.read();
    for (int i = output_feat_start_idx; i < min(Nout_,
          output_feat_start_idx+Pout_); ++i) {
      const Payload& read_data = in_data[i-output_feat_start_idx].read();
      out_data[i].write(read_data);
    }
#endif
  }
}

double DemuxOutReg::Area() const {
  return Pout_ * demux_model_->Area();
}

double DemuxOutReg::StaticPower() const {
  return Pout_ * demux_model_->StaticPower();
}

double DemuxOutReg::DynamicPower() const {
  sc_time clock_period = dynamic_cast<const sc_clock*>(clock.get_interface())->
    period();
  sc_time sim_time = sc_time_stamp();
  double total_cycles = sim_time / clock_period;
  return dynamic_energy_ / total_cycles;
}

double DemuxOutReg::TotalPower() const {
  return StaticPower() + DynamicPower();
}
