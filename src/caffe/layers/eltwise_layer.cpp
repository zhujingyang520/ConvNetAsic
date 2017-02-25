/*
 * Filename: eltwise_layer.cpp
 * ----------------------------
 * This file implements the class EltwiseLayer.
 */

#include "header/caffe/layers/eltwise_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The output feature map of eltwise layer is same as the input feature map.
 */
void EltwiseLayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // we only expect multiple input feature maps and single output feature map
  assert(bottom_shape.size() > 1);
  assert(top_shape.size() == 1);
  // only summation
  assert(layer_param_.eltwise_param().operation() ==
      EltwiseParameter_EltwiseOp_SUM);

  // check the input blobs have the same shape with each other
  for (size_t i = 1; i < bottom_shape.size(); ++i) {
    for (size_t j = 0; j < bottom_shape[i]->size(); ++j) {
      assert(bottom_shape[i]->at(j) == bottom_shape[0]->at(j));
    }
  }

  // allocate the shape of output feature map
  top_shape[0]->clear();
  for (size_t j = 0; j < bottom_shape[0]->size(); ++j) {
    top_shape[0]->push_back(bottom_shape[0]->at(j));
  }

  // there is no internal parameters
  blobs_shape_.clear();
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * The EltwiseLayer takes 1 add per pixel in the input feature map.
 */
void EltwiseLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
  // 1 ADD per pixel
  int volume = 1;
  for (vector<int>::const_iterator iter = bottom_shape[0]->begin();
      iter != bottom_shape[0]->end(); ++iter) {
    volume *= *iter;
  }
  num_op_.num_add = volume;
}

// Register EltwiseLayer
REGISTER_LAYER_CLASS(Eltwise);
