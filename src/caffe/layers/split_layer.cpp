/*
 * Filename: split_layer.cpp
 * --------------------------
 * This file implements the class SplitLayer.
 */

#include "header/caffe/layers/split_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The top shape of each blob is the same as the bottom shape.
 */
void SplitLayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // sanity check: single bottom blob & multiple top blobs
  assert(bottom_shape.size() == 1);
  assert(top_shape.size() > 1);

  // top blobs have the same shape as the bottom one
  for (size_t blob_id = 0; blob_id < top_shape.size(); ++blob_id) {
    // we do NOT allow the in-place split layer, it is guaranteed by the
    // insertSplit routine
    assert(top_shape[blob_id] != bottom_shape[0]);

    top_shape[blob_id]->clear();
    *top_shape[blob_id] = *bottom_shape[0];
  }

  // there is no additional parameters for SplitLayer
  blobs_shape_.clear();
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * There is no computation associated with the SplitLayer.
 */
void SplitLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
}

// Register SplitLayer
REGISTER_LAYER_CLASS(Split);
