/*
 * Filename: conv_layer_pe.cpp
 * ----------------------------
 * This file implements the class ConvLayerPe.
 */

#include "header/systemc/conv_layer_pe.hpp"
using namespace std;

/*
 * Implementation notes: Constructor
 * ----------------------------------
 * The constructor mainly initializes all the required modules in the
 * convolution layer Processing Element, and makes the connections of each
 * module.
 */
ConvLayerPe::ConvLayerPe(sc_module_name module_name, int Kh, int Kw, int h,
    int w, int Nin, int Nout, int Pin, int Pout, int Pad_h, int Pad_w,
    int Stride_h, int Stride_w)
  : sc_module(module_name), Nin_(Nin), Nout_(Nout), Pout_(Pout), Pin_(Pin) {
  // allocate the ports and interconnections
  prev_layer_data = new sc_in<Payload> [Nin];
  next_layer_data = new sc_out<Payload> [Nout];
  line_buffer_in_data_ = new sc_signal<Payload> [Nin];
  line_buffer_out_data_ = new sc_signal<Payload> [Nin*Kh*Kw];
  line_buffer_mux_out_data_ = new sc_signal<Payload> [Pin*Kh*Kw];
  weight_mem_rd_data_ = new sc_signal<Payload> [Pout*Pin*Kh*Kw];
  mult_array_in_valid_ = new sc_signal<bool> [Pout*Pin];
  mult_array_out_data_ = new sc_signal<Payload> [Pout*Pin*Kh*Kw];
  add_array_in_valid_ = new sc_signal<bool> [Pout];
  add_array_reg_in_data_ = new sc_signal<Payload> [Pout];
  add_array_out_data_ = new sc_signal<Payload> [Pout];
  out_reg_data_ = new sc_signal<Payload> [Nout];

  char name[100];
  // initialize the FSM controller
  sprintf(name, "%s", "controller");
  conv_layer_ctrl_ = new ConvLayerCtrl(name, Kh, Kw, h, w, Nin, Nout, Pin,
      Pout, Pad_h, Pad_w, Stride_h, Stride_w);
  conv_layer_ctrl_->clock(clock);
  conv_layer_ctrl_->reset(reset);
  conv_layer_ctrl_->prev_layer_valid(prev_layer_valid);
  conv_layer_ctrl_->prev_layer_rdy(prev_layer_rdy);
  conv_layer_ctrl_->next_layer_rdy(next_layer_rdy);
  conv_layer_ctrl_->next_layer_valid(next_layer_valid);
  conv_layer_ctrl_->line_buffer_valid(line_buffer_valid_);
  conv_layer_ctrl_->line_buffer_zero_in(line_buffer_zero_in_);
  conv_layer_ctrl_->line_buffer_mux_en(line_buffer_mux_en_);
  conv_layer_ctrl_->line_buffer_mux_select(line_buffer_mux_select_);
  conv_layer_ctrl_->weight_mem_rd_en(weight_mem_rd_en_);
  conv_layer_ctrl_->weight_mem_rd_addr(weight_mem_rd_addr_);
  conv_layer_ctrl_->mult_array_en(mult_array_en_);
  for (int i = 0; i < Pout*Pin; ++i) {
    conv_layer_ctrl_->mult_array_in_valid[i](mult_array_in_valid_[i]);
  }
  conv_layer_ctrl_->add_array_en(add_array_en_);
  for (int i = 0; i < Pout; ++i) {
    conv_layer_ctrl_->add_array_in_valid[i](add_array_in_valid_[i]);
  }
  conv_layer_ctrl_->add_array_out_reg_select(add_array_out_reg_select_);
  conv_layer_ctrl_->demux_out_reg_clear(demux_out_reg_clear_);
  conv_layer_ctrl_->demux_out_reg_enable(demux_out_reg_enable_);
  conv_layer_ctrl_->demux_select(demux_select_);

  // initialize Nin line buffers
  line_buffer_ = new LineBuffer* [Nin];
  for (int i = 0; i < Nin; ++i) {
    sprintf(name, "line_buffer_%d", i);
    line_buffer_[i] = new LineBuffer(name, Kh, Kw, h+2*Pad_h, w+2*Pad_w);
    line_buffer_[i]->clock(clock);
    line_buffer_[i]->reset(reset);
    line_buffer_[i]->input_data(line_buffer_in_data_[i]);
    line_buffer_[i]->input_data_valid(line_buffer_valid_);
    for (int k = 0; k < Kh*Kw; ++k) {
      line_buffer_[i]->output_data[k](line_buffer_out_data_[i*Kh*Kw+k]);
    }
  }

  // initialize line buffer mux
  sprintf(name, "%s", "line_buffer_mux");
  line_buffer_mux_ = new LineBufferMux(name, Kh, Kw, Nin, Pin);
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

  // initialize weight memory
  sprintf(name, "%s", "weight_mem");
  weight_mem_ = new WeightMem(name, Kh, Kw, Pin, Pout, Nin, Nout);
  weight_mem_->clock(clock);
  weight_mem_->reset(reset);
  weight_mem_->mem_rd_en(weight_mem_rd_en_);
  weight_mem_->mem_rd_addr(weight_mem_rd_addr_);
  for (int i = 0; i < Pout*Pin*Kh*Kw; ++i) {
    weight_mem_->mem_rd_data[i](weight_mem_rd_data_[i]);
  }

  // initialize multiplier array
  sprintf(name, "%s", "mult_array");
  mult_array_ = new MultArray(name, Kh, Kw, Pin, Pout);
  mult_array_->clock(clock);
  mult_array_->reset(reset);
  mult_array_->mult_array_en(mult_array_en_);
  for (int i = 0; i < Pout*Pin; ++i) {
    mult_array_->mult_array_in_valid[i](mult_array_in_valid_[i]);
  }
  for (int i = 0; i < Pin*Kh*Kw; ++i) {
    mult_array_->mult_array_act_in_data[i](line_buffer_mux_out_data_[i]);
  }
  for (int i = 0; i < Pout*Pin*Kh*Kw; ++i) {
    mult_array_->mult_array_weight_in_data[i](weight_mem_rd_data_[i]);
  }
  for (int i = 0; i < Pout*Pin*Kh*Kw; ++i) {
    mult_array_->mult_array_output_data[i](mult_array_out_data_[i]);
  }

  // initialize the add array
  sprintf(name, "%s", "add_array");
  add_array_ = new AddArray(name, Kh, Kw, Pin, Pout);
  add_array_->clock(clock);
  add_array_->reset(reset);
  add_array_->add_array_enable(add_array_en_);
  for (int i = 0; i < Pout; ++i) {
    add_array_->add_array_in_valid[i](add_array_in_valid_[i]);
  }
  for (int i = 0; i < Pout*Pin*Kh*Kw; ++i) {
    add_array_->add_array_mult_in_data[i](mult_array_out_data_[i]);
  }
  for (int i = 0; i < Pout; ++i) {
    add_array_->add_array_reg_in_data[i](add_array_reg_in_data_[i]);
  }
  for (int i = 0; i < Pout; ++i) {
    add_array_->add_array_out_data[i](add_array_out_data_[i]);
  }

  // initialize the demux output register
  sprintf(name, "%s", "demux_out_reg");
  demux_out_reg_ = new DemuxOutReg(name, Nout, Pout);
  demux_out_reg_->clock(clock);
  demux_out_reg_->reset(reset);
  demux_out_reg_->demux_out_reg_clear(demux_out_reg_clear_);
  demux_out_reg_->demux_out_reg_enable(demux_out_reg_enable_);
  demux_out_reg_->demux_select(demux_select_);
  for (int i = 0; i < Pout; ++i) {
    demux_out_reg_->in_data[i](add_array_out_data_[i]);
  }
  for (int i = 0; i < Nout; ++i) {
    demux_out_reg_->out_data[i](out_reg_data_[i]);
  }

  // connect output register to the interface
  SC_METHOD(NextLayerDataConnect);
  for (int i = 0; i < Nout; ++i) {
    sensitive << out_reg_data_[i];
  }

  // additional process dealing with mux input
  SC_METHOD(LineBufferInMux);
  sensitive << line_buffer_zero_in_;
  for (int i = 0; i < Nin; ++i) {
    sensitive << prev_layer_data[i];
  }

  // additional process dealing with partial output results from the output
  // registers
  SC_METHOD(AddArrayRegInMux);
  sensitive << add_array_out_reg_select_;
  for (int i = 0; i < Nout; ++i) {
    sensitive << out_reg_data_[i];
  }
}

void ConvLayerPe::NextLayerDataConnect() {
  for (int i = 0; i < Nout_; ++i) {
    next_layer_data[i].write(out_reg_data_[i]);
  }
}

void ConvLayerPe::LineBufferInMux() {
  for (int i = 0; i < Nin_; ++i) {
    if (line_buffer_zero_in_.read()) {
      line_buffer_in_data_[i] = Payload(0);
    } else {
      line_buffer_in_data_[i] = prev_layer_data[i];
    }
  }
}

void ConvLayerPe::AddArrayRegInMux() {
  for (int i = 0; i < Pout_; ++i) {
    const int output_feat_start_idx = add_array_out_reg_select_.read();
    if (output_feat_start_idx+i < Nout_) {
      add_array_reg_in_data_[i] = out_reg_data_[output_feat_start_idx+i];
    } else {
      add_array_reg_in_data_[i] = Payload(0);
    }
  }
}

/*
 * Implementation notes: Destructor
 * ---------------------------------
 * Free the dynamic allocated memory space associated with the ConvLayerPe.
 */
ConvLayerPe::~ConvLayerPe() {
  delete [] prev_layer_data;
  delete [] next_layer_data;
  delete [] line_buffer_in_data_;
  delete [] line_buffer_out_data_;
  delete [] line_buffer_mux_out_data_;
  delete [] weight_mem_rd_data_;
  delete [] mult_array_in_valid_;
  delete [] mult_array_out_data_;
  delete [] add_array_in_valid_;
  delete [] add_array_out_data_;
  delete [] out_reg_data_;
  delete conv_layer_ctrl_;
  for (int i = 0; i < Nin_; ++i) {
    delete line_buffer_[i];
  }
  delete [] line_buffer_;
  delete line_buffer_mux_;
  delete weight_mem_;
  delete mult_array_;
  delete add_array_;
  delete demux_out_reg_;
}

/*
 * Implementation notes: Area
 * ---------------------------
 * Accumulate each submodules's area. Note: not all modules have the area
 * definition. We only provide the area model for major blocks.
 */
double ConvLayerPe::Area(int bit_width, int tech_node,
    config::ConfigParameter_MemoryType weight_memory_type) const {
  double total_area = 0.;
  // line buffer area: distribute implementation (overhead)
//  for (int i = 0; i < Nin_; ++i) {
//    total_area += line_buffer_[i]->Area(bit_width, tech_node);
//  }
  // line buffer area: centralized implementation
  int line_buffer_memory_size = 0;
  for (int i = 0; i < Nin_; ++i) {
    line_buffer_memory_size += line_buffer_[i]->MemorySize();
  }
  MemoryModel line_buffer_memory_model(line_buffer_memory_size*bit_width/1024.,
      tech_node, config::ConfigParameter_MemoryType_RAM);
  total_area += line_buffer_memory_model.Area();

  // weight memory
  total_area += weight_mem_->Area(bit_width, tech_node, weight_memory_type);
  // multiplier array
  total_area += mult_array_->Area(bit_width, tech_node);
  // adder array
  total_area += add_array_->Area(bit_width, tech_node);

  return total_area;
}
