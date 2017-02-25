/*
 * Filename: scale_layer.hpp
 * --------------------------
 * This file exports the class ScaleLayer.
 */

#ifndef __SCALE_LAYER_HPP__
#define __SCALE_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class ScaleLayer : public Layer {
  public:
    // Constructor
    explicit ScaleLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~ScaleLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);

  private:
    // the scale layer contains bias
    bool bias_term_;
};

#endif
