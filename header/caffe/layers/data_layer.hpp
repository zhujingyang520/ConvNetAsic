/*
 * Filename: data_layer.hpp
 * -------------------------
 * This file exports the class DataLayer.
 */


#ifndef __DATA_LAYER_HPP__
#define __DATA_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class DataLayer : public Layer {
  public:
    // Constructor
    explicit DataLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~DataLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);
};

#endif
