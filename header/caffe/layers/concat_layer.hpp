/*
 * Filename: concat_layer.hpp
 * ---------------------------
 * This file exports the class ConcatLayer, which is commonly used in Inception
 * Module of neural network. It will concatenate several blobs into a single
 * blobs with along with a specified axis.
 */

#ifndef __CONCAT_LAYER_HPP__
#define __CONCAT_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class ConcatLayer : public Layer {
  public:
    // Constructor
    explicit ConcatLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~ConcatLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);

  private:
    // there is no additional parameters required for ConcatLayer
};

#endif
