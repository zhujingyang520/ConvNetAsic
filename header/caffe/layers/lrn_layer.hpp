/* Filename: lrn_layer.hpp
 * ------------------------
 * This file exports the class LRNLayer.
 */

#ifndef __LRN_LAYER_HPP__
#define __LRN_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class LRNLayer : public Layer {
  public:
    // Constructor
    explicit LRNLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~LRNLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);

  private:
    // local size of normalization
    int local_size_;
};

#endif
