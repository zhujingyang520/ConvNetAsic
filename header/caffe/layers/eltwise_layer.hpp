/*
 * Filename: eltwise_layer.hpp
 * ----------------------------
 * This file exports the class EltwiseLayer.
 */

#ifndef __ELTWISE_LAYER_HPP__
#define __ELTWISE_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class EltwiseLayer : public Layer {
  public:
    // Constructor
    explicit EltwiseLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~EltwiseLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);

  private:
    // there is no parameters associated with EltwiseLayer
};

#endif
