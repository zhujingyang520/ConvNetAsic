/*
 * Filename: pool_layer_ctrl.cpp
 * ------------------------------
 * This file implements the class PoolLayerCtrl.
 */

#include "header/systemc/pool_layer_ctrl.hpp"
using namespace std;

PoolLayerCtrl::PoolLayerCtrl(sc_module_name module_name, int Kh, int Kw, int h,
    int w, int Nin, int Pin, int Pad_h, int Pad_w, int Stride_h,
    int Stride_w)
  : sc_module(module_name) {
  // assign the parameters to the instance variables
  Kh_ = Kh;
  Kw_ = Kw;
  h_ = h;
  w_ = w;
  Nin_ = Nin;
  Pin_ = Pin;
  Pad_h_ = Pad_h;
  Pad_w_ = Pad_w;
  Stride_h_ = Stride_h;
  Stride_w_ = Stride_w;

  // allocate the ports
  pool_array_in_valid = new sc_out<bool> [Pin_];

  // synchronous to clock & reset
  SC_CTHREAD(PoolLayerCtrlProc, clock.pos());
  reset_signal_is(reset, true);

  SC_METHOD(PoolArrayCtrlProc);
  sensitive << clock.pos() << reset;

  SC_METHOD(DemuxOutRegCtrlProc);
  sensitive << clock.pos() << reset;
}

PoolLayerCtrl::~PoolLayerCtrl() {
  //delete [] pool_array_in_valid;
}

void PoolLayerCtrl::PoolLayerCtrlProc() {
  // reset behavior
  prev_layer_rdy.write(0);    // not ready for the previous layer
  next_layer_valid.write(0);  // invalid for the next layer
  // disable & invalid for all the internal units
  line_buffer_valid.write(0);
  line_buffer_zero_in.write(0);
  line_buffer_mux_en.write(0);
  line_buffer_mux_select.write(0);
  pipeline_flags_[0] = false;
  in_feat_idx_pool_ctrl_ = 0;
  demux_out_reg_clear.write(1);

  // calculate scheduling related parameters
  // warm up cycles
  const int warm_up_cycles = (w_+2*Pad_w_) * (Kh_-1) + Kw_ - 1;
  // total feature map pixels
  const int feat_pixels = (h_ + 2*Pad_h_) * (w_ + 2*Pad_w_);
  // feature map received counter
  int feat_pixel_counter = 0;

  wait();

  while (true) {
    // reset feat_pixel_counter
    if (feat_pixel_counter == feat_pixels) {
      feat_pixel_counter = 0;
    }

    // pad zero or receive the data from primary input
    if (feat_pixel_counter < Pad_h_*(w_+2*Pad_w_)) {
      // padding leading zeros: first Pad_h rows
      line_buffer_valid.write(1);
      line_buffer_zero_in.write(1);
      ++feat_pixel_counter;
      wait();
      line_buffer_valid.write(0);
      line_buffer_zero_in.write(0);
    }
    else if (feat_pixel_counter % (w_+2*Pad_w_) >= 0 &&
        feat_pixel_counter % (w_+2*Pad_w_) < Pad_w_) {
      // padding leading zeros: first Pad_w 0s
      line_buffer_valid.write(1);
      line_buffer_zero_in.write(1);
      ++feat_pixel_counter;
      wait();
      line_buffer_valid.write(0);
      line_buffer_zero_in.write(0);
    }
    else if (feat_pixel_counter % (w_+2*Pad_w_) >= w_+Pad_w_) {
      // padding tailing zeros: last Pad_w 0s
      line_buffer_valid.write(1);
      line_buffer_zero_in.write(1);
      ++feat_pixel_counter;
      wait();
      line_buffer_valid.write(0);
      line_buffer_zero_in.write(0);
    }
    else if (feat_pixel_counter >= (w_+2*Pad_w_)*(h_+Pad_h_) &&
        feat_pixel_counter < feat_pixels) {
      // padding tailing zeros: last Pad_h rows 0s
      line_buffer_valid.write(1);
      line_buffer_zero_in.write(1);
      ++feat_pixel_counter;
      wait();
      line_buffer_valid.write(0);
      line_buffer_zero_in.write(0);
    }
    else {
      // accept data from the previous layer
      prev_layer_rdy.write(1);
      do {
        wait();
      } while (!prev_layer_valid.read());
      // deassert the ready
      prev_layer_rdy.write(0);
      line_buffer_valid.write(1);
      ++feat_pixel_counter;
      wait();
      line_buffer_valid.write(0);
    }

    // warm up cycles
    if (feat_pixel_counter < warm_up_cycles) {
      continue;
    }

    // go across the row of feature map
    if (feat_pixel_counter % (w_+2*Pad_w_) < Kw_ &&
        feat_pixel_counter % (w_+2*Pad_w_) > 0) {
      continue;
    }

    // stride bypass
    const int row_idx = 1 + (feat_pixel_counter-1) % (w_+2*Pad_w_);
    const int col_idx = 1 + (feat_pixel_counter-1) / (w_+2*Pad_w_);
    if ((row_idx-Kw_) % Stride_w_ != 0 || (col_idx-Kh_) % Stride_h_ != 0) {
      continue;
    }

    // real computation here
    // the scheduling just iterates through all the input feature maps
    pipeline_flags_[0] = true;
    // disable the output write register clear
    demux_out_reg_clear.write(0);
    for (int i = 0; i < ceil(static_cast<double>(Nin_)/Pin_); ++i) {
      // activate the line buffer
      line_buffer_mux_en.write(1);
      line_buffer_mux_select.write(i);
      // store the feature map index to the instance variable
      in_feat_idx_pool_ctrl_ = i*Pin_;
      wait();
    }
    // deassert all the arithmetic stages
    pipeline_flags_[0] = false;
    line_buffer_mux_en.write(0);
    line_buffer_mux_select.write(0);

    // drain the calculation
    int drain_pipeline_ = 0;
    while ((++drain_pipeline_) < PIPELINE_STAGE) {
      wait();
    }

    // handshake protocol for next stage is ready to receive data
    next_layer_valid.write(1);
    do {
      wait();
    } while (!next_layer_rdy.read());
    next_layer_valid.write(0);

    // clear the output register after successful sending out 1 pixel
    demux_out_reg_clear.write(1);
  }   // while (true)
}

void PoolLayerCtrl::PoolArrayCtrlProc() {
  if (reset.read()) {
    pipeline_flags_[1] = false;
    pool_array_en.write(0);
    for (int i = 0; i < Pin_; ++i) {
      pool_array_in_valid[i].write(0);
    }
    in_feat_idx_demux_out_ctrl_ = 0;
  } else if (pipeline_flags_[0]) {
    pipeline_flags_[1] = true;
    pool_array_en.write(1);

    // read the input feature map index
    const int input_feat_start_idx = in_feat_idx_pool_ctrl_;
    // pipeline the input feature map index to in_feat_idx_demux_out_ctrl_
    in_feat_idx_demux_out_ctrl_ = input_feat_start_idx;

    for (int i = 0; i < Pin_; ++i) {
      if (input_feat_start_idx + i >= Nin_) {
        pool_array_in_valid[i].write(0);
      } else {
        pool_array_in_valid[i].write(1);
      }
    }
  } else {
    pipeline_flags_[1] = false;
    pool_array_en.write(0);
    for (int i = 0; i < Pin_; ++i) {
      pool_array_in_valid[i].write(0);
    }
    in_feat_idx_demux_out_ctrl_ = 0;
  }
}

void PoolLayerCtrl::DemuxOutRegCtrlProc() {
  if (reset.read()) {
    pipeline_flags_[2] = false;
    demux_out_reg_enable.write(0);
    demux_select.write(0);
  } else if (pipeline_flags_[1]) {
    pipeline_flags_[2] = true;
    demux_out_reg_enable.write(1);
    // read the input feature map index
    const int input_feat_start_idx = in_feat_idx_demux_out_ctrl_;
    demux_select.write(input_feat_start_idx);
  } else {
    pipeline_flags_[2] = false;
    demux_out_reg_enable.write(0);
    demux_select.write(0);
  }
}
