/*
 * Filename: scale_layer.cpp
 * --------------------------
 * This file implements the class ScaleLayer.
 */

#include "header/caffe/layers/scale_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The output feature map of Scale layer keeps the same dimension as the input
 * feature map. It contains the scalar and the bias (optional) in this layer.
 */
void ScaleLayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // we only expect a single input & output feature map
  assert(bottom_shape.size() == 1);
  assert(top_shape.size() == 1);

  // read the parameter
  const ScaleParameter& param = layer_param_.scale_param();
  bias_term_ = param.bias_term();
  // only the simplest case, where the scaling is conducting over channel axis
  assert(param.axis() == 1 && param.num_axes() == 1);

  // reshape the output blob shape for non in-place optimization
  if (bottom_shape[0] != top_shape[0]) {
    top_shape[0]->clear();
    *top_shape[0] = *bottom_shape[0];
  }

  // scalar or bias (optional)
  const int channel_axis = 1;
  if (bias_term_) {
    blobs_shape_.resize(2);
    blobs_shape_[0].clear();
    blobs_shape_[0].push_back(bottom_shape[0]->at(channel_axis));
    blobs_shape_[1].clear();
    blobs_shape_[1].push_back(bottom_shape[0]->at(channel_axis));
  } else {
    blobs_shape_.resize(1);
    blobs_shape_[0].clear();
    blobs_shape_[0].push_back(bottom_shape[0]->at(channel_axis));
  }
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * The ScaleLayer takes one MAC per pixel in the input feature map.
 */
void ScaleLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
  // 1 MAC per pixel
  int volume = 1;
  for (vector<int>::const_iterator iter = bottom_shape[0]->begin();
      iter != bottom_shape[0]->end(); ++iter) {
    volume *= *iter;
  }
  num_op_.num_mac = volume;
}

// Register ScaleLayer
REGISTER_LAYER_CLASS(Scale);
