/*
 * Filename: softmax_loss_layer.cpp
 * ---------------------------------
 * This file implements the class SoftmaxWithLossLayer.
 */

#include "header/caffe/layers/softmax_loss_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The loss layer output a scalar. There is no parameters associated with the
 * SoftmaxWithLossLayer.
 */
void SoftmaxWithLossLayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // the bottom_shape should be of 2 blobs, 1 for predicition results, and the
  // other is the ground truth label
  assert(bottom_shape.size() == 2);
  assert(top_shape.size() == 1);

  // we accepts the axis equals 1
  assert(layer_param_.softmax_param().axis() == 1);
  // the shape of batch size should be equal for 2 bottom shapes
  assert(bottom_shape[0]->at(0) == bottom_shape[1]->at(0));

  // loss layer outputs a scalar
  top_shape[0]->clear();
  top_shape[0]->push_back(1);

  // no internal parameters
  blobs_shape_.clear();
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * SoftmaxWithLossLayer requires exp, div, and add.
 */
void SoftmaxWithLossLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int> *>& top_shape) {
  // set all iterms to 0
  memset(&num_op_, 0, sizeof(num_op_));

  // obtain the batch size
  const int batch_size = bottom_shape[0]->at(0);
  // for each axis, we operate a softmax operation
  for (size_t i = 1; i < bottom_shape[0]->size(); ++i) {
    num_op_.num_add += batch_size * bottom_shape[0]->at(i);
    num_op_.num_div += batch_size * bottom_shape[0]->at(i);
    num_op_.num_exp += batch_size * bottom_shape[0]->at(i);
  }
}

// Register SoftmaxWithLossLayer
REGISTER_LAYER_CLASS(SoftmaxWithLoss);
