/*
 * Filename: accuracy_layer.hpp
 * -----------------------------
 * This file expors the class AccuracyLayer.
 */

#ifndef __ACCURACY_LAYER_HPP__
#define __ACCURACY_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class AccuracyLayer : public Layer {
  public:
    // Constructor
    explicit AccuracyLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~AccuracyLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);

  private:
    // there is no additional parameters associated with AccuracyLayer
};

#endif
