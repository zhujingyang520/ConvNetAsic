/*
 * Filename: batch_norm_layer.hpp
 * -------------------------------
 * This file exports the class BatchNormLayer.
 */

#ifndef __BATCH_NORM_LAYER_HPP__
#define __BATCH_NORM_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class BatchNormLayer : public Layer {
  public:
    // Constructor
    explicit BatchNormLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~BatchNormLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);

  private:
    // there is no additional information required for BatchNormLayer
};

#endif
