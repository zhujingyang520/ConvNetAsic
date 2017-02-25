/*
 * Filename: softmax_loss_layer.hpp
 * ---------------------------------
 * This file exports class SoftmaxWithLossLayer.
 */

#ifndef __SOFTMAX_LOSS_HPP__
#define __SOFTMAX_LOSS_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class SoftmaxWithLossLayer : public Layer {
  public:
    // constructor
    explicit SoftmaxWithLossLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~SoftmaxWithLossLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);

  private:
    // there is no additional information required for SoftmaxWithLossLayer
};

#endif
