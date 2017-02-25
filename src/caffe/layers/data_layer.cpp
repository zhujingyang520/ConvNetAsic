/*
 * Filename: data_layer.cpp
 * -------------------------
 * This file implements the class DataLayer.
 */

#include "header/caffe/layers/data_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * DataLayer only setup the shape of output feature map. It is the input layer
 * of the neural network.
 */
void DataLayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // read the DataParameter
  const DataParameter& param = layer_param_.data_param();
  const int batch_size = param.batch_size();
  const int crop_size = param.has_crop_size() ? param.crop_size() :
    layer_param_.transform_param().crop_size();

  // we assume the non-zero crop size
  assert(crop_size);

  // we assume the ImagNet dataset
  const int num_channel = 3;
  const int num_class = 1000;

  // DataLayer may have 2 data blobs: input data and labels
  assert(top_shape.size() == static_cast<size_t>(layer_param_.top_size()));

  // reshape the output feature map into (batch size, channel, h, w)
  top_shape[0]->clear();
  top_shape[0]->push_back(batch_size);
  top_shape[0]->push_back(num_channel);
  top_shape[0]->push_back(crop_size);
  top_shape[0]->push_back(crop_size);

  if (top_shape.size() == 2) {
    // label shape (batch size, class)
    top_shape[1]->clear();
    top_shape[1]->push_back(batch_size);
    top_shape[1]->push_back(num_class);
  }

  // no parameters associated with DataLayer
  blobs_shape_.clear();
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * DataLayer does NOT contain any computation.
 */
void DataLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
}

// Register DataLayer
REGISTER_LAYER_CLASS(Data);
