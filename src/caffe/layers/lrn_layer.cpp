/*
 * Filename: lrn_layer.cpp
 * ------------------------
 * This file implements the class LRNLayer.
 */

#include "header/caffe/layers/lrn_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The output feature map (top) shape of LRN should be the same as the input
 * dimension. Unlike the ReLULayer, LRNLayer can only accept a single blob with
 * the shape of 4D, i.e. (batch, channel, height, width).
 */
void LRNLayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // sanity check: single data blob & 4D feature map
  assert(bottom_shape.size() == 1);
  assert(top_shape.size() == 1);
  assert(bottom_shape[0]->size() == 4);
  // we only forcus on the common normalization accross channel
  assert(layer_param_.lrn_param().norm_region() == 
      LRNParameter_NormRegion_ACROSS_CHANNELS);

  // reshape the top_shape to have the same shape as bottom_shape
  // we only allocate the new shape for non in-place computation
  if (top_shape[0] != bottom_shape[0]) {
    top_shape[0]->clear();
    *top_shape[0] = *bottom_shape[0];
  }

  // parse the local size of LRN
  local_size_ = layer_param_.lrn_param().local_size();

  // there is no internal parameters associated with LRN
  blobs_shape_.clear();
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * The formular for LRN is:
 *    y = x / (k + alpha * sum(x^2))^beta
 *
 * The operation per each feature map computation is referred from Netscope:
 * 1. No. mac: local size
 * 2. No. exp: 1
 * 3. No. div: 2
 * 4. No. add: 1
 */
void LRNLayer::ComputationComplexity(const vector<vector<int>* >& 
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
  // no. of ops per pixel (according to Netscope)
  const int NumMacPerPixel = local_size_;
  const int NumExpPerPixel = 1;
  const int NumDivPerPixel = 2;
  const int NumAddPerPixel = 1;

  // sanity check
  assert(bottom_shape.size() == 1);
  assert(bottom_shape[0]->size() == 4);

  // volume: batch * channel * height * width
  const int output_volume = bottom_shape[0]->at(0) * bottom_shape[0]->at(1) *
    bottom_shape[0]->at(2) * bottom_shape[0]->at(3);

  // write statistics
  num_op_.num_mac = NumMacPerPixel * output_volume;
  num_op_.num_exp = NumExpPerPixel * output_volume;
  num_op_.num_div = NumDivPerPixel * output_volume;
  num_op_.num_add = NumAddPerPixel * output_volume;
}

// Register LRNLayer
REGISTER_LAYER_CLASS(LRN);
