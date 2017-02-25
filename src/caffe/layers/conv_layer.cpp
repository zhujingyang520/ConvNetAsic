/*
 * Filename: conv_layer.cpp
 * -------------------------
 * This file implements the class ConvolutionLayer.
 */

#include "header/caffe/layers/conv_layer.hpp"
#include "proto/caffe.pb.h"
#include "header/caffe/layer_factory.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace caffe;

/*
 * Implementation notes: SetUp
 * ----------------------------
 * The top shape of the convolution layer is inferred from the convolutional
 * parameter. Here we use a simplification of the original caffe framework
 * because we are not intended to target for a general CONV operation.
 */
void ConvolutionLayer::SetUp(const vector<vector<int>* >& bottom_shape,
        const vector<vector<int>* >& top_shape) {
  // read the convolution parameter
  ConvolutionParameter conv_param = layer_param_.convolution_param();

  // parse the output feature map number
  num_output_ = conv_param.num_output();

  // parse the kernel shape
  if (conv_param.has_kernel_h() || conv_param.has_kernel_w()) {
    kh_ = conv_param.kernel_h();
    kw_ = conv_param.kernel_w();
  } else {
    // we only expect a single definition of kernel size
    assert(conv_param.kernel_size_size() == 1);
    kh_ = kw_ = conv_param.kernel_size(0);
  }

  // parse the stride shape
  if (conv_param.has_stride_h() || conv_param.has_stride_w()) {
    stride_h_ = conv_param.stride_h();
    stride_w_ = conv_param.stride_w();
  } else {
    // we only expect a single or default (0) definition of kernel size
    const int num_stride_dims = conv_param.stride_size();
    const int DefaultStride = 1;
    assert(num_stride_dims == 1 || num_stride_dims == 0);
    stride_h_ = stride_w_ = (num_stride_dims == 0) ? DefaultStride : 
      conv_param.stride(0);
  }

  // parse the pad shape
  if (conv_param.has_pad_h() || conv_param.has_pad_w()) {
    pad_h_ = conv_param.pad_h();
    pad_w_ = conv_param.pad_w();
  } else {
    // we only expect a single or default (0) definition of pad size
    const int num_pad_dims = conv_param.pad_size();
    const int DefaultPad = 0;
    assert(num_pad_dims == 1 || num_pad_dims == 0);
    pad_h_ = pad_w_ = (num_pad_dims == 0) ? DefaultPad : conv_param.pad(0);
  }

  // parse the group (only appears in AlexNet, CaffeNet)
  // due to the limitation of GPU memory at that time
  group_ = conv_param.group();

  // parse the bias term
  bias_term_ = conv_param.bias_term();

  // obtain the input feature map dimension
  // we only consider the simple case where the no. of bottom blob is 1
  // and the dimension of the input feature map is of shape (N, C, H, W)
  assert(bottom_shape.size() == 1);
  assert(bottom_shape[0]->size() == 4);
  batch_num_ = bottom_shape[0]->at(0);
  num_input_ = bottom_shape[0]->at(1);
  h_ = bottom_shape[0]->at(2);
  w_ = bottom_shape[0]->at(3);

  // based on parsed parameters and input feature maps, we can calculate the
  // dimension of the output feature map
  out_h_ = (h_ - kh_ + 2*pad_h_) / stride_h_ + 1;
  out_w_ = (w_ - kw_ + 2*pad_w_) / stride_w_ + 1;
  // we only expect top blob contain 1 blob
  assert(top_shape.size() == 1);
  top_shape[0]->clear();
  // (N, C, H, W)
  top_shape[0]->push_back(batch_num_);
  top_shape[0]->push_back(num_output_);
  top_shape[0]->push_back(out_h_);
  top_shape[0]->push_back(out_w_);

  // initialize the kernel parameters
  if (bias_term_) {
    cout << "num_output_: " << num_output_ << " num_input_: " << num_input_
      << " group: " << group_ << " kh_: " << kh_ << " kw_: " << kw_ << endl;
    // contain the biases in CONV
    blobs_shape_.resize(2);
    // weight shape: (Nout, Nin, Kh, Kw)
    blobs_shape_[0].clear();
    blobs_shape_[0].push_back(num_output_);
    // the effective number of input channels is reduced by group_
    blobs_shape_[0].push_back(num_input_/group_);
    blobs_shape_[0].push_back(kh_);
    blobs_shape_[0].push_back(kw_);
    // bias shape: (Nout,)
    blobs_shape_[1].clear();
    blobs_shape_[1].push_back(num_output_);
  } else {
    // not contain the biases in CONV
    blobs_shape_.resize(1);
    // weight shape: (Nout, Nin, Kh, Kw)
    blobs_shape_[0].clear();
    blobs_shape_[0].push_back(num_output_);
    // the effective number of input channels is reduced by group_
    blobs_shape_[0].push_back(num_input_/group_);
    blobs_shape_[0].push_back(kh_);
    blobs_shape_[0].push_back(kw_);
  }
}

/*
 * Implementation notes: ComputationComplexity
 * --------------------------------------------
 * Convolutional layer takes up the MAC operation, where the number of MAC
 * depends on the kernel dimension and output feature map dimension.
 */
void ConvolutionLayer::ComputationComplexity(const vector<vector<int>* >&
    bottom_shape, const vector<vector<int>* >& top_shape) {
  memset(&num_op_, 0, sizeof(num_op_));
  num_op_.num_mac = batch_num_ * kh_ * kw_ * (num_input_ / group_) *
    num_output_ * out_h_ * out_w_;
}

// Register ConvolutionLayer
REGISTER_LAYER_CLASS(Convolution);
