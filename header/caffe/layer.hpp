/*
 * Filename: layer.hpp
 * --------------------
 * This file exports the abstract class Layer, containing the elementary
 * parameters and function definitions for different types of layers.
 */

#ifndef __LAYER_HPP__
#define __LAYER_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/statistics.hpp"
#include <string>
#include <vector>

class Layer {
  public:
    /* Constructor: Layer
     * Usage: Layer *layer = new Layer(param);
     * ----------------------------------------
     * Pass the layer parameter to the instance variables.
     */
    explicit Layer(const caffe::LayerParameter& param)
      : layer_param_(param) {}
    virtual ~Layer() {}

    /* 
     * Method: SetUp
     * Usage: layer->SetUp(bottom_shape, top_shape);
     * ----------------------------------------------
     * Abstract method to set up the parameters for individual layer type,
     * including:
     * a) Calculate the shape of parameters of the current layer.
     * b) Calculate the shape of top blobs (feature maps) of the current layer.
     */
    virtual void SetUp(const std::vector<std::vector<int>* >& bottom_shape, 
        const std::vector<std::vector<int>* >& top_shape) = 0;

    /*
     * Method: ComputationComplexity
     * ------------------------------
     * Calculate the statistics of computation complexity associated with each
     * layer.
     */
    virtual void ComputationComplexity(const std::vector<std::vector<int>* >&
        bottom_shape, const std::vector<std::vector<int>* >& top_shape) = 0;

    /*
     * Method: NumParameters
     * Usage: int num_params = layer->NumParameters();
     * ------------------------------------------------
     * Returns the number of parameters required to stored in the current layer.
     */
    int NumParameters() const;

    /*
     * Method: GetOperation
     * ---------------------
     * Get the number of operations in this layer.
     */
    inline const Operation& GetOperation() const {
      return num_op_;
    }

    /*
     * Method: layer_param
     * --------------------
     * Returns the layer parameters.
     */
    inline const caffe::LayerParameter layer_param() const {
      return layer_param_;
    }

  protected:
    // instance variables
    caffe::LayerParameter layer_param_;   // layer parameter

    // we are not intended to store the actual parameters in the layer, but just
    // the shapes of all parameters in a specific layer
    std::vector<std::vector<int> > blobs_shape_;

    // number of arithmetic operations associated with each layer
    Operation num_op_;
};

#endif
