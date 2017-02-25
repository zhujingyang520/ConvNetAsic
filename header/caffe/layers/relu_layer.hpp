/*
 * Filename: relu_layer.hpp
 * -------------------------
 * This file exports the class ReLULayer.
 */

#ifndef __RELU_LAYER_HPP__
#define __RELU_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class ReLULayer : public Layer {
  public:
    // Constructor
    explicit ReLULayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~ReLULayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int> *>& top_shape);

  private:
    // there is no additional information required for ReLU layer
};

#endif
