/*
 * Filename: pooling_layer.hpp
 * ----------------------------
 * This file exports the class PoolingLayer.
 */

#ifndef __POOLING_LAYER_HPP__
#define __POOLING_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class PoolingLayer : public Layer {
  friend class ConvNetAcc;
  public:
    // Constructor
    explicit PoolingLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~PoolingLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);

  private:
    // input feature map shape
    int batch_num_;
    int num_input_;
    int h_, w_;

    // pooling parameters
    // kernel shape
    int kh_, kw_;
    // pad shape
    int pad_h_, pad_w_;
    // stride shape
    int stride_h_, stride_w_;
    // pooling method (MAX, AVG)
    caffe::PoolingParameter::PoolMethod pool_method_;

    // inferred output feature map spatial dimension
    int out_h_, out_w_;
};


#endif
