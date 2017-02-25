/*
 * Filename: convnet_acc.cpp
 * --------------------------
 * This file implements the top module of the class of ConvNet Accelerator.
 */

#include "header/systemc/convnet_acc.hpp"
#include "header/caffe/layers/conv_layer.hpp"
#include "header/caffe/layers/inner_product_layer.hpp"
#include "header/caffe/layers/pooling_layer.hpp"

using namespace std;
using namespace config;

ConvNetAcc::ConvNetAcc(sc_module_name module_name, const Net& net,
    const ConfigParameter& config_param, sc_trace_file* tf)
  : sc_module(module_name), tf_(tf) {
  // obtain required variables from the configure parameter
  append_buffer_capacity_ = config_param.append_buffer_capacity();
  // initialize the parallelism
  InitParallelism(net, config_param.pixel_inference_rate());

  // initialize the network processing elements
  Init(net);

  // Allocates & makes the input layer connections
  input_layer_data = new sc_in<Payload> [Nin_];
  SC_METHOD(InputLayerConnections);
  sensitive << input_layer_valid << *layer_rdy_[input_blob_idx_];
  for (int i = 0; i < Nin_; ++i) {
    sensitive << input_layer_data[i];
  }

  // Allocates & makes the output layer connections
  output_layer_data = new sc_out<Payload> [Nout_];
  SC_METHOD(OutputLayerConnections);
  sensitive << output_layer_rdy << *layer_valid_[output_blob_idx_];
  for (int i = 0; i < Nout_; ++i) {
    sensitive << layer_data_[output_blob_idx_][i];
  }

  //cout << "Nin: " << Nin_ << " Nout: " << Nout_ << endl;
}

void ConvNetAcc::InputLayerConnections() {
  layer_valid_[input_blob_idx_]->write(input_layer_valid.read());
  input_layer_rdy.write(layer_rdy_[input_blob_idx_]->read());
  for (int i = 0; i < Nin_; ++i) {
    layer_data_[input_blob_idx_][i].write(input_layer_data[i].read());
  }
}

void ConvNetAcc::OutputLayerConnections() {
  layer_rdy_[output_blob_idx_]->write(output_layer_rdy.read());
  output_layer_valid.write(layer_valid_[output_blob_idx_]->read());
  for (int i = 0; i < Nout_; ++i) {
    output_layer_data[i].write(layer_data_[output_blob_idx_][i].read());
  }
}

/*
 * Implementation notes: InitParallelism
 * --------------------------------------
 * Determine the parallelism of each layer based on the specified
 * pixel_inference_rate.
 */
void ConvNetAcc::InitParallelism(const Net& net, int pixel_inference_rate) {
  int max_inference_rate = 0;
  vector< pair<int, int> > inference_rate_array;
  int layer_inference_rate = 0;
  // the actual pixel inference rate for the accelerator is
  // pixel_inference_rate = ceil(Nin/Pin) * ceil(Nout/Pout) + pipeline_stage
  for (size_t layer_id = 0; layer_id < net.layers_.size(); ++layer_id) {
    const Layer* layer = net.layers_[layer_id];
    const string layer_name = layer->layer_param().name();
    if (layer->layer_param().type() == "Input") {
      // dimension: (N, C, H, W)
      const int h = net.top_blobs_shape_ptr_[layer_id][0]->at(2);
      const int w = net.top_blobs_shape_ptr_[layer_id][0]->at(3);
      input_spatial_dim_ = h * w;
      layer_inference_rate = input_spatial_dim_ * pixel_inference_rate;
      cout << "layer_inference_rate: " << layer_inference_rate << endl;
      continue;
    } else if (layer->layer_param().type() == "Convolution") {
      const int Nin = dynamic_cast<const ConvolutionLayer*>(layer)->num_input_;
      const int Nout = dynamic_cast<const ConvolutionLayer*>(layer)->
        num_output_;
      const int h = net.top_blobs_shape_ptr_[layer_id][0]->at(2);
      const int w = net.top_blobs_shape_ptr_[layer_id][0]->at(3);
      parallelism_[layer_id] = CalculateParallelsim(Nin, Nout, h, w,
          layer_inference_rate);
      const int inference_rate = (ceil(static_cast<double>(Nin) /
          parallelism_[layer_id].first) * ceil(static_cast<double>(Nout) /
            parallelism_[layer_id].second) + pipeline_stage_) * h * w;
      if (max_inference_rate < inference_rate) {
        max_inference_rate = inference_rate;
      }
      inference_rate_array.push_back(make_pair(layer_id, inference_rate));
    } else if (layer->layer_param().type() == "InnerProduct") {
      const int Nin = net.bottom_blobs_shape_ptr_[layer_id][0]->at(1);
      const int Nout = dynamic_cast<const InnerProductLayer*>(layer)->
        num_output_;
      const int h = 1;
      const int w = 1;
      parallelism_[layer_id] = CalculateParallelsim(Nin, Nout, h, w,
          layer_inference_rate);
      const int inference_rate = (ceil(static_cast<double>(Nin) /
          parallelism_[layer_id].first) * ceil(static_cast<double>(Nout) /
            parallelism_[layer_id].second) + pipeline_stage_) * h * w;
      if (max_inference_rate < inference_rate) {
        max_inference_rate = inference_rate;
      }
      inference_rate_array.push_back(make_pair(layer_id, inference_rate));
    } else if (layer->layer_param().type() == "Pooling") {
      const int Nin = dynamic_cast<const PoolingLayer*>(layer)->num_input_;
      const int h = net.top_blobs_shape_ptr_[layer_id][0]->at(2);
      const int w = net.top_blobs_shape_ptr_[layer_id][0]->at(3);
      parallelism_[layer_id] = CalculateParallelsim(Nin, 0, h, w,
          layer_inference_rate);
      const int inference_rate = (ceil(static_cast<double>(Nin) /
          parallelism_[layer_id].first) + pipeline_stage_) * h * w;
      if (max_inference_rate < inference_rate) {
        max_inference_rate = inference_rate;
      }
      inference_rate_array.push_back(make_pair(layer_id, inference_rate));
    } else {
      // bypass the other types, there is no need to determine the parallelism
      // for such layers
      continue;
    }
    cout << "- set " << layer_name << " Pin: " << parallelism_[layer_id].first
      << " Pout: " << parallelism_[layer_id].second << endl;
  }

  cout << "################################" << endl;
  cout << "# Resulted layer inference rate " << endl;
  cout << "################################" << endl;
  for (size_t i = 0; i < inference_rate_array.size(); ++i) {
    const int layer_id = inference_rate_array[i].first;
    const int inference_rate = inference_rate_array[i].second;
    cout << "- " << net.layers_[layer_id]->layer_param().name() << " with rate "
      << inference_rate << endl;
  }
  cout << "Max layer inference rate: " << max_inference_rate << endl;
  cout << "Press ENTER to continue";
  cin.get();
}

pair<int, int> ConvNetAcc::CalculateParallelsim(int Nin, int Nout, int h, int w,
    int layer_inference_rate) const {
  // regularize the layer_inference_rate
  if (layer_inference_rate <= 0) {
    layer_inference_rate = 1;
  }

  if (Nout == 0) {
    // POOL: pixel inference rate = Nin / Pin + pipeline_stage
    double pixel_inference_rate = static_cast<double>(layer_inference_rate) /
      (h * w) - pipeline_stage_;
    if (pixel_inference_rate <= 0) {
      pixel_inference_rate = 1;
    }
    int Pin = round(static_cast<double>(Nin) /
        static_cast<double>(pixel_inference_rate));
    // regularize the parallelism
    if (Pin <= 0) Pin = 1;
    if (Pin > Nin) Pin = Nin;
    return make_pair(Pin, 0);
  } else {
    // CONV or FC: inference rate = Nin * Nout / Pin / Pout + pipeline_stage
    double pixel_inference_rate = static_cast<double>(layer_inference_rate) /
      (h * w) - pipeline_stage_;
    if (pixel_inference_rate <= 0) {
      pixel_inference_rate = 1;
    }
    double k = sqrt(1. / static_cast<double>(pixel_inference_rate));
    int Pin = round(k * Nin);
    int Pout = round(k * Nout);
    // regularize the parallelism
    if (Pin <= 0) Pin = 1;
    if (Pin > Nin) Pin = Nin;
    if (Pout <= 0) Pout = 1;
    if (Pout > Nout) Pout = Nout;
    return make_pair(Pin, Pout);
  }
}

/*
 * Implementation notes: Init
 * ---------------------------------------------------------------------
 * Initialize the ConvNetAcc with the parsed network.
 */
void ConvNetAcc::Init(const Net& net) {
  for (size_t layer_id = 0; layer_id < net.layers_.size(); ++layer_id) {
    const Layer* layer = net.layers_[layer_id];
    cout << "ConvNetAcc[" << layer_id << "] of type: "
      << layer->layer_param().type() << endl;

    // input layer
    if (layer->layer_param().type() == "Input") {
      // instantiates the input layer (allocate new connections)
      InitInputLayer(net, layer_id);
    } else if (layer->layer_param().type() == "Convolution") {
      // instantiates the convolutional pe in the ConvNetAcc
      InitConvolutionPe(net, layer_id);
    } else if (layer->layer_param().type() == "InnerProduct") {
      // instantiates the inner product layer using convolutional pe
      InitInnerProductLayer(net, layer_id);
    } else if (layer->layer_param().type() == "Pooling") {
      // instantiates the pooling pe in the ConvNetAcc
      InitPoolingPe(net, layer_id);
    } else if (layer->layer_param().type() == "Split") {
      // instantiates the split layer pe in the ConvNetAcc
      InitSplitLayer(net, layer_id);
    } else if (layer->layer_param().type() == "Concat") {
      // instantiates the concat layer pe in the ConvNetAcc
      InitConcatLayer(net, layer_id);
    } else {
      // bypass the remaining layers
      BypassLayer(net, layer_id);
    }
  }
}

void ConvNetAcc::InitInputLayer(const Net& net, int layer_id) {
  const Layer* layer = net.layers_[layer_id];
  // sanity check
  assert(layer->layer_param().type() == "Input");
  const int Nin = net.top_blobs_shape_ptr_[layer_id][0]->at(1);

  // allocate the input channel depth at dim 1 (N, C, H, W) in caffe
  //const string next_connection = layer->layer_param().top(0) +
  //  "_append_channel_buffer";
  const string next_connection = layer->layer_param().top(0);
  layer_valid_.push_back(new sc_signal<bool>);
  layer_rdy_.push_back(new sc_signal<bool>);
  layer_data_.push_back(new sc_signal<Payload> [Nin]);
  interconnections_to_idx_[next_connection] =
    layer_valid_.size() - 1;
  // record the input blob index & number
  Nin_ = Nin;
  input_blob_idx_ = layer_valid_.size() - 1;

  // add the data path to trace file
  if (tf_) {
    const string layer_name = next_connection;
    char name [100];
    sprintf(name, "%s_valid", layer_name.c_str());
    sc_trace(tf_, *layer_valid_.back(), name);
    sprintf(name, "%s_rdy", layer_name.c_str());
    sc_trace(tf_, *layer_rdy_.back(), name);
    for (int i = 0; i < Nin_; ++i) {
      sprintf(name, "%s_data_%d", layer_name.c_str(), i);
      sc_trace(tf_, layer_data_.back()[i], name);
    }
  }

  // log info: map the interconnection name to the the interconnection index
  cout << "Input layer: " << net.layers_name_[layer_id]
    << " allocates new connections: " << next_connection
    << " to index " << layer_valid_.size() - 1 << endl;

  // append the channel buffer to the end of the current layer
  //AppendChannelBuffer(net, layer_id, 0, append_buffer_capacity_);
}

/*
 * Implementation notes: BypassLayer
 * ----------------------------------
 * We will bypass the layer with the specified layer_id. The top blob (next
 * layer connection) will be remapped to its bottom blob if the current layer is
 * not an in-place layer.
 */
void ConvNetAcc::BypassLayer(const Net& net, int layer_id) {
  // skip the implementation of trival layers, e.g. normalization, relu
  // the interconnections of these layers should be registered as well
  const Layer* layer = net.layers_[layer_id];
  // sanity check: we only skip the layers with the single bottom blob
  assert(layer->layer_param().bottom_size() == 1);

  // register the new interconnection name with the bottom interconnection
  // index
  const string prev_connection = layer->layer_param().bottom(0);
  if (interconnections_to_idx_.find(prev_connection) ==
      interconnections_to_idx_.end()) {
    cerr << "undefined previous layer connections: " << prev_connection
      << endl;
    exit(1);
  }
  const int prev_connection_idx = interconnections_to_idx_[prev_connection];
  for (int top_id = 0; top_id < layer->layer_param().top_size(); ++top_id) {
    const string next_connection = layer->layer_param().top(top_id);
    if (prev_connection != next_connection) {
      // register the new interconnection name
      interconnections_to_idx_[next_connection] = prev_connection_idx;
      cout << "register new connection: " << next_connection << " to idx "
        << prev_connection_idx << endl;
    }
  }
  cout << "Bypass layer " << net.layers_name_[layer_id] << endl;
}

void ConvNetAcc::InitPoolingPe(const Net& net, int layer_id) {
  const Layer* layer = net.layers_[layer_id];
  // sanity check
  assert(layer->layer_param().type() == "Pooling");

  // parse the related Pool parameters from the Caffe Network
  char module_name[100];
  sprintf(module_name, "%s_pe", net.layers_name_[layer_id].c_str());
  const int Nin = dynamic_cast<const PoolingLayer*>(layer)->num_input_;
  const int Kh = dynamic_cast<const PoolingLayer*>(layer)->kh_;
  const int Kw = dynamic_cast<const PoolingLayer*>(layer)->kw_;
  const int h = dynamic_cast<const PoolingLayer*>(layer)->h_;
  const int w = dynamic_cast<const PoolingLayer*>(layer)->w_;
  const int Stride_h = dynamic_cast<const PoolingLayer*>(layer)->stride_h_;
  const int Stride_w = dynamic_cast<const PoolingLayer*>(layer)->stride_w_;
  const int Pad_h = dynamic_cast<const PoolingLayer*>(layer)->pad_h_;
  const int Pad_w = dynamic_cast<const PoolingLayer*>(layer)->pad_w_;
  PoolArray::PoolMethod pool_method;
  if (dynamic_cast<const PoolingLayer*>(layer)->pool_method_ ==
      caffe::PoolingParameter_PoolMethod_MAX) {
    pool_method = PoolArray::MAX;
  } else if (dynamic_cast<const PoolingLayer*>(layer)->pool_method_ ==
      caffe::PoolingParameter_PoolMethod_AVE) {
    pool_method = PoolArray::AVG;
  } else {
    cerr << "undefined pooling method: "
      << dynamic_cast<const PoolingLayer*>(layer)->pool_method_ << endl;
    exit(1);
  }
  //const int Pin = Nin;
  //const int Pin = 1;
  const int Pin = parallelism_[layer_id].first;

  // allocate the new PoolLayerPe
  PoolLayerPe* pool_layer_pe = new PoolLayerPe(module_name, Kh, Kw, h, w, Nin,
      Pin, Pad_h, Pad_w, Stride_h, Stride_w, pool_method);
  pool_layer_pe_.push_back(pool_layer_pe);
  // make the connections
  pool_layer_pe->clock(clock);
  pool_layer_pe->reset(reset);
  // look for the previous layer connections (bottom blobs in Caffe)
  const string prev_connection = layer->layer_param().bottom(0);
  if (interconnections_to_idx_.find(prev_connection) ==
      interconnections_to_idx_.end()) {
    cerr << "undefined previous layer connections: " << prev_connection
      << endl;
    exit(1);
  }
  const int prev_connection_idx = interconnections_to_idx_[prev_connection];
  pool_layer_pe->prev_layer_valid(*layer_valid_[prev_connection_idx]);
  pool_layer_pe->prev_layer_rdy(*layer_rdy_[prev_connection_idx]);
  for (int i = 0; i < Nin; ++i) {
    pool_layer_pe->prev_layer_data[i](layer_data_[prev_connection_idx][i]);
  }
  // allocate the next layer connections (top blobs in Caffe)
  // Nout = Nin for pooling layer
  const int Nout = Nin;
  const string next_connection = layer->layer_param().top(0) +
    "_append_channel_buffer";
  layer_valid_.push_back(new sc_signal<bool>);
  layer_rdy_.push_back(new sc_signal<bool>);
  layer_data_.push_back(new sc_signal<Payload> [Nout]);
  // record the interconnections to the map
  interconnections_to_idx_[next_connection] = layer_valid_.size() - 1;
  // make the connections of the current convolution pe
  pool_layer_pe->next_layer_rdy(*layer_rdy_.back());
  pool_layer_pe->next_layer_valid(*layer_valid_.back());
  for (int i = 0; i < Nout; ++i) {
    pool_layer_pe->next_layer_data[i](layer_data_.back()[i]);
  }
  // add the data path to trace file
  if (tf_) {
    const string layer_name = next_connection;
    char name [100];
    sprintf(name, "%s_valid", layer_name.c_str());
    sc_trace(tf_, *layer_valid_.back(), name);
    sprintf(name, "%s_rdy", layer_name.c_str());
    sc_trace(tf_, *layer_rdy_.back(), name);
#ifdef DATA_PATH
    for (int i = 0; i < Nout; ++i) {
      sprintf(name, "%s_data_%d", layer_name.c_str(), i);
      sc_trace(tf_, layer_data_.back()[i], name);
    }
#endif
  }

  cout << "Pool: " << module_name << " - Nin: " << Nin << " Nout: " << Nout
    << " Kh: " << Kh << " Kw: " << Kw << " h: " << h << " w: " << w
    << " Sh: " << Stride_h << " Sw: " << Stride_w << " Ph: " << Pad_h
    << " Pw: " << Pad_w << endl;

  cout << "previous connections: " << layer->layer_param().bottom(0) 
    << " of idx " << prev_connection_idx  << " allocate next connections: "
    << next_connection << " of idx " << layer_valid_.size()-1
    << endl;

  // append the channel buffer to the end of the current layer
  AppendChannelBuffer(net, layer_id, 0, append_buffer_capacity_);

  // update the output channel depth
  Nout_ = Nout;
  output_blob_idx_ = interconnections_to_idx_[layer->layer_param().top(0)];
}

/*
 * Implementation notes: InitInnerProductLayer
 * --------------------------------------------
 * Use convolution layer processing element do the real computation of
 * InnerProduct layer.
 */
void ConvNetAcc::InitInnerProductLayer(const Net& net, int layer_id) {
  const Layer* layer = net.layers_[layer_id];
  // sanity check
  assert(layer->layer_param().type() == "InnerProduct");

  char module_name[100];
  sprintf(module_name, "%s_pe", net.layers_name_[layer_id].c_str());
  // convert the inner product layer to the convolution layer
  // there will be 2 cases:
  // a. the bottom blob is a 3D feature map
  // b. the bottom blob is a 1D flatten neuron
  int Nin, Kh, Kw, h, w;
  if (net.bottom_blobs_shape_ptr_[layer_id][0]->size() == 4) {
    // of shape (N, C, H, W)
    Nin = net.bottom_blobs_shape_ptr_[layer_id][0]->at(1);
    h = net.bottom_blobs_shape_ptr_[layer_id][0]->at(2);
    w = net.bottom_blobs_shape_ptr_[layer_id][0]->at(3);
    // Kh = h & Kw = w
    Kh = h;
    Kw = w;
  } else if (net.bottom_blobs_shape_ptr_[layer_id][0]->size() == 2) {
    // of shape (N, C)
    Nin = net.bottom_blobs_shape_ptr_[layer_id][0]->at(1);
    h = w = Kh = Kw = 1;
  } else {
    cerr << "unexpected bottom blob shape: "
      << net.bottom_blobs_shape_ptr_[layer_id][0]->size() << endl;
    exit(1);
  }
  const int Nout = dynamic_cast<const InnerProductLayer*>(layer)->num_output_;
  // unit stride & zero padding for fully-connected layer
  const int Stride_h = 1;
  const int Stride_w = 1;
  const int Pad_h = 0;
  const int Pad_w = 0;
  //const int Pin = Nin;
  //const int Pout = Nout;
  //const int Pin = 1;
  //const int Pout = 1;
  const int Pin = parallelism_[layer_id].first;
  const int Pout = parallelism_[layer_id].second;

  // allocate the new ConvLayerPe
  ConvLayerPe* fc_layer_pe = new ConvLayerPe(module_name, Kh, Kw, h, w, Nin,
      Nout, Pin, Pout, Pad_h, Pad_w, Stride_h, Stride_w);
  conv_layer_pe_.push_back(fc_layer_pe);
  // make the connections
  fc_layer_pe->clock(clock);
  fc_layer_pe->reset(reset);
  // look for the previous layer connections (bottom blobs in Caffe)
  const string prev_connection = layer->layer_param().bottom(0);
  if (interconnections_to_idx_.find(prev_connection) ==
      interconnections_to_idx_.end()) {
    cerr << "undefined previous layer connections: " << prev_connection
      << endl;
    exit(1);
  }
  const int prev_connection_idx = interconnections_to_idx_[prev_connection];
  fc_layer_pe->prev_layer_valid(*layer_valid_[prev_connection_idx]);
  fc_layer_pe->prev_layer_rdy(*layer_rdy_[prev_connection_idx]);
  for (int i = 0; i < Nin; ++i) {
    fc_layer_pe->prev_layer_data[i](layer_data_[prev_connection_idx][i]);
  }
  // allocate the next layer connections (top blobs in Caffe)
  const string next_connection = layer->layer_param().top(0) +
    "_append_channel_buffer";
  layer_valid_.push_back(new sc_signal<bool>);
  layer_rdy_.push_back(new sc_signal<bool>);
  layer_data_.push_back(new sc_signal<Payload> [Nout]);
  // record the interconnections to the map
  interconnections_to_idx_[next_connection] = layer_valid_.size() - 1;
  // make the connections of the current convolution pe
  fc_layer_pe->next_layer_rdy(*layer_rdy_.back());
  fc_layer_pe->next_layer_valid(*layer_valid_.back());
  for (int i = 0; i < Nout; ++i) {
    fc_layer_pe->next_layer_data[i](layer_data_.back()[i]);
  }
  // add the data path to trace file
  if (tf_) {
    const string layer_name = next_connection;
    char name [100];
    sprintf(name, "%s_valid", layer_name.c_str());
    sc_trace(tf_, *layer_valid_.back(), name);
    sprintf(name, "%s_rdy", layer_name.c_str());
    sc_trace(tf_, *layer_rdy_.back(), name);
#ifdef DATA_PATH
    for (int i = 0; i < Nout; ++i) {
      sprintf(name, "%s_data_%d", layer_name.c_str(), i);
      sc_trace(tf_, layer_data_.back()[i], name);
    }
#endif
  }

  cout << "FC: " << module_name << " - Nin: " << Nin << " Nout: " << Nout
    << " Kh: " << Kh << " Kw: " << Kw << " h: " << h << " w: " << w
    << " Sh: " << Stride_h << " Sw: " << Stride_w << " Ph: " << Pad_h
    << " Pw: " << Pad_w << endl;

  cout << "previous connections: " << layer->layer_param().bottom(0)
    << " of idx " << prev_connection_idx  << " allocate next connections: "
    << next_connection << " of idx " << layer_valid_.size()-1
    << endl;

  // append the channel buffer the end of the current layer
  AppendChannelBuffer(net, layer_id, 0, append_buffer_capacity_);

  // update the output channel depth
  Nout_ = Nout;
  output_blob_idx_ = interconnections_to_idx_[layer->layer_param().top(0)];
}

void ConvNetAcc::InitConcatLayer(const Net& net, int layer_id) {
  const Layer* layer = net.layers_[layer_id];
  // sanity check
  assert(layer->layer_param().type() == "Concat");

  // instantiates the concatenation pe in the ConvNetAcc
  char module_name[100];
  sprintf(module_name, "%s_pe", net.layers_name_[layer_id].c_str());
  // obtain the concatenated feature map depth & number of splits
  const int Nin = net.top_blobs_shape_ptr_[layer_id][0]->at(1);
  const int numSplits = net.bottom_blobs_shape_ptr_[layer_id].size();
  // allocate the new ConcatPe
  ConcatPe *concat_pe = new ConcatPe(module_name, Nin, numSplits);
  concat_layer_pe_.push_back(concat_pe);
  // allocate the next layer connections (top blobs in Caffe)
  const string next_connection = layer->layer_param().top(0);
  layer_valid_.push_back(new sc_signal<bool>);
  layer_rdy_.push_back(new sc_signal<bool>);
  layer_data_.push_back(new sc_signal<Payload> [Nin]);
  // record the interconnections to the map
  interconnections_to_idx_[next_connection] = layer_valid_.size() - 1;
  // connect the next layer valid & ready signal
  concat_pe->next_layer_rdy(*layer_rdy_.back());
  concat_pe->next_layer_valid(*layer_valid_.back());
  // update the output channel number
  Nout_ = Nin;
  output_blob_idx_ = layer_valid_.size() - 1;

  int cur_data_blob_idx = 0;
  for (int blob_id = 0; blob_id < layer->layer_param().bottom_size();
      ++blob_id) {
    const string prev_connection = layer->layer_param().bottom(blob_id);
    if (interconnections_to_idx_.find(prev_connection) ==
        interconnections_to_idx_.end()) {
      cerr << "undefined previous layer connections: " << prev_connection
        << endl;
      exit(1);
    }
    // prepend the channel buffer module with the infinite capacity
    PrependChannelBuffer(net, layer_id, blob_id);

    // connect the previous connection to the channel buffer output
    if (interconnections_to_idx_.find(prev_connection +
          "_prepend_channel_buffer") ==
        interconnections_to_idx_.end()) {
      cerr << "undefined previous layer connections: " << prev_connection +
        "_prepend_channel_buffer" << endl;
      exit(1);
    }
    const int prev_connection_idx = interconnections_to_idx_[prev_connection +
      "_prepend_channel_buffer"];
    concat_pe->prev_layer_valid[blob_id](*layer_valid_[prev_connection_idx]);
    concat_pe->prev_layer_rdy[blob_id](*layer_rdy_[prev_connection_idx]);
    const int channel_depth = net.bottom_blobs_shape_ptr_[layer_id][blob_id]->
      at(1);
    for (int i = 0; i < channel_depth; ++i) {
      concat_pe->prev_layer_data[cur_data_blob_idx+i](
          layer_data_[prev_connection_idx][i]);
    }

    // connect the next layer data connection
    const int next_connection_idx = interconnections_to_idx_[next_connection];
    for (int i = 0; i < channel_depth; ++i) {
      concat_pe->next_layer_data[cur_data_blob_idx+i](
          layer_data_[next_connection_idx][cur_data_blob_idx+i]);
    }

    // update the data blob index
    cur_data_blob_idx += channel_depth;
  }

  // add the signals to the trace file
  if (tf_) {
    const int next_connection_idx = interconnections_to_idx_[next_connection];
    const string layer_name = layer->layer_param().top(0);
    char name [100];
    sprintf(name, "%s_valid", layer_name.c_str());
    sc_trace(tf_, *layer_valid_[next_connection_idx], name);
    sprintf(name, "%s_rdy", layer_name.c_str());
    sc_trace(tf_, *layer_rdy_[next_connection_idx], name);
#ifdef DATA_PATH
    for (int i = 0; i < Nin; ++i) {
      sprintf(name, "%s_data_%d", layer_name.c_str(), i);
      sc_trace(tf_, layer_data_[next_connection_idx][i], name);
    }
#endif
  }
}

void ConvNetAcc::AppendChannelBuffer(const Net& net, int layer_id, int blob_id,
    int capacity) {
  const Layer* layer = net.layers_[layer_id];
  char module_name[100];
  sprintf(module_name, "%s_append_channel_buffer_%d",
      net.layers_name_[layer_id].c_str(), blob_id);
  // obtain the input channel depth
  const int Nin = net.top_blobs_shape_ptr_[layer_id][blob_id]->at(1);
  // allocate the channel buffer
  ChannelBuffer *channel_buffer = new ChannelBuffer(module_name, Nin, capacity);
  channel_buffer_.push_back(channel_buffer);
  // make the connections
  channel_buffer->clock(clock);
  channel_buffer->reset(reset);
  const string prev_connection = layer->layer_param().top(blob_id) +
    "_append_channel_buffer";
  if (interconnections_to_idx_.find(prev_connection) ==
      interconnections_to_idx_.end()) {
    cerr << "undefined previous layer connections: " << prev_connection
      << endl;
    exit(1);
  }
  const int prev_connection_idx = interconnections_to_idx_[prev_connection];
  channel_buffer->prev_layer_valid(*layer_valid_[prev_connection_idx]);
  channel_buffer->prev_layer_rdy(*layer_rdy_[prev_connection_idx]);
  for (int i = 0; i < Nin; ++i) {
    channel_buffer->prev_layer_data[i](layer_data_[prev_connection_idx][i]);
  }
  // allocate the next layer connections
  const string next_connection = layer->layer_param().top(blob_id);
  layer_valid_.push_back(new sc_signal<bool>);
  layer_rdy_.push_back(new sc_signal<bool>);
  layer_data_.push_back(new sc_signal<Payload> [Nin]);
  // record the interconnection to the map
  interconnections_to_idx_[next_connection] = layer_valid_.size() - 1;
  // make the connection to the channle buffer
  channel_buffer->next_layer_valid(*layer_valid_.back());
  channel_buffer->next_layer_rdy(*layer_rdy_.back());
  for (int i = 0; i < Nin; ++i) {
    channel_buffer->next_layer_data[i](layer_data_.back()[i]);
  }

  // add the signals to the trace file
  if (tf_) {
    char name[100];
    sprintf(name, "%s_valid", next_connection.c_str());
    sc_trace(tf_, *layer_valid_.back(), name);
    sprintf(name, "%s_rdy", next_connection.c_str());
    sc_trace(tf_, *layer_rdy_.back(), name);
#ifdef DATA_PATH
    for (int i = 0; i < Nin; ++i) {
      sprintf(name, "%s_data_%d", next_connection.c_str(), i);
      sc_trace(tf_, layer_data_.back()[i], name);
    }
#endif
  }
  cout << "previous connections: " << prev_connection
    << " of idx " << prev_connection_idx  << " allocate next connections: "
    << next_connection << " of idx " << layer_valid_.size()-1
    << endl;
  cout << "Channel buffer: " << module_name << " Nin: " << Nin << endl;
}

void ConvNetAcc::PrependChannelBuffer(const Net& net, int layer_id, int blob_id,
    int capacity) {
  const Layer* layer = net.layers_[layer_id];
  // we only add the channel buffer in the concatenation layer
  assert(layer->layer_param().type() == "Concat");
  char module_name[100];
  sprintf(module_name, "%s_prepend_channel_buffer_%d",
      net.layers_name_[layer_id].c_str(), blob_id);
  // obtain the input channel depth
  const int Nin = net.bottom_blobs_shape_ptr_[layer_id][blob_id]->at(1);
  // allocate the channel buffer
  ChannelBuffer *channel_buffer = new ChannelBuffer(module_name, Nin, capacity);
  channel_buffer_.push_back(channel_buffer);
  // make the connections
  channel_buffer->clock(clock);
  channel_buffer->reset(reset);
  const string prev_connection = layer->layer_param().bottom(blob_id);
  if (interconnections_to_idx_.find(prev_connection) ==
      interconnections_to_idx_.end()) {
    cerr << "undefined previous layer connections: " << prev_connection
      << endl;
    exit(1);
  }
  const int prev_connection_idx = interconnections_to_idx_[prev_connection];
  channel_buffer->prev_layer_valid(*layer_valid_[prev_connection_idx]);
  channel_buffer->prev_layer_rdy(*layer_rdy_[prev_connection_idx]);
  for (int i = 0; i < Nin; ++i) {
    channel_buffer->prev_layer_data[i](layer_data_[prev_connection_idx][i]);
  }
  // allocate the next layer outputs
  // append the name with channel_buffer
  const string next_connection = prev_connection + "_prepend_channel_buffer";
  layer_valid_.push_back(new sc_signal<bool>);
  layer_rdy_.push_back(new sc_signal<bool>);
  layer_data_.push_back(new sc_signal<Payload> [Nin]);
  // record the interconnection to the map
  interconnections_to_idx_[next_connection] = layer_valid_.size() - 1;
  // make the connection of the channel buffer
  channel_buffer->next_layer_valid(*layer_valid_.back());
  channel_buffer->next_layer_rdy(*layer_rdy_.back());
  for (int i = 0; i < Nin; ++i) {
    channel_buffer->next_layer_data[i](layer_data_.back()[i]);
  }

  // add the signals to the trace file
  if (tf_) {
    char name[100];
    sprintf(name, "%s_valid", next_connection.c_str());
    sc_trace(tf_, *layer_valid_.back(), name);
    sprintf(name, "%s_rdy", next_connection.c_str());
    sc_trace(tf_, *layer_rdy_.back(), name);
#ifdef DATA_PATH
    for (int i = 0; i < Nin; ++i) {
      sprintf(name, "%s_data_%d", next_connection.c_str(), i);
      sc_trace(tf_, layer_data_.back()[i], name);
    }
#endif
  }
  cout << "previous connections: " << layer->layer_param().bottom(blob_id)
    << " of idx " << prev_connection_idx  << " allocate next connections: "
    << next_connection << " of idx " << layer_valid_.size()-1
    << endl;
  cout << "Channel buffer: " << module_name << " Nin: " << Nin << endl;
}

void ConvNetAcc::InitSplitLayer(const Net& net, int layer_id) {
  const Layer* layer = net.layers_[layer_id];
  // sanity check
  assert(layer->layer_param().type() == "Split");

  // instantiates the split pe in the ConvNetAcc
  char module_name[100];
  sprintf(module_name, "%s_pe", net.layers_name_[layer_id].c_str());
  // obtain the number of splits from the blob size
  const int Nin = net.bottom_blobs_shape_ptr_[layer_id][0]->at(1);
  const int numSplits = net.top_blobs_shape_ptr_[layer_id].size();
  // allocate the new SplitPe
  SplitPe *split_pe = new SplitPe(module_name, Nin, numSplits);
  split_layer_pe_.push_back(split_pe);
  // make the connections
  // look for the previous layer connections
  const string prev_connection = layer->layer_param().bottom(0);
  if (interconnections_to_idx_.find(prev_connection) ==
      interconnections_to_idx_.end()) {
    cerr << "undefined previous layer connections: " << prev_connection
      << endl;
    exit(1);
  }
  const int prev_connection_idx = interconnections_to_idx_[prev_connection];
  split_pe->prev_layer_valid(*layer_valid_[prev_connection_idx]);
  split_pe->prev_layer_rdy(*layer_rdy_[prev_connection_idx]);
  for (int i = 0; i < Nin; ++i) {
    split_pe->prev_layer_data[i](layer_data_[prev_connection_idx][i]);
  }
  // allocate the next layer connections (top blobs in Caffe)
  // split layer has more than one top blobs
  for (int blob_id = 0; blob_id < layer->layer_param().top_size(); ++blob_id) {
    const string next_connection = layer->layer_param().top(blob_id);
    layer_valid_.push_back(new sc_signal<bool>);
    layer_rdy_.push_back(new sc_signal<bool>);
    layer_data_.push_back(new sc_signal<Payload> [Nin]);
    // record the interconnections to the map
    interconnections_to_idx_[next_connection] = layer_valid_.size() - 1;
    // make the connections to the newly allocated port
    split_pe->next_layer_rdy[blob_id](*layer_rdy_.back());
    split_pe->next_layer_valid[blob_id](*layer_valid_.back());
    for (int i = 0; i < Nin; ++i) {
      split_pe->next_layer_data[blob_id*Nin+i](layer_data_.back()[i]);
    }

    // add the data path to the trace file
    if (tf_) {
      const string layer_name = layer->layer_param().top(blob_id);
      char name[100];
      sprintf(name, "%s_valid", layer_name.c_str());
      sc_trace(tf_, *layer_valid_.back(), name);
      sprintf(name, "%s_rdy", layer_name.c_str());
      sc_trace(tf_, *layer_rdy_.back(), name);
#ifdef DATA_PATH
      for (int i = 0; i < Nin; ++i) {
        sprintf(name, "%s_data_%d", layer_name.c_str(), i);
        sc_trace(tf_, layer_data_.back()[i], name);
      }
#endif
    }

    cout << "previous connections: " << layer->layer_param().bottom(0)
      << " of idx " << prev_connection_idx  << " allocate next connections: "
      << layer->layer_param().top(blob_id) << " of idx " << layer_valid_.size()-1
      << endl;
  }

  cout << "Split: " << module_name << " - Nin: " << Nin << " numSplits: "
    << numSplits << endl;
}

void ConvNetAcc::InitConvolutionPe(const Net& net, int layer_id) {
  const Layer* layer = net.layers_[layer_id];
  // sanity check
  assert(layer->layer_param().type() == "Convolution");

  // instantiates the convolutional pe in the ConvNetAcc
  // parse the related Conv parameters from Caffe Network
  char module_name[100];
  sprintf(module_name, "%s_pe", net.layers_name_[layer_id].c_str());
  const int Nin = dynamic_cast<const ConvolutionLayer*>(layer)->num_input_;
  const int Nout = dynamic_cast<const ConvolutionLayer*>(layer)->num_output_;
  const int Kh = dynamic_cast<const ConvolutionLayer*>(layer)->kh_;
  const int Kw = dynamic_cast<const ConvolutionLayer*>(layer)->kw_;
  const int h = dynamic_cast<const ConvolutionLayer*>(layer)->h_;
  const int w = dynamic_cast<const ConvolutionLayer*>(layer)->w_;
  const int Stride_h = dynamic_cast<const ConvolutionLayer*>(layer)->stride_h_;
  const int Stride_w = dynamic_cast<const ConvolutionLayer*>(layer)->stride_w_;
  const int Pad_h = dynamic_cast<const ConvolutionLayer*>(layer)->pad_h_;
  const int Pad_w = dynamic_cast<const ConvolutionLayer*>(layer)->pad_w_;
  //const int Pin = Nin;
  //const int Pout = Nout;
  //const int Pin = 1;
  //const int Pout = 1;
  const int Pin = parallelism_[layer_id].first;
  const int Pout = parallelism_[layer_id].second;

  // allocate the new ConvLayerPe
  ConvLayerPe* conv_layer_pe = new ConvLayerPe(module_name, Kh, Kw, h, w, Nin,
      Nout, Pin, Pout, Pad_h, Pad_w, Stride_h, Stride_w);
  conv_layer_pe_.push_back(conv_layer_pe);
  // make the connections
  conv_layer_pe->clock(clock);
  conv_layer_pe->reset(reset);
  // look for the previous layer connections (bottom blobs in Caffe)
  const string prev_connection = layer->layer_param().bottom(0);
  if (interconnections_to_idx_.find(prev_connection) ==
      interconnections_to_idx_.end()) {
    cerr << "undefined previous layer connections: " << prev_connection
      << endl;
    exit(1);
  }
  const int prev_connection_idx = interconnections_to_idx_[prev_connection];
  conv_layer_pe->prev_layer_valid(*layer_valid_[prev_connection_idx]);
  conv_layer_pe->prev_layer_rdy(*layer_rdy_[prev_connection_idx]);
  for (int i = 0; i < Nin; ++i) {
    conv_layer_pe->prev_layer_data[i](layer_data_[prev_connection_idx][i]);
  }
  // allocate the next layer connections (top blobs in Caffe)
  const string next_connection = layer->layer_param().top(0) +
    "_append_channel_buffer";
  layer_valid_.push_back(new sc_signal<bool>);
  layer_rdy_.push_back(new sc_signal<bool>);
  layer_data_.push_back(new sc_signal<Payload> [Nout]);
  // record the interconnections to the map
  interconnections_to_idx_[next_connection] = layer_valid_.size() - 1;
  // make the connections of the current convolution pe
  conv_layer_pe->next_layer_rdy(*layer_rdy_.back());
  conv_layer_pe->next_layer_valid(*layer_valid_.back());
  for (int i = 0; i < Nout; ++i) {
    conv_layer_pe->next_layer_data[i](layer_data_.back()[i]);
  }
  // add the data path to trace file
  if (tf_) {
    const string layer_name = next_connection;
    char name [100];
    sprintf(name, "%s_valid", layer_name.c_str());
    sc_trace(tf_, *layer_valid_.back(), name);
    sprintf(name, "%s_rdy", layer_name.c_str());
    sc_trace(tf_, *layer_rdy_.back(), name);
#ifdef DATA_PATH
    for (int i = 0; i < Nout; ++i) {
      sprintf(name, "%s_data_%d", layer_name.c_str(), i);
      sc_trace(tf_, layer_data_.back()[i], name);
    }

    // verbose trace
    sprintf(name, "%s_line_buffer_valid", layer->layer_param().name().c_str());
    sc_trace(tf_, conv_layer_pe->line_buffer_valid_, name);
    sprintf(name, "%s_line_buffer_zero", layer->layer_param().name().c_str());
    sc_trace(tf_, conv_layer_pe->line_buffer_zero_in_, name);
    sprintf(name, "%s_demux_out_reg_clr", layer->layer_param().name().c_str());
    sc_trace(tf_, conv_layer_pe->demux_out_reg_clear_, name);
    sprintf(name, "%s_demux_out_reg_en", layer->layer_param().name().c_str());
    sc_trace(tf_, conv_layer_pe->demux_out_reg_enable_, name);
    sprintf(name, "%s_demux_select", layer->layer_param().name().c_str());
    sc_trace(tf_, conv_layer_pe->demux_select_, name);
#endif
  }

  cout << "Conv: " << module_name << " - Nin: " << Nin << " Nout: " << Nout
    << " Kh: " << Kh << " Kw: " << Kw << " h: " << h << " w: " << w
    << " Sh: " << Stride_h << " Sw: " << Stride_w << " Ph: " << Pad_h
    << " Pw: " << Pad_w << endl;

  cout << "previous connections: " << layer->layer_param().bottom(0)
    << " of idx " << prev_connection_idx  << " allocate next connections: "
    << next_connection << " of idx " << layer_valid_.size()-1
    << endl;

  // append the channel buffer to the end of the current layer
  AppendChannelBuffer(net, layer_id, 0, append_buffer_capacity_);

  // update the output channel depth
  Nout_ = Nout;
  output_blob_idx_ = interconnections_to_idx_[layer->layer_param().top(0)];
}

/*
 * Implementation notes: Area
 * ---------------------------
 * Accumulate all the modules within the current accelerator.
 */
double ConvNetAcc::Area(int bit_width, int tech_node,
    ConfigParameter_MemoryType weight_memory_type) const {
  double total_area = 0.;
  // convolutional pe area
  for (vector<ConvLayerPe *>::const_iterator iter = conv_layer_pe_.begin();
      iter != conv_layer_pe_.end(); ++iter) {
    total_area += (*iter)->Area(bit_width, tech_node, weight_memory_type);
  }
  // pooling pe area
  for (vector<PoolLayerPe *>::const_iterator iter = pool_layer_pe_.begin();
      iter != pool_layer_pe_.end(); ++iter) {
    total_area += (*iter)->Area(bit_width, tech_node);
  }
  // no area model for the split pe & concat pe
  // can be added here

  // channel buffer area
  for (vector<ChannelBuffer *>::const_iterator iter = channel_buffer_.begin();
      iter != channel_buffer_.end(); ++iter) {
    total_area += (*iter)->Area(bit_width, tech_node);
  }

  return total_area;
}

/*
 * Implementation notes: clear
 * ----------------------------
 * It seems like SystemC can NOT handle the memory free for large port width. It
 * will cause a large time overhead. There will be some memory leakage issue
 * here for a fast speed.
 */
void ConvNetAcc::clear() {
  // remove all the data structures
  delete [] input_layer_data;
  delete [] output_layer_data;
  //for (size_t i = 0; i < conv_layer_pe_.size(); ++i) {
  //  delete conv_layer_pe_[i];
  //}
  conv_layer_pe_.clear();
  //for (size_t i = 0; i < pool_layer_pe_.size(); ++i) {
  //  delete pool_layer_pe_[i];
  //}
  pool_layer_pe_.clear();
  //for (size_t i = 0; i < split_layer_pe_.size(); ++i) {
  //  delete split_layer_pe_[i];
  //}
  split_layer_pe_.clear();
  //for (size_t i = 0; i < concat_layer_pe_.size(); ++i) {
  //  delete concat_layer_pe_[i];
  //}
  concat_layer_pe_.clear();
  //for (size_t i = 0; i < channel_buffer_.size(); ++i) {
  //  delete channel_buffer_[i];
  //}
  channel_buffer_.clear();

  //for (size_t i = 0; i < layer_valid_.size(); ++i) {
  //  delete layer_valid_[i];
  //}
  layer_valid_.clear();
  //for (size_t i = 0; i < layer_rdy_.size(); ++i) {
  //  delete layer_rdy_[i];
  //}
  layer_rdy_.clear();
  //for (size_t i = 0; i < layer_data_.size(); ++i) {
  //  delete [] layer_data_[i];
  //}
  layer_data_.clear();

  interconnections_to_idx_.clear();
  parallelism_.clear();
}
