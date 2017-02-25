/*
 * Filename: conv_layer.hpp
 * -------------------------
 * This file exports the class ConvolutionLayer.
 */

#ifndef __CONV_LAYER_HPP__
#define __CONV_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class ConvolutionLayer : public Layer {
  friend class ConvNetAcc;
  public:
    // Constructor
    explicit ConvolutionLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~ConvolutionLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >& 
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);

  private:
    // instance variables for the basic convolution operation
    // parameters of the input feature map, where the shape is (N, C, H, W)
    int batch_num_;
    int num_input_;
    int h_, w_;

    // number of output feature maps (kernels)
    int num_output_;
    // kernel shape
    int kh_, kw_;
    // stride shape
    int stride_h_, stride_w_;
    // pad shape
    int pad_h_, pad_w_;
    // group
    int group_;
    // bias term (flag for whether containing biases, CONV may do NOT require to
    // have the bias term when using batch normalization)
    bool bias_term_;

    // inferred output feature map spatial dimension
    int out_h_, out_w_;
};

#endif
