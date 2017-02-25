/*
 * Filename: dropout_layer.hpp
 * ----------------------------
 * This file exports the class DropoutLayer.
 */

#ifndef __DROPOUT_LAYER_HPP__
#define __DROPOUT_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class DropoutLayer : public Layer {
  public:
    // Constructor
    explicit DropoutLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~DropoutLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);

  private:
    // there is no instance variables for the dropout layer
};

#endif
