/*
 * Filename: inner_product_layer.cpp
 * ----------------------------------
 * This file implements the class InnerProductLayer.
 */

#include "header/caffe/layers/inner_product_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The top shape of the inner product layer is just batch number x no. outputs.
 * The bottom shape (input feature map) can be either a 4D or 2D blob, depending
 * on the preceding layer is from CONV or FC.
 */
void InnerProductLayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // read the inner product parameter
  InnerProductParameter inner_product_param = layer_param_.inner_product_param();
  // parse the number of output neurons
  num_output_ = inner_product_param.num_output();
  // parse the bias term
  bias_term_ = inner_product_param.bias_term();

  // we only expect the flatten axis starting from 1 (default value)
  assert(inner_product_param.axis() == 1);

  // we only expect to have the single blob input & output
  assert(bottom_shape.size() == 1);
  assert(top_shape.size() == 1);

  // parse the number of batches from the top shape
  batch_num_ = bottom_shape[0]->at(0);
  // infer the number of inputs from the top shape, flatten from dim 1
  num_input_ = 1;
  for (size_t i = 1; i < bottom_shape[0]->size(); ++i) {
    num_input_ *= bottom_shape[0]->at(i);
  }

  // top blob shape (batch size, output neuron no.)
  top_shape[0]->clear();
  top_shape[0]->push_back(batch_num_);
  top_shape[0]->push_back(num_output_);

  // allocate for the kernel parameters
  if (bias_term_) {
    // contain the bias in the inner product layer
    blobs_shape_.resize(2);
    // weight shape: (Nout, Nin)
    blobs_shape_[0].clear();
    blobs_shape_[0].push_back(num_output_);
    blobs_shape_[0].push_back(num_input_);
    // bias shape: (Nout,)
    blobs_shape_[1].clear();
    blobs_shape_[1].push_back(num_output_);
  } else {
    // not contain the biases in the inner product layer
    blobs_shape_[0].resize(1);
    // weight shape: (Nout, Nin)
    blobs_shape_[0].push_back(num_output_);
    blobs_shape_[0].push_back(num_input_);
  }
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * Inner product layer takes the MAC operation, where the number of MACs is the
 * product of batch number, input neuron no., and output neuron no.
 */
void InnerProductLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
  num_op_.num_mac = batch_num_ * num_input_ * num_output_;
}

// Register InnerProductLayer
REGISTER_LAYER_CLASS(InnerProduct);
