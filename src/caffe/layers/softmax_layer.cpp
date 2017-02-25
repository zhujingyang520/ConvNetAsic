/*
 * Filename: softmax_layer.cpp
 * ----------------------------
 * This file implements the class SoftmaxLayer.
 */

#include "header/caffe/layers/softmax_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The output feature map of Softmax keeps the same dimension as the input
 * feature map. There is no parameters associated with SoftmaxLayer.
 */
void SoftmaxLayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // we only expect the single bottom and top blob
  assert(bottom_shape.size() == 1);
  assert(top_shape.size() == 1);

  // we only expect the axis to be evaluated for Softmax = 1
  assert(layer_param_.softmax_param().axis() == 1);

  // same shape for output feature map & input feature map
  top_shape[0]->clear();
  *top_shape[0] = *bottom_shape[0];

  // no internal parameters
  blobs_shape_.clear();
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * Softmax requires the exp, div, and add, where the number of operations is the
 * number of neurons in the feature map.
 */
void SoftmaxLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  // set all items to 0
  memset(&num_op_, 0, sizeof(num_op_));

  // obtain the batch size from the input feature map
  const int batch_size = bottom_shape[0]->at(0);
  // for each axis, we will operate a softmax operation
  for (size_t i = 1; i < bottom_shape[0]->size(); ++i) {
    num_op_.num_add += batch_size * bottom_shape[0]->at(i);
    num_op_.num_div += batch_size * bottom_shape[0]->at(i);
    num_op_.num_exp += batch_size * bottom_shape[0]->at(i);
  }
}

// Register SoftmaxLayer
REGISTER_LAYER_CLASS(Softmax);
