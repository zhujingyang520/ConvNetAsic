/*
 * Filename: line_buffer_mux.cpp
 * ------------------------------
 * This file implements the class LineBufferMux.
 */

#include "header/systemc/line_buffer_mux.hpp"
using namespace std;

LineBufferMux::LineBufferMux(sc_module_name module_name, int Kh, int Kw,
    int Nin, int Pin, int bit_width, int tech_node, double clk_freq) :
  sc_module(module_name), Kh_(Kh), Kw_(Kw), Nin_(Nin), Pin_(Pin) {
    // input data from Nin line buffer: Nin*Kh*Kw
    line_buffer_data = new sc_in<Payload>[Nin_*Kh_*Kw_];
    // output data to Pin multiplier array
    mux_data_out = new sc_out<Payload>[Pin_*Kh_*Kw_];

    // LineBufferMux: synchronous with clock and reset
    SC_METHOD(LineBufferMuxProc);
    sensitive << clock.pos() << reset;

    // one mux model: the mux after line buffers is not fully broadcast
    // it only requires to broadcast to one of multiplier array
    const int num_inputs = ceil(static_cast<double>(Nin)/Pin);
    mux_model_ = new MuxModel(Kh*Kw*bit_width, num_inputs, tech_node, clk_freq);
    dynamic_energy_ = 0.;
  }

LineBufferMux::~LineBufferMux() {
  delete [] line_buffer_data;
  delete [] mux_data_out;
  delete mux_model_;
}

void LineBufferMux::LineBufferMuxProc() {
  if (reset.read()) {
    // reset, simply output full 0s
    for (int i = 0; i < Pin_*Kh_*Kw_; ++i) {
      mux_data_out[i].write(Payload(0));
    }
  } else if (mux_en.read()) {
    // manage the dynamic power: infer the active mux number
    // for a specified select i, the input range covers [i*Pin, (i+1)*Pin-1]
    const int active_mux_num = (Nin_ - (mux_select.read()+1) * Pin_) >= 0
      ? Pin_ : Nin_ - mux_select.read() * Pin_;
    dynamic_energy_ += active_mux_num *
      mux_model_->DynamicEnergyOfOneOperation();
#ifdef DATA_PATH
    // mux is enabled
    const int mux_select_max = ceil(static_cast<double>(Nin_)/Pin_);
    // sanity check the select signals
    assert(mux_select.read() >= 0 && mux_select.read() < mux_select_max);
    const int line_buffer_start_idx = mux_select.read()*Pin_*Kh_*Kw_;
    for (int i = 0; i < Pin_; ++i) {
      for (int m = 0; m < Kh_; ++m) {
        for (int n = 0; n < Kw_; ++n) {
          // read index referred to the line buffer input
          const int read_idx = line_buffer_start_idx + i*Kh_*Kw_+m*Kw_+n;
          if (read_idx < Nin_*Kh_*Kw_) {
            mux_data_out[i*Kh_*Kw_+m*Kw_+n].write(
                line_buffer_data[read_idx].read());
          } else {
            // out-of range due to non-integer case
            mux_data_out[i*Kh_*Kw_+m*Kw_+n].write(Payload(0));
          }
        }
      }
    }
#endif
  }
}

double LineBufferMux::Area() const {
  return Pin_ * mux_model_->Area();
}

double LineBufferMux::StaticPower() const {
  return Pin_ * mux_model_->StaticPower();
}

double LineBufferMux::DynamicPower() const {
  sc_time clock_period = dynamic_cast<const sc_clock*>(clock.get_interface())->
    period();
  sc_time sim_time = sc_time_stamp();
  int total_cycles = sim_time / clock_period;
  return dynamic_energy_ / total_cycles;
}

double LineBufferMux::TotalPower() const {
  return StaticPower() + DynamicPower();
}
