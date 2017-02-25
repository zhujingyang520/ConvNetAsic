/*
 * Filename: accuracy_layer.cpp
 * -----------------------------
 * This file implements the class AccuracyLayer.
 */

#include "header/caffe/layers/accuracy_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The output feature map of Accuracy is just a scalar. We only expect the top 
 * blob size is 1.
 */
void AccuracyLayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // we expect the bottom blobs size is 2, including prediction results & ground
  // truth label
  assert(bottom_shape.size() == 2);
  // we expect 2 bottom blobs have the same batch size
  assert(bottom_shape[0]->at(0) == bottom_shape[1]->at(0));
  // here we only expect top shape of 1
  assert(top_shape.size() == 1);

  // accuracy only produce the scalar for the accuracy
  top_shape[0]->clear();
  top_shape[0]->push_back(1);

  // no internal parameters
  blobs_shape_.clear();
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * Accuracy requires the compare operation. The number of comparisons is the
 * product of batch size and (no. output neurons + 1). We will compare the
 * max value of output neurons with the ground truth results.
 */
void AccuracyLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  // set all items to 0
  memset(&num_op_, 0, sizeof(num_op_));

  const int batch_size = bottom_shape[0]->at(0);
  const int num_output = bottom_shape[0]->at(1);
  num_op_.num_comp = batch_size * (num_output + 1);
}

// Register AccuracyLayer
REGISTER_LAYER_CLASS(Accuracy);
