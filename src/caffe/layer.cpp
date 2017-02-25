/*
 * Filename: layer.cpp
 * --------------------
 * This file implements the class Layer.
 */

#include "header/caffe/layer.hpp"
#include <vector>

using std::vector;

int Layer::NumParameters() const {
  int num_params = 0;
  for (size_t i = 0; i < blobs_shape_.size(); ++i) {
    // shape of a single parameter
    const vector<int>& blob_shape = blobs_shape_[i];
    int p = 1;
    for (size_t j = 0; j < blob_shape.size(); ++j) {
      p *= blob_shape[j];
    }
    num_params += p;
  }

  return num_params;
}
