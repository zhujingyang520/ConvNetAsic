/*
 * Filename: softmax_layer.hpp
 * ----------------------------
 * This class exports the class SoftmaxLayer.
 */

#ifndef __SOFTMAX_LAYER_HPP__
#define __SOFTMAX_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class SoftmaxLayer : public Layer {
  public:
    // Constructor
    explicit SoftmaxLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~SoftmaxLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);

  private:
    // there is no additional information required for Softmax layer
};

#endif
