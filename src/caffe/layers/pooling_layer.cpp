/*
 * Filename: pooling_layer.cpp
 * ----------------------------
 * This file implements the class PoolingLayer.
 */

#include "header/caffe/layers/pooling_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>
#include <math.h>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The top shape of the pooling layer is inferred from the pooling parameter. The
 * bottom shape of the PoolingLayer should only contain a single blob, and the
 * shape of the blob is 4D, i.e. (N, C, H, W).
 */
void PoolingLayer::SetUp(const vector<vector<int>* >& bottom_shape,
    const vector<vector<int>* >& top_shape) {
  // read the pooling parameter
  PoolingParameter pool_param = layer_param_.pooling_param();

  // sanity check: the bottom shape should contain single blob & the shape of
  // blob is of 4D (N, C, H, W)
  assert(bottom_shape.size() == 1);
  assert(bottom_shape[0]->size() == 4);
  assert(top_shape.size() == 1);

  // global pooling: set the kh_ & kw_ to bottom shape
  if (pool_param.has_global_pooling()) {
    // sanity check: no kernel definition for global pooling
    assert(pool_param.has_kernel_size() == 0 &&
        pool_param.has_kernel_h() == 0 && pool_param.has_kernel_w() == 0);
  } else {
    // we should define 1 kernel size or 2 kernel spec for h & w respectively
    // not both (XOR)
    assert(pool_param.has_kernel_size() ^
        (pool_param.has_kernel_h() && pool_param.has_kernel_w()));
  }

  // parse the input feature map dimension
  batch_num_ = bottom_shape[0]->at(0);
  num_input_ = bottom_shape[0]->at(1);
  h_ = bottom_shape[0]->at(2);
  w_ = bottom_shape[0]->at(3);

  // parse the kernel shape
  if (pool_param.has_global_pooling()) {
    kh_ = h_;
    kw_ = w_;
  } else {
    if (pool_param.has_kernel_size()) {
      kw_ = kh_ = pool_param.kernel_size();
    } else {
      kh_ = pool_param.kernel_h();
      kw_ = pool_param.kernel_w();
    }
  }
  // parse the stride shape
  if (!pool_param.has_stride_h()) {
    stride_h_ = stride_w_ = pool_param.stride();
  } else {
    stride_h_ = pool_param.stride_h();
    stride_w_ = pool_param.stride_w();
  }
  // parse the pad shape
  if (!pool_param.has_pad_h()) {
    pad_h_ = pad_w_ = pool_param.pad();
  } else {
    pad_h_ = pool_param.pad_h();
    pad_w_ = pool_param.pad_w();
  }
  // parse the pooling method
  pool_method_ = pool_param.pool();
  assert(pool_method_ != PoolingParameter_PoolMethod_STOCHASTIC);

  // sanity check for global pooling, stride must be 1 and pad must be 0
  if (pool_param.has_global_pooling()) {
    assert(pad_h_ == 0 && pad_w_ == 0 && stride_h_ == 1 && stride_w_ == 1);
  }

  // infer the output feature map dimension (unlike FLOOR in CONV, POOL uses
  // CEIL to round the floating number to integer)
  out_h_ = static_cast<int>(ceil(static_cast<float>(h_ - kh_ + 2*pad_h_) /
        stride_h_)) + 1;
  out_w_ = static_cast<int>(ceil(static_cast<float>(w_ - kw_ + 2*pad_w_) /
        stride_w_)) + 1;

  // top shape: (batch, out, h, w)
  top_shape[0]->clear();
  top_shape[0]->push_back(batch_num_);
  top_shape[0]->push_back(num_input_);
  top_shape[0]->push_back(out_h_);
  top_shape[0]->push_back(out_w_);

  // there is no parameters associated with pooling layer
  blobs_shape_.clear();
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * For average pooling, each output feature map takes kernel_h * kernel_w add
 * operations. For max pooling, each output feature map takes kernel_h *
 * kernel_w compare operations.
 */
void PoolingLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
  switch (pool_method_) {
    case PoolingParameter_PoolMethod_MAX:
      num_op_.num_comp = num_input_ * out_h_ * out_w_ * kh_ * kw_;
      break;
    case PoolingParameter_PoolMethod_AVE:
      num_op_.num_add = num_input_ * out_h_ * out_w_ * kh_ * kw_;
      break;
    default:
      cerr << "unexpected pooling method" << endl;
      exit(1);
  }
}

// Register PoolingLayer
REGISTER_LAYER_CLASS(Pooling);
