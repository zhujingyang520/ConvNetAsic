/*
 * Filename: top.cpp
 * ------------------
 * This file implements the Top module.
 */

#include "header/systemc/top.hpp"
using namespace std;

Top::Top(sc_module_name module_name, const Net& net,
    const config::ConfigParameter& config_param, sc_trace_file* tf)
  : sc_module(module_name) {
  // allocate the ConvNetAcc
  convnet_acc = new ConvNetAcc("convnet_acc", net, config_param, tf);
  convnet_acc->clock(clock);
  convnet_acc->reset(reset);
  convnet_acc->input_layer_valid(input_layer_valid);
  convnet_acc->input_layer_rdy(input_layer_rdy);
  // allocate the input data
  input_layer_data = new sc_signal<Payload> [convnet_acc->Nin_];
  for (int i = 0; i < convnet_acc->Nin_; ++i) {
    convnet_acc->input_layer_data[i](input_layer_data[i]);
  }
  convnet_acc->output_layer_valid(output_layer_valid);
  convnet_acc->output_layer_rdy(output_layer_rdy);
  // allocate the output data
  output_layer_data = new sc_signal<Payload> [convnet_acc->Nout_];
  for (int i = 0; i < convnet_acc->Nout_; ++i) {
    convnet_acc->output_layer_data[i](output_layer_data[i]);
  }

  // allocate the Testbench
  testbench = new Testbench("testbench", convnet_acc->Nin_, convnet_acc->Nout_,
      convnet_acc->input_spatial_dim_);
  testbench->clock(clock);
  testbench->reset(reset);
  testbench->input_layer_valid(input_layer_valid);
  testbench->input_layer_rdy(input_layer_rdy);
  for (int i = 0; i < convnet_acc->Nin_; ++i) {
    testbench->input_layer_data[i](input_layer_data[i]);
  }
  testbench->output_layer_rdy(output_layer_rdy);
  testbench->output_layer_valid(output_layer_valid);
  for (int i = 0; i < convnet_acc->Nout_; ++i) {
    testbench->output_layer_data[i](output_layer_data[i]);
  }
}

Top::~Top() {
  delete [] input_layer_data;
  delete [] output_layer_data;
  delete testbench;
  delete convnet_acc;
}

void Top::ReportAreaBreakdown(int bit_width, int tech_node,
    config::ConfigParameter_MemoryType weight_memory_type) const {
  cout << "###################################" << endl;
  cout << "# ConvNetAsic Area Breakdown [um2]" << endl;
  cout << "###################################" << endl;
  double mult_area = 0;
  double line_buffer_area = 0;
  double adder_area = 0;
  double weight_mem_area = 0;
  double pool_array_area = 0;
  // convolutional layer pe
  for (size_t i = 0; i < convnet_acc->conv_layer_pe_.size(); ++i) {
    const ConvLayerPe* conv_layer_pe = convnet_acc->conv_layer_pe_[i];
    // hierarchy of convolution layer pe
    // distribute line buffer implementation (overhead)
//    double line_buffer_area = 0.;
//    for (int j = 0; j < conv_layer_pe->Nin_; ++j) {
//      line_buffer_area += conv_layer_pe->line_buffer_[j]->Area(bit_width,
//          tech_node);
//    }
    // centralized line buffer implementation
    int line_buffer_memory_size = 0;
    for (int j = 0; j < conv_layer_pe->Nin_; ++j) {
      line_buffer_memory_size += conv_layer_pe->line_buffer_[j]->MemorySize();
    }
    MemoryModel line_buffer_memory_model(line_buffer_memory_size*bit_width/1024.,
        tech_node, config::ConfigParameter_MemoryType_RAM);
    line_buffer_area += line_buffer_memory_model.Area();
    weight_mem_area += conv_layer_pe->weight_mem_->Area(bit_width, tech_node,
        weight_memory_type);
    mult_area += conv_layer_pe->mult_array_->Area(bit_width, tech_node);
    adder_area += conv_layer_pe->add_array_->Area(bit_width, tech_node);

    // LOG info
    cout << conv_layer_pe->basename() << ": " << conv_layer_pe->Area(bit_width,
        tech_node, weight_memory_type) << endl;
    cout << "\tLine Buffer: " << line_buffer_memory_model.Area() << endl;
    cout << "\tWeight Mem: " << conv_layer_pe->weight_mem_->Area(bit_width,
        tech_node, weight_memory_type) << endl;
    cout << "\tMultipier Array: " << conv_layer_pe->mult_array_->Area(bit_width,
        tech_node) << endl;
    cout << "\tAdder Array: " << conv_layer_pe->add_array_->Area(bit_width,
        tech_node) << endl;
  }

  // pooling layer pe
  for (size_t i = 0; i < convnet_acc->pool_layer_pe_.size(); ++i) {
    const PoolLayerPe* pool_layer_pe = convnet_acc->pool_layer_pe_[i];
    // hierarchy of pooling layer pe
    // distributed line buffer implementation
//    double line_buffer_area = 0.;
//    for (int j = 0; j < pool_layer_pe->Nin_; ++j) {
//      line_buffer_area += pool_layer_pe->line_buffer_[j]->Area(bit_width,
//          tech_node);
//    }
    // centralized line buffer implementation
    int line_buffer_memory_size = 0;
    for (int j = 0; j < pool_layer_pe->Nin_; ++j) {
      line_buffer_memory_size += pool_layer_pe->line_buffer_[j]->MemorySize();
    }
    MemoryModel line_buffer_memory_model(line_buffer_memory_size*bit_width/1024.,
        tech_node, config::ConfigParameter_MemoryType_RAM);
    line_buffer_area += line_buffer_memory_model.Area();
    pool_array_area += pool_layer_pe->pool_array_->Area(bit_width, tech_node);

    // LOG info
    cout << pool_layer_pe->basename() << ": " << pool_layer_pe->Area(bit_width,
        tech_node) << endl;
    cout << "\tLine Buffer: " << line_buffer_memory_model.Area() << endl;
    cout << "\tPool Array: " << pool_layer_pe->pool_array_->Area(bit_width,
        tech_node) << endl;
  }

  // channel buffer
  for (size_t i = 0; i < convnet_acc->channel_buffer_.size(); ++i) {
    const ChannelBuffer* channel_buffer = convnet_acc->channel_buffer_[i];
    cout << channel_buffer->basename() << " with max buffer depth: "
      << channel_buffer->MaxBufferSize() << " of area "
      << channel_buffer->Area(bit_width, tech_node) << endl;
  }

  // LOG info
  cout << "###############################" << endl;
  cout << "Mult area: " << mult_area << endl;
  cout << "Line buffer area: " << line_buffer_area << endl;
  cout << "Adder area:" << adder_area << endl;
  cout << "Weight memory area: " << weight_mem_area << endl;
  cout << "Pool array area: " << pool_array_area << endl;
  cout << "###############################" << endl;
}
