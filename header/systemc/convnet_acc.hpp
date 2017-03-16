/*
 * Filename: convnet_acc.hpp
 * --------------------------
 * This file exports the top module of the class of ConvNet Accelerator. It will
 * instantiate each block based on the Network architecture parsed from the
 * network prototxt file.
 */

#ifndef __CONVNET_ACC_HPP__
#define __CONVNET_ACC_HPP__

#include <systemc.h>
#include <vector>
#include <map>
#include <string>
#include "header/systemc/data_type.hpp"
#include "header/systemc/conv_layer_pe.hpp"
#include "header/systemc/pool_layer_pe.hpp"
#include "header/systemc/split_pe.hpp"
#include "header/systemc/concat_pe.hpp"
#include "header/systemc/channel_buffer.hpp"
#include "header/caffe/net.hpp"
#include "proto/config.pb.h"

class ConvNetAcc : public sc_module {
  friend class Top;
  // ports
  public:
    sc_in<bool> clock;
    sc_in<bool> reset;
    // data path of the first input layer
    sc_in<bool> input_layer_valid;
    sc_out<bool> input_layer_rdy;
    sc_in<Payload>* input_layer_data;
    // data path of the final output layer
    sc_in<bool> output_layer_rdy;
    sc_out<bool> output_layer_valid;
    sc_out<Payload>* output_layer_data;

    SC_HAS_PROCESS(ConvNetAcc);

  private:
    // internal modules
    // convolutional layer processing elements
    std::vector<ConvLayerPe *> conv_layer_pe_;
    // pooling layer processing elements
    std::vector<PoolLayerPe *> pool_layer_pe_;
    // split layer processing elements
    std::vector<SplitPe *> split_layer_pe_;
    // concatenation layer processing elements
    std::vector<ConcatPe *> concat_layer_pe_;
    // channel buffer unit
    std::vector<ChannelBuffer *> channel_buffer_;

    // internal connections
    std::vector< sc_signal<bool> * > layer_valid_;
    std::vector< sc_signal<bool> * > layer_rdy_;
    std::vector< sc_signal<Payload> * > layer_data_;

  public:
    int Nin_;             // input channel depth
    int input_blob_idx_;  // input connection blob index
    int Nout_;            // output channel depth
    int output_blob_idx_; // output connection blob index
    int append_buffer_capacity_;
    int input_spatial_dim_; // input spatial dimension
    int bit_width_;       // bit width of each number
    int tech_node_;       // technology node
    double clk_freq_;     // clock frequency [GHz]
    // weight (kernel) memory type
    config::ConfigParameter_MemoryType memory_type_;

  public:
    // constructor
    explicit ConvNetAcc(sc_module_name module_name, const Net& net,
        const config::ConfigParameter& config_param, sc_trace_file* tf=NULL);
    // destructor
    ~ConvNetAcc() { clear(); }

    void clear();

    // processing function of input & output layer connections
    void InputLayerConnections();
    void OutputLayerConnections();

    // area model of the accelerator
    double Area() const;
    // power model of the accelerator
    double StaticPower() const;
    double DynamicPower() const;
    double TotalPower() const;

    void InitParallelism(const Net& net, int pixel_inference_rate);
  private:
    // pipeline stage for within processing unit
    static const int pipeline_stage_ = 6;
    // helper function to set the parallelism input & parallelism output
    std::pair<std::pair<int, int>, int> CalculateParallelsim(int Nin, int Nout,
        int Kh, int Kw, int h, int w, int layer_inference_rate) const;
    // brute force search of the parallelism of the CONV
    std::pair<std::pair<int, int>, int> CalculateParallelsimBruteForce(int Nin,
        int Nout, int Kh, int Kw, double rate) const;

  private:
    // initialize the modules & interconnections based on the ConvNet
    // achitecture
    void Init(const Net& net);

    // helper function to instantiate the input layer
    // allocate the first connections
    void InitInputLayer(const Net& net, int layer_id);

    // helper function to instantiate the layer with layer_id as the
    // convolution processing element
    void InitConvolutionPe(const Net& net, int layer_id);

    // helper function to instantiate the layer with layer_id as the
    // pooling processing element
    void InitPoolingPe(const Net& net, int layer_id);

    // helper function to instantiate the layer with layer_id as the
    // fully-connected layer (computed by convolution processing element)
    void InitInnerProductLayer(const Net& net, int layer_id);

    // helper function to instantiate the layer with layer_id as the
    // split layer processing unit (split the handshake signals)
    void InitSplitLayer(const Net& net, int layer_id);

    // helper function to instantiate the layer with layer_id as the
    // concatenation layer (automatically append the channel buffer for each
    // path)
    void InitConcatLayer(const Net& net, int layer_id);

    // helper function to prepend the channel buffer to the bottom blob
    // it is utilized in the inception module (prepend in the SplitLayer)
    void PrependChannelBuffer(const Net& net, int layer_id, int blob_id,
        int capacity=INT_MAX);

    // helper function to append the channle buffer to the top blob
    // it is utilized between the layer to increase the performance
    void AppendChannelBuffer(const Net& net, int layer_id, int blob_id,
        int capacity=INT_MAX);

    // helper function bypass the trival layers, we will not consider such layer
    // in the hardware modeling
    void BypassLayer(const Net& net, int layer_id);

    // map of the interconnections to index, where key is the name of feature
    // map (blob in caffe), and value is the index of interconnections
    std::map<std::string, int> interconnections_to_idx_;

    // parallelism for each layer, key: layer index; value: <Pin, Pout, Pk>
    std::map<int, std::pair<std::pair<int, int>, int> > parallelism_;

    // trace file
    sc_trace_file* tf_;
};

#endif
