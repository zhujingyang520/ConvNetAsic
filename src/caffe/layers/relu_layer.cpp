/*
 * Filename: relu_layer.cpp
 * -------------------------
 * This file implements the class ReLULayer.
 */

#include "header/caffe/layers/relu_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The output feature map (top) shape of ReLU layer keeps the same dimension as
 * the input feature map (bottom). In addition, there is not any parameter
 * asscociated with ReLU layer.
 */
void ReLULayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // we only accept the ReLU layer with single bottom blob shape
  assert(bottom_shape.size() == 1);
  assert(top_shape.size() == 1);
  // non-zero blob shape
  assert(bottom_shape[0]->size());

  // reshape the top_shape to have the same shape as bottom_shape
  // we only allocate the new shape for non in-place computation
  if (top_shape[0] != bottom_shape[0]) {
    top_shape[0]->clear();
    *top_shape[0] = *bottom_shape[0];
  }

  // there is no internal parameters assciated with ReLU
  blobs_shape_.clear();
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * ReLU layer only contains the compare opeartion, where the number of
 * comparison is the same as the input feature map.
 */
void ReLULayer::ComputationComplexity(const vector<vector<int>* >& 
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
  num_op_.num_comp = 1;
  // sanity check, we only accept ReLU layer with single bottom blob shape
  assert(bottom_shape.size() == 1);
  for (vector<int>::const_iterator iter = bottom_shape[0]->begin();
      iter != bottom_shape[0]->end(); ++iter) {
    num_op_.num_comp *= *iter;
  }
}

// Register ReLULayer
REGISTER_LAYER_CLASS(ReLU);
