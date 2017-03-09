/*
 * Filename: pool_layer_pe.cpp
 * ----------------------------
 * This file implements the class PoolLayerPe.
 */

#include "header/systemc/pool_layer_pe.hpp"
using namespace std;

/*
 * Implementation notes: Constructor
 * ----------------------------------
 * The constructor initializes the modules in the pooling layer processing
 * elements, and makes the connections of each module.
 */
PoolLayerPe::PoolLayerPe(sc_module_name module_name, int Kh, int Kw, int h,
    int w, int Nin, int Pin, int Pad_h, int Pad_w, int Stride_h, int Stride_w,
    PoolArray::PoolMethod pool_method, int bit_width, int tech_node)
  : sc_module(module_name), Nin_(Nin), Pin_(Pin) {
  // pooling layer: same input & output channel depth / parallelism
  const int Nout = Nin;
  const int Pout = Pin;
  // allocate the ports and interconnections
  prev_layer_data = new sc_in<Payload> [Nin];
  next_layer_data = new sc_out<Payload> [Nout];
  line_buffer_in_data_ = new sc_signal<Payload> [Nin];
  line_buffer_out_data_ = new sc_signal<Payload> [Nin*Kh*Kw];
  line_buffer_mux_out_data_ = new sc_signal<Payload> [Pin*Kh*Kw];
  pool_array_in_valid_ = new sc_signal<bool> [Pin];
  pool_array_out_data_ = new sc_signal<Payload> [Pout];

  char name[100];
  // initialize the FSM controller
  sprintf(name, "%s", "controller");
  pool_layer_ctrl_ = new PoolLayerCtrl(name, Kh, Kw, h, w, Nin, Pin, Pad_h,
      Pad_w, Stride_h, Stride_w);
  pool_layer_ctrl_->clock(clock);
  pool_layer_ctrl_->reset(reset);
  pool_layer_ctrl_->prev_layer_valid(prev_layer_valid);
  pool_layer_ctrl_->prev_layer_rdy(prev_layer_rdy);
  pool_layer_ctrl_->next_layer_rdy(next_layer_rdy);
  pool_layer_ctrl_->next_layer_valid(next_layer_valid);
  pool_layer_ctrl_->line_buffer_valid(line_buffer_valid_);
  pool_layer_ctrl_->line_buffer_zero_in(line_buffer_zero_in_);
  pool_layer_ctrl_->line_buffer_mux_en(line_buffer_mux_en_);
  pool_layer_ctrl_->line_buffer_mux_select(line_buffer_mux_select_);
  pool_layer_ctrl_->pool_array_en(pool_array_en_);
  for (int i = 0; i < Pin; ++i) {
    pool_layer_ctrl_->pool_array_in_valid[i](pool_array_in_valid_[i]);
  }
  pool_layer_ctrl_->demux_out_reg_clear(demux_out_reg_clear_);
  pool_layer_ctrl_->demux_out_reg_enable(demux_out_reg_enable_);
  pool_layer_ctrl_->demux_select(demux_select_);

  // initialize the line buffer arrays
  line_buffer_array_ = new LineBufferArray("line_buffer_array", Kh, Kw,
      h+2*Pad_h, w+2*Pad_w, Nin, bit_width, tech_node);
  line_buffer_array_->clock(clock);
  line_buffer_array_->reset(reset);
  line_buffer_array_->input_data_valid(line_buffer_valid_);
  for (int i = 0; i < Nin; ++i) {
    line_buffer_array_->input_data[i](line_buffer_in_data_[i]);
  }
  for (int i = 0; i < Nin*Kh*Kw; ++i) {
    line_buffer_array_->output_data[i](line_buffer_out_data_[i]);
  }

  // initialize the line buffer mux
  sprintf(name, "%s", "line_buffer_mux");
  line_buffer_mux_ = new LineBufferMux(name, Kh, Kw, Nin, Pin, bit_width,
      tech_node);
  line_buffer_mux_->clock(clock);
  line_buffer_mux_->reset(reset);
  line_buffer_mux_->mux_en(line_buffer_mux_en_);
  for (int i = 0; i < Nin*Kh*Kw; ++i) {
    line_buffer_mux_->line_buffer_data[i](line_buffer_out_data_[i]);
  }
  line_buffer_mux_->mux_select(line_buffer_mux_select_);
  for (int i = 0; i < Pin*Kh*Kw; ++i) {
    line_buffer_mux_->mux_data_out[i](line_buffer_mux_out_data_[i]);
  }

  // initialize the pooling array (max/avg)
  sprintf(name, "%s", "pool_array");
  pool_array_ = new PoolArray(name, Kh, Kw, Pin, pool_method, bit_width,
      tech_node);
  pool_array_->clock(clock);
  pool_array_->reset(reset);
  pool_array_->pool_array_en(pool_array_en_);
  for (int i = 0; i < Pin; ++i) {
    pool_array_->pool_array_in_valid[i](pool_array_in_valid_[i]);
  }
  for (int i = 0; i < Pin*Kh*Kw; ++i) {
    pool_array_->pool_array_in_data[i](line_buffer_mux_out_data_[i]);
  }
  for (int i = 0; i < Pout; ++i) {
    pool_array_->pool_array_out_data[i](pool_array_out_data_[i]);
  }

  // initialize the demux output register
  sprintf(name, "%s", "demux_out_reg");
  demux_out_reg_ = new DemuxOutReg(name, Nout, Pout, bit_width, tech_node);
  demux_out_reg_->clock(clock);
  demux_out_reg_->reset(reset);
  demux_out_reg_->demux_out_reg_clear(demux_out_reg_clear_);
  demux_out_reg_->demux_out_reg_enable(demux_out_reg_enable_);
  demux_out_reg_->demux_select(demux_select_);
  for (int i = 0; i < Pout; ++i) {
    demux_out_reg_->in_data[i](pool_array_out_data_[i]);
  }
  for (int i = 0; i < Nout; ++i) {
    demux_out_reg_->out_data[i](next_layer_data[i]);
  }

  // process dealing with line buffer zero padding
  SC_METHOD(LineBufferInMux);
  sensitive << line_buffer_zero_in_;
  for (int i = 0; i < Nin; ++i) {
    sensitive << prev_layer_data[i];
  }
}

void PoolLayerPe::LineBufferInMux() {
  for (int i = 0; i < Nin_; ++i) {
    if (line_buffer_zero_in_.read()) {
      line_buffer_in_data_[i] = Payload(0);
    } else {
      line_buffer_in_data_[i] = prev_layer_data[i];
    }
  }
}

PoolLayerPe::~PoolLayerPe() {
  delete [] prev_layer_data;
  delete [] next_layer_data;
  delete [] line_buffer_in_data_;
  delete [] line_buffer_out_data_;
  delete [] line_buffer_mux_out_data_;
  delete [] pool_array_in_valid_;
  delete [] pool_array_out_data_;

  delete pool_layer_ctrl_;
  delete line_buffer_array_;
  delete line_buffer_mux_;
  delete pool_array_;
  delete demux_out_reg_;
}

/*
 * Implementation notes: Area
 * ---------------------------
 * Accumulate all the components within the pool layer.
 */
double PoolLayerPe::Area() const {
  double total_area = 0.;
  // line buffer area: centralized implementation
  // where the width is concatenation over all channels
  total_area += line_buffer_array_->Area();
  // line buffer mux area
  total_area += line_buffer_mux_->Area();
  // pool array
  total_area += pool_array_->Area();
  // demux reg
  total_area += demux_out_reg_->Area();

  return total_area;
}

double PoolLayerPe::StaticPower() const {
  double total_power = 0.;
  // accumulate the static power of all components
  total_power += line_buffer_array_->StaticPower();
  total_power += line_buffer_mux_->StaticPower();
  total_power += pool_array_->StaticPower();
  total_power += demux_out_reg_->StaticPower();
  return total_power;
}

double PoolLayerPe::DynamicPower() const {
  double total_power = 0.;
  // accumulate the dynamic power of all components
  total_power += line_buffer_array_->DynamicPower();
  total_power += line_buffer_mux_->DynamicPower();
  total_power += pool_array_->DynamicPower();
  total_power += demux_out_reg_->DynamicPower();
  return total_power;
}

double PoolLayerPe::TotalPower() const {
  return StaticPower() + DynamicPower();
}
