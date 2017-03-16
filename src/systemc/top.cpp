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
      convnet_acc->input_spatial_dim_, config_param.early_stop_frame_size());
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

void Top::ReportAreaBreakdown() const {
  cout << "###################################" << endl;
  cout << "# ConvNetAsic Area Breakdown [um2]" << endl;
  cout << "###################################" << endl;
  double mult_area = 0;
  double line_buffer_area = 0;
  double line_buffer_mux_area = 0;
  double adder_area = 0;
  double weight_mem_area = 0;
  double pool_array_area = 0;
  double demux_area = 0;
  double channel_buffer_area = 0;
  // convolutional layer pe
  for (size_t i = 0; i < convnet_acc->conv_layer_pe_.size(); ++i) {
    const ConvLayerPe* conv_layer_pe = convnet_acc->conv_layer_pe_[i];
    // hierarchy of convolution layer pe
    // centralized line buffer implementation
    // where the width is concatenation over all channels
    line_buffer_area += conv_layer_pe->line_buffer_array_->Area();
    line_buffer_mux_area += conv_layer_pe->line_buffer_mux_->Area();
    weight_mem_area += conv_layer_pe->weight_mem_->Area();
    mult_area += conv_layer_pe->mult_array_->Area();
    adder_area += conv_layer_pe->add_array_->Area();
    demux_area += conv_layer_pe->demux_out_reg_->Area();

    // LOG info
    cout << conv_layer_pe->basename() << ": " << conv_layer_pe->Area() << endl;
    cout << "\tLine Buffer: " << conv_layer_pe->line_buffer_array_->Area()
      << endl;
    cout << "\tLine Buffer Mux: " << conv_layer_pe->line_buffer_mux_->Area()
      << endl;
    cout << "\tWeight Mem: " << conv_layer_pe->weight_mem_->Area() << endl;
    cout << "\tMultipier Array: " << conv_layer_pe->mult_array_->Area()
      << endl;
    cout << "\tAdder Array: " << conv_layer_pe->add_array_->Area() << endl;
    cout << "\tDemux: " << conv_layer_pe->demux_out_reg_->Area() << endl;
  }

  // pooling layer pe
  for (size_t i = 0; i < convnet_acc->pool_layer_pe_.size(); ++i) {
    const PoolLayerPe* pool_layer_pe = convnet_acc->pool_layer_pe_[i];
    // centralized line buffer implementation
    // where the width is concatenation over all channels
    line_buffer_area += pool_layer_pe->line_buffer_array_->Area();
    // line buffer mux
    line_buffer_mux_area += pool_layer_pe->line_buffer_mux_->Area();
    // pool array area
    pool_array_area += pool_layer_pe->pool_array_->Area();
    // demux area
    demux_area += pool_layer_pe->demux_out_reg_->Area();

    // LOG info
    cout << pool_layer_pe->basename() << ": " << pool_layer_pe->Area() << endl;
    cout << "\tLine Buffer: " << pool_layer_pe->line_buffer_array_->Area()
      << endl;
    cout << "\tLine Buffer Mux: " << pool_layer_pe->line_buffer_mux_->Area()
      << endl;
    cout << "\tPool Array: " << pool_layer_pe->pool_array_->Area() << endl;
    cout << "\tDemux: " << pool_layer_pe->demux_out_reg_->Area() << endl;
  }

  // channel buffer
  for (size_t i = 0; i < convnet_acc->channel_buffer_.size(); ++i) {
    const ChannelBuffer* channel_buffer = convnet_acc->channel_buffer_[i];
    channel_buffer_area += channel_buffer->Area();
    cout << channel_buffer->basename() << " with max buffer depth: "
      << channel_buffer->MaxBufferSize() << " of area "
      << channel_buffer->Area() << endl;
  }

  // LOG info
  cout << "###############################" << endl;
  cout << "Mult area: " << mult_area << endl;
  cout << "Line buffer area: " << line_buffer_area << endl;
  cout << "Line buffer mux area: " << line_buffer_mux_area << endl;
  cout << "Adder area:" << adder_area << endl;
  cout << "Weight memory area: " << weight_mem_area << endl;
  cout << "Pool array area: " << pool_array_area << endl;
  cout << "Demux area: " << demux_area << endl;
  cout << "Channel buffer area: " << channel_buffer_area << endl;
  cout << "###############################" << endl;
}

void Top::ReportPowerBreakdown() const {
  cout << "###################################" << endl;
  cout << "# ConvNetAsic Power Breakdown [uW]" << endl;
  cout << "# Static Power [S], Dynamic Power [D], Total Power [T]" << endl;
  cout << "###################################" << endl;
  double mult_static = 0, mult_dynamic = 0;
  double line_buffer_static = 0, line_buffer_dynamic = 0;
  double line_buffer_mux_static = 0, line_buffer_mux_dynamic = 0;
  double adder_static = 0, adder_dynamic = 0;
  double weight_mem_static = 0, weight_mem_dynamic = 0;
  double pool_array_static = 0, pool_array_dynamic = 0;
  double demux_static = 0, demux_dynamic = 0;
  double channel_buffer_static = 0, channel_buffer_dynamic = 0;
  // convolutional layer pe
  for (size_t i = 0; i < convnet_acc->conv_layer_pe_.size(); ++i) {
    const ConvLayerPe* conv_layer_pe = convnet_acc->conv_layer_pe_[i];
    // hierarchy of convolution layer pe
    line_buffer_static += conv_layer_pe->line_buffer_array_->StaticPower();
    line_buffer_dynamic += conv_layer_pe->line_buffer_array_->DynamicPower();
    line_buffer_mux_static += conv_layer_pe->line_buffer_mux_->StaticPower();
    line_buffer_mux_dynamic += conv_layer_pe->line_buffer_mux_->DynamicPower();
    weight_mem_static += conv_layer_pe->weight_mem_->StaticPower();
    weight_mem_dynamic += conv_layer_pe->weight_mem_->DynamicPower();
    mult_static += conv_layer_pe->mult_array_->StaticPower();
    mult_dynamic += conv_layer_pe->mult_array_->DynamicPower();
    adder_static += conv_layer_pe->add_array_->StaticPower();
    adder_dynamic += conv_layer_pe->add_array_->DynamicPower();
    demux_static += conv_layer_pe->demux_out_reg_->StaticPower();
    demux_dynamic += conv_layer_pe->demux_out_reg_->DynamicPower();

    // LOG info
    cout << conv_layer_pe->basename() << ": " << conv_layer_pe->TotalPower()
      << endl;
    cout << "\tLine Buffer: [S]: " << conv_layer_pe->line_buffer_array_->
      StaticPower() << " [D]: " << conv_layer_pe->line_buffer_array_->
      DynamicPower() << " [T]: " << conv_layer_pe->line_buffer_array_->
      TotalPower()<< endl;
    cout << "\tLine Buffer Mux: [S]: " << conv_layer_pe->line_buffer_mux_->
      StaticPower() << " [D]: " << conv_layer_pe->line_buffer_mux_->
      DynamicPower() << " [T]: " << conv_layer_pe->line_buffer_mux_->
      TotalPower() << endl;
    cout << "\tWeight Mem: [S]: " << conv_layer_pe->weight_mem_->StaticPower()
     << " [D]: " << conv_layer_pe->weight_mem_->DynamicPower()
     << " [T]: " << conv_layer_pe->weight_mem_->TotalPower() << endl;
    cout << "\tMultipier Array: [S]: " << conv_layer_pe->mult_array_->
      StaticPower() << " [D]: " << conv_layer_pe->mult_array_->DynamicPower()
      << " [T]: " << conv_layer_pe->mult_array_->TotalPower() << endl;
    cout << "\tAdder Array: [S]: " << conv_layer_pe->add_array_->StaticPower()
     << " [D]: " << conv_layer_pe->add_array_->DynamicPower()
     << " [T]: " << conv_layer_pe->add_array_->TotalPower() << endl;
    cout << "\tDemux: [S]: " << conv_layer_pe->demux_out_reg_->StaticPower()
      << " [D]: " << conv_layer_pe->demux_out_reg_->DynamicPower()
      << " [T]: " << conv_layer_pe->demux_out_reg_->TotalPower() << endl;
  }

  // pooling layer pe
  for (size_t i = 0; i < convnet_acc->pool_layer_pe_.size(); ++i) {
    const PoolLayerPe* pool_layer_pe = convnet_acc->pool_layer_pe_[i];
    line_buffer_static += pool_layer_pe->line_buffer_array_->StaticPower();
    line_buffer_dynamic += pool_layer_pe->line_buffer_array_->DynamicPower();
    line_buffer_mux_static += pool_layer_pe->line_buffer_mux_->StaticPower();
    line_buffer_mux_dynamic += pool_layer_pe->line_buffer_mux_->DynamicPower();
    pool_array_static += pool_layer_pe->pool_array_->StaticPower();
    pool_array_dynamic += pool_layer_pe->pool_array_->DynamicPower();
    demux_static += pool_layer_pe->demux_out_reg_->StaticPower();
    demux_dynamic += pool_layer_pe->demux_out_reg_->DynamicPower();

    // LOG info
    cout << pool_layer_pe->basename() << ": " << pool_layer_pe->TotalPower()
      << endl;
    cout << "\tLine Buffer: [S]: " << pool_layer_pe->line_buffer_array_->
      StaticPower() << " [D]: " << pool_layer_pe->line_buffer_array_->
      DynamicPower() << " [T]: " << pool_layer_pe->line_buffer_array_->
      TotalPower() << endl;
    cout << "\tLine Buffer Mux: [S]: " << pool_layer_pe->line_buffer_mux_->
      StaticPower() << " [D]: " << pool_layer_pe->line_buffer_mux_->
      DynamicPower() << " [T]: " << pool_layer_pe->line_buffer_mux_->
      TotalPower() << endl;
    cout << "\tPool Array: [S]: " << pool_layer_pe->pool_array_->StaticPower()
      << " [D]: " << pool_layer_pe->pool_array_->DynamicPower()
      << " [T]: " << pool_layer_pe->pool_array_->TotalPower() << endl;
    cout << "\tDemux: [S]: " << pool_layer_pe->demux_out_reg_->StaticPower()
      << " [D]: " << pool_layer_pe->demux_out_reg_->DynamicPower()
      << " [T]: " << pool_layer_pe->demux_out_reg_->TotalPower() << endl;
  }

  // channel buffer
  for (size_t i = 0; i < convnet_acc->channel_buffer_.size(); ++i) {
    const ChannelBuffer* channel_buffer = convnet_acc->channel_buffer_[i];
    channel_buffer_static += channel_buffer->StaticPower();
    channel_buffer_dynamic += channel_buffer->DynamicPower();

    cout << channel_buffer->basename() << " with max buffer depth: "
      << channel_buffer->MaxBufferSize() << " [S]: "
      << channel_buffer-> StaticPower() << " [D]: "
      << channel_buffer->DynamicPower() << " [T]: "
      << channel_buffer->TotalPower() << endl;
  }
  cout << "###############################" << endl;
  cout << "- Line buffer: [S]: " <<  line_buffer_static << " [D]: "
    << line_buffer_dynamic << " [T]: " << line_buffer_static +
    line_buffer_dynamic << endl;
  cout << "- Line buffer mux: [S]: " << line_buffer_mux_static << " [D]: "
    << line_buffer_mux_dynamic << " [T]: " << line_buffer_static +
    line_buffer_mux_dynamic << endl;
  cout << "- Mult array: [S]: " << mult_static << " [D]: " << mult_dynamic
    << " [T]: " << mult_static + mult_dynamic << endl;
  cout << "- Adder array: [S]: " << adder_static << " [D]: " << adder_dynamic
    << " [T]: " << adder_static + adder_dynamic << endl;
  cout << "- Weight memory: [S]: " << weight_mem_static << " [D]: "
    << weight_mem_dynamic << " [T]: "<< weight_mem_static + weight_mem_dynamic
    <<  endl;
  cout << "- Pool array: [S]: " << pool_array_static << " [D]: "
    << pool_array_dynamic << " [T]: " << pool_array_static + pool_array_dynamic
    << endl;
  cout << "- Demux: [S]: " << demux_static << " [D]: " << demux_dynamic
    << " [T]: " << demux_static + demux_dynamic << endl;
  cout << "- Channel buffer: [S]: " << channel_buffer_static << " [D]: "
    << channel_buffer_dynamic << " [T]: " << channel_buffer_static +
    channel_buffer_dynamic << endl;
  cout << "###############################" << endl;
}

/*
 * Implementation notes: ReportMemoryDistribution
 * -----------------------------------------------
 * Report the memory distribution in the ConvNet ASIC accelerator.
 */
void Top::ReportMemoryDistribution() const {
  cout << "###################################" << endl;
  cout << "# Memory distribution in ConvNetAcc" << endl;
  cout << "# Depth [no.] x Width [bits]" << endl;
  cout << "###################################" << endl;
  // convolutional layer pe
  cout << "#########################" << endl;
  cout << "# Convolutional layer PE" << endl;
  cout << "#########################" << endl;
  for (size_t i = 0; i < convnet_acc->conv_layer_pe_.size(); ++i) {
    const ConvLayerPe* conv_layer_pe = convnet_acc->conv_layer_pe_[i];
    cout << "#" << conv_layer_pe->basename() << endl;
    cout << "\t" << "line buffer: " << conv_layer_pe->line_buffer_array_->
      MemoryDepth() << "x" << conv_layer_pe->line_buffer_array_->MemoryWidth()
      << endl;
    cout << "\t" << "weight memory: " << conv_layer_pe->weight_mem_->
      MemoryDepth() << "x" << conv_layer_pe->weight_mem_->MemoryWidth() << endl;
  }

  // pooling layer pe
  cout << "#########################" << endl;
  cout << "# Pooling layer PE" << endl;
  cout << "#########################" << endl;
  for (size_t i = 0; i < convnet_acc->pool_layer_pe_.size(); ++i) {
    const PoolLayerPe* pool_layer_pe = convnet_acc->pool_layer_pe_[i];
    cout << "#" << pool_layer_pe->basename() << endl;
    cout << "\t" << "line buffer: " << pool_layer_pe->line_buffer_array_->
      MemoryDepth() << "x" << pool_layer_pe->line_buffer_array_->MemoryWidth()
      << endl;
  }

  // channel buffer
  cout << "#########################" << endl;
  cout << "# Channel buffer" << endl;
  cout << "#########################" << endl;
  for (size_t i = 0; i < convnet_acc->channel_buffer_.size(); ++i) {
    const ChannelBuffer* channel_buffer = convnet_acc->channel_buffer_[i];
    cout << "#" << channel_buffer->basename() << endl;
    cout << "\t" << "line buffer: " << channel_buffer->MemoryDepth() << "x"
      << channel_buffer->MemoryWidth() << endl;
  }

  cout << "###################################" << endl;
}
