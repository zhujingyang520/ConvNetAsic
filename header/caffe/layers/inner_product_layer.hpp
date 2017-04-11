/*
 * Filename: inner_product_layer.hpp
 * ----------------------------------
 * This file exports the class InnerProductLayer.
 */

#ifndef __INNER_PRODUCT_LAYER_HPP__
#define __INNER_PRODUCT_LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <vector>

class InnerProductLayer : public Layer {
  friend class ConvNetAcc;
  friend class VerilogCompiler;
  public:
    // Constructor
    explicit InnerProductLayer(const caffe::LayerParameter& param)
      : Layer(param) {}
    ~InnerProductLayer() {}

    void SetUp(const std::vector<std::vector<int>* >& bottom_shape,
        const std::vector<std::vector<int>* >& top_shape);

    void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape);

  private:
    // for inner product layer, we only need to store the number of input
    // neurons & the number of output neurons
    // number of inputs
    int num_input_;
    // number of outputs
    int num_output_;
    // batch number
    int batch_num_;

    // bias term (flag for whether containing biases, FC may do NOT require to
    // have the bias term when using batch normalization)
    bool bias_term_;
};

#endif
