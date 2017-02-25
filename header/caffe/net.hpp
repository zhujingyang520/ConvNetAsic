/*
 * Filename: net.hpp
 * ------------------
 * This file exports the Net class, which consists of a graph of parsed network
 * prototxt.
 */

#ifndef __NET_HPP__
#define __NET_HPP__

#include "proto/caffe.pb.h"
#include "header/caffe/layer.hpp"
#include <string>
#include <vector>
#include <map>

class Net {
  // friend class of ConvNetAcc for easy access instance variables
  friend class ConvNetAcc;
  public:
    /*
     * Constructor: Net
     * Usage: Net network(param);
     *        Net network(param_file);
     * --------------------------------
     * Initialize the neural network from the network parameter, either from the
     * explicit settings of parameters or from the prototxt file.
     */
    explicit Net(const caffe::NetParameter& param);
    explicit Net(const std::string& param_file);

    // Destructor
    ~Net() {}

    // shorthand for Blob's shape
    typedef std::vector<int> BlobShape;

    // outputs the summary of the neural net
    void Summary() const;

    // outputs the summary of number of parameters in each layer
    void SummarizeNumParameters() const;

    // outputs the summary of number of operations taken in each layer
    void SummarizeNumOperations() const;

    /*
     * Method: TotalParameters, TotalOperations
     * -----------------------------------------
     * Get the total parameters & operations of the neural network.
     */
    inline long int TotalParameters() const {
      long int result = 0;
      for (size_t layer_id = 0; layer_id < layers_.size(); ++layer_id) {
        result += layers_[layer_id]->NumParameters();
      }
      return result;
    }

    inline Operation TotalOperations() const {
      Operation result;
      memset(&result, 0, sizeof(Operation));
      for (size_t layer_id = 0; layer_id < layers_.size(); ++layer_id) {
        result += layers_[layer_id]->GetOperation();
      }
      return result;
    }

  private:
    // Initialize a network with a netparameter
    void Init(const caffe::NetParameter& in_param);

    // Filter layers only in the test phase
    void FilterTestLayers(const caffe::NetParameter& param,
        caffe::NetParameter* param_filtered);

    // Insert the split layer to the network parameter
    void InsertSplits(const caffe::NetParameter& param,
        caffe::NetParameter* param_split);
    std::string SplitBlobName(const std::string& layer_name,
        const std::string& blob_name, const int blob_idx, const int split_idx);
    std::string SplitLayerName(const std::string& layer_name,
        const std::string& blob_name, const int blob_idx);
    void ConfigureSplitLayer(const std::string& layer_name,
        const std::string& blob_name, const int blob_idx, const int split_count,
        caffe::LayerParameter* split_layer_param);

    // Helper functions for Init
    // Append the bottom_id-th blob of layer_id-th layer in the neural net
    void AppendBottom(const caffe::NetParameter& param, const int layer_id,
        const int bottom_id);
    // Append the top_id-th blob of layer_id-th layer in the neural net
    void AppendTop(const caffe::NetParameter& param, const int layer_id,
        const int top_id);

  private:
    // network name
    std::string name_;
    // individual layer in the neural network
    std::vector<Layer *> layers_;
    // individual layer name in the neural network 
    std::vector<std::string> layers_name_;
    // inidividual blob (feature map) shape in the neural network
    std::vector<BlobShape *> blobs_shape_;
    // inidividual blob (feature map) name in the neural network
    std::vector<std::string> blobs_name_;

    // pointers for convenient access
    // bottom blobs shape pointer
    std::vector<std::vector<BlobShape *> > bottom_blobs_shape_ptr_;
    // top blobs shape pointer
    std::vector<std::vector<BlobShape *> > top_blobs_shape_ptr_;
    // track the pointers of layers for each blob
    //std::vector<Layer *> src_layers_ptr_;
    //std::vector<Layer *> dst_layers_ptr_;

    // blob name to index map
    std::map<std::string, int> blobs_name_to_idx_;

  public:
    /*
     * Method: MaxBlobShapeVolume, MinBlobShapeVolume
     * -----------------------------------------------
     * Returns the max or min volume of the blob shape of the intermediate 
     * feature map. The range of blob index can be specified from start index
     * (inclusive) to end index (exclusive).
     */
    int MaxBlobShapeVolume(int start_id, int end_id) const;
    int MinBlobShapeVolume(int start_id, int end_id) const;
    inline int MaxBlobShapeVolume() const
    { return MaxBlobShapeVolume(0, blobs_shape_.size()); }
    inline int MinBlobShapeVolume() const
    { return MinBlobShapeVolume(0, blobs_shape_.size()); }

  private:
    int BlobShapeVolume(const BlobShape& blob_shape) const;
};

#endif
