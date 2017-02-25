/*
 * Filename: input_layer.cpp
 * --------------------------
 * This file implements the class InputLayer.
 */

#include "header/caffe/layers/input_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: Setup
 * ----------------------------
 * The top shape of the input layer is purely defined by the definition in the
 * prototxt file.
 */
void InputLayer::SetUp(const vector<vector<int>* >& bottom_shape, 
    const vector<vector<int>* >& top_shape) {
  // read the InputParameter
  const InputParameter& param = this->layer_param_.input_param();
  const int num_shape = param.shape_size();

  // there is no parameters associated with InputLayer
  blobs_shape_.clear();

  // we only expect a single shape
  if (num_shape != 1) {
    cerr << "unexpected number of shape: " << num_shape << endl;
    exit(1);
  }

  // create the top shape vector containing (N, C, H, W)
  assert(top_shape.size() == 1);  // we only expect 1 top data blob
  top_shape[0]->clear();
  for (int i = 0; i < param.shape(0).dim_size(); ++i) {
    top_shape[0]->push_back(param.shape(0).dim(i));
  }
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * Input layer does NOT do any arithmetic computation.
 */
void InputLayer::ComputationComplexity(const vector<vector<int> *>&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
}

// Register InputLayer
REGISTER_LAYER_CLASS(Input);
