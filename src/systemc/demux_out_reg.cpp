/*
 * Filename: demux_out_reg.cpp
 * ----------------------------
 * This file implements the class DemuxOutReg.
 */

#include "header/systemc/demux_out_reg.hpp"
using namespace std;

DemuxOutReg::DemuxOutReg(sc_module_name module_name, int Nout, int Pout)
  : sc_module(module_name), Nout_(Nout), Pout_(Pout) {
    // allocate the input data port
    in_data = new sc_in<Payload> [Pout_];
    // allocate the output data port
    out_data = new sc_out<Payload> [Nout_];

    // DemuxOutReg: synchronous with clock and reset
    SC_METHOD(DemuxOutRegProc);
    sensitive << clock.pos() << reset;
  }

DemuxOutReg::~DemuxOutReg() {
  delete [] in_data;
  delete [] out_data;
}

void DemuxOutReg::DemuxOutRegProc() {
  if (reset.read() || demux_out_reg_clear.read()) {
    for (int i = 0; i < Nout_; ++i) {
      out_data[i].write(Payload(0));
    }
  } else if (demux_out_reg_enable.read()) {
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
