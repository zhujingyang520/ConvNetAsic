/*
 * Filename: batch_norm_layer.cpp
 * -------------------------------
 * This file implements the class BatchNormLayer.
 */

#include "header/caffe/layers/batch_norm_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The output feature map of batch normalization keeps the same dimension as the
 * input feature map. The Batch Normalization requires the storage of mean and
 * variance in the inference stage.
 */
void BatchNormLayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // we only accepts the BN layer have single input & output feature map
  assert(bottom_shape.size() == 1);
  assert(top_shape.size() == 1);

  // reshape the output blob shape for non in-place computation
  if (bottom_shape[0] != top_shape[0]) {
    top_shape[0]->clear();
    *top_shape[0] = *bottom_shape[0];
  }

  // there is 2 parameters associated with BN: mean & variance
  const int channel_axis = 1;
  blobs_shape_.resize(2);
  blobs_shape_[0].clear();
  blobs_shape_[0].push_back(bottom_shape[0]->at(channel_axis));
  blobs_shape_[1].clear();
  blobs_shape_[1].push_back(bottom_shape[0]->at(channel_axis));
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * The BatchNormLayer takes one additional & one division for each pixel in the
 * input feature map.
 */
void BatchNormLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
  // 1 add & 1 division per pixel
  int volume = 1;
  for (vector<int>::const_iterator iter = bottom_shape[0]->begin();
      iter != bottom_shape[0]->end(); ++iter) {
    volume *= *iter;
  }
  num_op_.num_add = volume;
  num_op_.num_div = volume;
}

// Register BatchNormLayer
REGISTER_LAYER_CLASS(BatchNorm);
