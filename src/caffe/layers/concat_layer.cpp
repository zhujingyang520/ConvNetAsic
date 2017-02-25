/*
 * Filename: concat_layer.cpp
 * ---------------------------
 * This file implements the ConcatLayer.
 */

#include "header/caffe/layers/concat_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The top shape of the ConcatLayer is the concatenation version of input.
 */
void ConcatLayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // Read the concatenation parameter
  ConcatParameter concat_param = layer_param_.concat_param();

  // we only expect the axis == 1
  // no deprecated concat dim definition
  assert(concat_param.has_concat_dim() == false);
  // we only expect to concat over channel axis
  assert(concat_param.axis() == 1);

  // bottom should have multiple blobs & top should have single blob
  assert(bottom_shape.size() > 1);
  assert(top_shape.size() == 1);

  // allocate the top shape
  top_shape[0]->clear();
  *top_shape[0] = *bottom_shape[0];
  int concat_dim = 0;
  // sanity check: for bottom blobs shape, they should have the same shape of
  // batch size, height, and width
  for (size_t i = 0; i < bottom_shape.size(); ++i) {
    // 4D feature map
    assert(bottom_shape[i]->size() == 4);

    // check over all dimension except for concat dim (channel)
    for (size_t j = 0; j < bottom_shape[i]->size(); ++j) {
      if (j == static_cast<size_t>(concat_param.axis())) {
        // concat current dim
        concat_dim += bottom_shape[i]->at(j);
      } else {
        // check the remaining dimension share the same value
        assert(bottom_shape[i]->at(j) == top_shape[0]->at(j));
      }
    }
  }

  // resize the concatenation axis
  top_shape[0]->at(concat_param.axis()) = concat_dim;

  // there is no additional parameters for ConcatLayer
  blobs_shape_.clear();
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * There is no computation associated with ConcatLayer, because it only
 * re-organizes the memory layout into a desired format.
 */
void ConcatLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
}

// Register ConcatLayer
REGISTER_LAYER_CLASS(Concat);

