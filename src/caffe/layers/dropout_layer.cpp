/*
 * Filename: dropout_layer.cpp
 * ----------------------------
 * This file implements the class DropoutLayer.
 */

#include "header/caffe/layers/dropout_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The top feature map shares the same dimension as the bottom feature map. We
 * will only reshape the top shape when the bottom shape and top shape do NOT
 * share the same name (non in-place layer).
 */
void DropoutLayer::SetUp(const vector<vector<int> *>& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // we only expect dropout layer with single bottom and top blob
  assert(bottom_shape.size() == 1);
  assert(bottom_shape.size() == 1);

  // reshape the top shape if non in-place
  if (bottom_shape[0] != top_shape[0]) {
    top_shape[0]->clear();
    *top_shape[0] = *bottom_shape[0];
  }

  // there is no parameters associated with the dropout layer
  blobs_shape_.clear();
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * Dropout layer will not have any operations assciated withit. Ideally, it
 * would have a scaling operation (mult) per pixel. However, we only consider
 * the inference phase, where the dropout layer only bypass the input feature
 * map to the output feature map. This is so-called inversed dropout.
 */
void DropoutLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
  // for consistency with netscope, the dropout takes compare operations
  num_op_.num_comp = 1;
  for (vector<int>::const_iterator iter = bottom_shape[0]->begin();
      iter != bottom_shape[0]->end(); ++iter) {
    num_op_.num_comp *= *iter;
  }
}

// Register DropoutLayer
REGISTER_LAYER_CLASS(Dropout);
