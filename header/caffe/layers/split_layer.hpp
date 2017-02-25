/*
 * Filename: split_layer.hpp
 * --------------------------
 * This file exports the class SplitLayer, which is automatically inserted for
 * the split blobs (the blobs are used by multiple layers).
 */

#ifndef __SPLIT_LAYER_HPP__
#define __SPLIT_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class SplitLayer : public Layer {
  public:
    // Constructor
    explicit SplitLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~SplitLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);
};

#endif
