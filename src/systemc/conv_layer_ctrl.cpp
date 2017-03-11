/*
 * Filename: conv_layer_ctrl.cpp
 * ------------------------------
 * This file implements the class ConvLayerCtrl.
 */

#include "header/systemc/conv_layer_ctrl.hpp"
using namespace std;

ConvLayerCtrl::ConvLayerCtrl(sc_module_name module_name, int Kh, int Kw, int h,
    int w, int Nin, int Nout, int Pin, int Pout, int Pad_h, int Pad_w,
    int Stride_h, int Stride_w, int extra_pipeline_stage)
  : sc_module(module_name) {
  // assign the parameters to the instance variables
  Kh_ = Kh;
  Kw_ = Kw;
  h_ = h;
  w_ = w;
  Nin_ = Nin;
  Nout_ = Nout;
  Pin_ = Pin;
  Pout_ = Pout;
  Pad_h_ = Pad_h;
  Pad_w_ = Pad_w;
  Stride_h_ = Stride_h;
  Stride_w_ = Stride_w;
  extra_pipeline_stage_ = extra_pipeline_stage;

  // allocate the ports
  mult_array_in_valid = new sc_out<bool> [Pout_*Pin_];
  add_array_in_valid = new sc_out<bool> [Pout_];

  // synchronous to clock & reset
  SC_CTHREAD(ConvLayerCtrlProc, clock.pos());
  reset_signal_is(reset, true);

  SC_METHOD(MultArrayCtrlProc);
  sensitive << clock.pos() << reset;

  SC_METHOD(AddArrayCtrlProc);
  sensitive << clock.pos() << reset;

  SC_METHOD(DemuxOutRegCtrlProc);
  sensitive << clock.pos() << reset;

  SC_METHOD(LineBufferValid);
  sensitive << prev_layer_rdy << prev_layer_valid << line_buffer_zero_in;
}

ConvLayerCtrl::~ConvLayerCtrl() {
  delete [] mult_array_in_valid;
  delete [] add_array_in_valid;
}

void ConvLayerCtrl::LineBufferValid() {
  if (line_buffer_zero_in) {
    line_buffer_valid.write(1);
  } else if (prev_layer_rdy.read()) {
    line_buffer_valid.write(prev_layer_valid.read());
  } else {
    line_buffer_valid.write(0);
  }
}

void ConvLayerCtrl::ConvLayerCtrlProc() {
  // reset behavior
  prev_layer_rdy.write(0);    // not ready for the reset
  next_layer_valid.write(0);  // invalid for the next layer (not compute yet)
  // invalid & disable for all the internal units
  line_buffer_zero_in.write(0);
  line_buffer_mux_en.write(0);
  line_buffer_mux_select.write(0);
  weight_mem_rd_en.write(0);
  weight_mem_rd_addr.write(0);
  pipeline_flags_[0] = false;
  line_buffer_mux_select_ = 0;
  weight_mem_rd_addr_ = 0;
  // clear the feat_map_loc_
  feat_map_loc_ = make_pair(0, 0);
  // clear the output register
  demux_out_reg_clear.write(1);

  // calculate scheduling related parameters
  // warm up cycles: start computation until the buffer is filled
  const int warm_up_cycles = (w_+2*Pad_w_) * (Kh_-1) + Kw_-1;
  // total feature map pixels
  const int feat_pixels = (h_+2*Pad_h_) * (w_+2*Pad_w_);
  // counters
  int feat_pixel_counter = 0;

  wait();

  while (true) {
    // reset feat_pixel_counter
    if (feat_pixel_counter == feat_pixels) {
      feat_pixel_counter = 0;
    }

    if (feat_pixel_counter < Pad_h_*(w_+2*Pad_w_)) {
      // padding leading zeros: first Pad_h full 0 rows
      line_buffer_zero_in.write(1);
      ++feat_pixel_counter;
      wait();
      line_buffer_zero_in.write(0);
    }
    else if (feat_pixel_counter % (w_+2*Pad_w_) >= 0 &&
        feat_pixel_counter % (w_+2*Pad_w_) < Pad_w_) {
      // padding leading zeros: first Pad_w 0s
      line_buffer_zero_in.write(1);
      ++feat_pixel_counter;
      wait();
      line_buffer_zero_in.write(0);
    }
    else if (feat_pixel_counter % (w_+2*Pad_w_) >= w_+Pad_w_) {
      // padding tailing zeros: last Pad_w 0s
      line_buffer_zero_in.write(1);
      ++feat_pixel_counter;
      wait();
      line_buffer_zero_in.write(0);
    }
    else if (feat_pixel_counter >= (w_+2*Pad_w_)*(h_+Pad_h_) &&
        feat_pixel_counter < feat_pixels) {
      // padding tailing zeros: last Pad_h rows 0s
      line_buffer_zero_in.write(1);
      ++feat_pixel_counter;
      wait();
      line_buffer_zero_in.write(0);
    }
    else {
      // accept the data from previous layers
      prev_layer_rdy.write(1);  // indicate current layer is ready for receiving
      do {
        wait();                 // handshake property, wait for valid
      } while (!prev_layer_valid.read());

      // deassert the ready, indicate it is busy right now
      prev_layer_rdy.write(0);
      ++feat_pixel_counter;
    }

    // warm up the line buffer at first
    if (feat_pixel_counter < warm_up_cycles) {
      // do not enter the computation stage
      continue;
    }

    // go across the row of feature map
    if (feat_pixel_counter % (w_+2*Pad_w_) < Kw_ &&
        feat_pixel_counter % (w_+2*Pad_w_) > 0) {
      continue;
    }

    // stride bypass: we use the row-major line buffer
    // row index & column index in the feature map (1-indexed)
    const int row_idx = 1 + (feat_pixel_counter-1) % (w_+2*Pad_w_);
    const int col_idx = 1 + (feat_pixel_counter-1) / (w_+2*Pad_w_);
    if ((row_idx-Kw_) % Stride_w_ != 0 || (col_idx-Kh_) % Stride_h_ != 0) {
      continue;
    }

    // real computation phase
    // the scheduling first iterates over the output feature map, then over the
    // input feature map. It can reduce the memory access of line buffer
    pipeline_flags_[0] = true;
    // start accumulating the partial results in the output register
    demux_out_reg_clear.write(0);
    for (int i = 0; i < ceil(static_cast<double>(Nin_)/Pin_); ++i) {
      for (int o = 0; o < ceil(static_cast<double>(Nout_)/Pout_); ++o) {
        // activate the line buffer mux
        if (o == 0) {
          line_buffer_mux_en.write(1);
          line_buffer_mux_select.write(i);
        } else {
          // do not require to activate the line buffer mux for the remaining
          // cycle, save part of the mux power
          line_buffer_mux_en.write(0);
        }
        // activate the weight memory access
        weight_mem_rd_en.write(1);
        weight_mem_rd_addr.write(o+i*static_cast<int>(ceil(static_cast<double>
                (Nout_)/Pout_)));
        // first, second: start idx of input feature map & output feature map
        feat_map_loc_ = make_pair(i*Pin_, o*Pout_);
        wait();
      }
    }
    // deassert all the arithmetic stages
    pipeline_flags_[0] = false;
    line_buffer_mux_en.write(0);
    line_buffer_mux_select.write(0);
    weight_mem_rd_en.write(0);
    weight_mem_rd_addr.write(0);

    int drain_pipeline_ = 0;
    while ((++drain_pipeline_) < (PIPELINE_STAGE+extra_pipeline_stage_)) {
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
  } // while (true)
}

void ConvLayerCtrl::MultArrayCtrlProc() {
  if (reset.read()) {
    pipeline_flags_[1] = false;
    mult_array_en.write(0);
    for (int i = 0; i < Pout_*Pin_; ++i) {
      mult_array_in_valid[i].write(0);
    }
    // clear output feature map index
    out_feat_idx_add_ctrl_ = 0;
  } else if (pipeline_flags_[0]) {
    pipeline_flags_[1] = true;
    mult_array_en.write(1);

    // read the input & output feature map index
    const int input_feat_start_idx = feat_map_loc_.first;
    const int output_feat_start_idx = feat_map_loc_.second;
    // pipeline the output index to out_feat_idx_add_ctrl_
    out_feat_idx_add_ctrl_ = output_feat_start_idx;

    for (int o = 0; o < Pout_; ++o) {
      for (int i = 0; i < Pin_; ++i) {
        if (input_feat_start_idx + i >= Nin_ ||
            output_feat_start_idx + o >= Nout_) {
          mult_array_in_valid[o*Pin_+i].write(0);
        } else {
          mult_array_in_valid[o*Pin_+i].write(1);
        }
      }
    }
  } else {
    pipeline_flags_[1] = false;
    mult_array_en.write(0);
    for (int i = 0; i < Pout_*Pin_; ++i) {
      mult_array_in_valid[i].write(0);
    }
  }
}

void ConvLayerCtrl::AddArrayCtrlProc() {
  if (reset.read()) {
    pipeline_flags_[2] = false;
    add_array_en.write(0);
    for (int i = 0; i < Pout_; ++i) {
      add_array_in_valid[i].write(0);
    }
    // clear output feature map index
    out_feat_idx_demux_out_ctrl_ = 0;
    add_array_out_reg_select.write(0);
  } else if (pipeline_flags_[1]) {
    pipeline_flags_[2] = true;
    add_array_en.write(1);

    // read the output feature map index
    const int output_feat_start_idx = out_feat_idx_add_ctrl_;
    // pipeline the output feature map index to out_feat_idx_demux_out_ctrl_
    out_feat_idx_demux_out_ctrl_ = output_feat_start_idx;

    // write the output feature map index to select the paritial results
    add_array_out_reg_select.write(output_feat_start_idx);

    // determine the add array valid signals based on output feature map index
    for (int o = 0; o < Pout_; ++o) {
      if (output_feat_start_idx + o >= Nout_) {
        add_array_in_valid[o].write(0);
      } else {
        add_array_in_valid[o].write(1);
      }
    }
  } else {
    pipeline_flags_[2] = false;
    add_array_en.write(0);
    for (int i = 0; i < Pout_; ++i) {
      add_array_in_valid[i].write(0);
    }
    add_array_out_reg_select.write(0);
  }
}

void ConvLayerCtrl::DemuxOutRegCtrlProc() {
  if (reset.read()) {
    pipeline_flags_[3] = false;
    demux_out_reg_enable.write(0);
    demux_select.write(0);
  } else if (pipeline_flags_[2]) {
    pipeline_flags_[3] = true;
    demux_out_reg_enable.write(1);
    // read the output feature map index
    const int output_feat_start_idx = out_feat_idx_demux_out_ctrl_;
    demux_select.write(output_feat_start_idx);
  } else {
    pipeline_flags_[3] = false;
    demux_out_reg_enable.write(0);
    demux_select.write(0);
  }
}
