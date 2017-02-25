/*
 * Filename: input_layer.hpp
 * --------------------------
 * This file exports the class InputLayer. It only represents the 1st input
 * layer of the neural network.
 */

#ifndef __INPUT_LAYER_HPP__
#define __INPUT_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class InputLayer : public Layer {
  public:
    // Constructor 
    explicit InputLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~InputLayer() {}

    // Method: SetUp, inherit from Layer
    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    // Method: ComputationComplexity, inherit from Layer
    void ComputationComplexity(const std::vector<std::vector<int>* >& 
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);
};

#endif
