/*
 * Filename: net.cpp
 * ------------------
 * This file implements the class Net.
 */

#include "header/caffe/net.hpp"
#include "header/caffe/util/upgrade_proto.hpp"
#include "header/caffe/layer_factory.hpp"
#include "proto/caffe.pb.h"
#include <string>
#include <iostream>
#include <climits>
#include <sstream>

using namespace std;
using namespace caffe;

Net::Net(const caffe::NetParameter& param) {
  Init(param);
}

Net::Net(const string& param_file) {
  NetParameter param;
  ReadNetParamsFromTextFileOrDie(param_file, &param);
  Init(param);
}

/*
 * Implementation notes: Init
 * ---------------------------
 * Initialize the data structure of neural network with a specified network
 * parameter.
 */
void Net::Init(const NetParameter& in_param) {
  // filter the layers not in the TEST phase (we only consider the inference
  // phase)
  NetParameter filtered_param;
  FilterTestLayers(in_param, &filtered_param);

  // Insert the split layer
  NetParameter param;
  InsertSplits(filtered_param, &param);

  // Build up all layers and their connections 
  name_ = param.name();

  // initialize the pointers 
  bottom_blobs_shape_ptr_.resize(param.layer_size());
  top_blobs_shape_ptr_.resize(param.layer_size());

  for (int layer_id = 0; layer_id < param.layer_size(); ++layer_id) {
    // push the current Layer into the vector
    const LayerParameter& layer_param = param.layer(layer_id);
    layers_.push_back(LayerRegistry::CreateLayer(layer_param));
    layers_name_.push_back(layer_param.name());

    // insert the bottom & top blobs
    for (int bottom_id = 0; bottom_id < layer_param.bottom_size(); 
        ++bottom_id) {
      AppendBottom(param, layer_id, bottom_id);
    }
    for (int top_id = 0; top_id < layer_param.top_size(); ++top_id) {
      AppendTop(param, layer_id, top_id);
    }
    // set up the current layer
    if (layers_[layer_id]) {
      // reshape the output feature map shape and internal parameters
      layers_[layer_id]->SetUp(bottom_blobs_shape_ptr_[layer_id],
          top_blobs_shape_ptr_[layer_id]);

      // calculate the computation complexity for each layer
      layers_[layer_id]->ComputationComplexity(
          bottom_blobs_shape_ptr_[layer_id], top_blobs_shape_ptr_[layer_id]);
    }

    // LOG info
    cout << "Layer[" << layer_id << "]: " << layer_param.name() 
      << " of type " << layer_param.type() << endl;
  }
}

/*
 * Implementation notes: AppendBottom
 * -----------------------------------
 * In this function, the bottom_blobs_shape_ptr_ of the specified layer id and
 * blob id will point to the corresponding blobs provided by its name.
 */
void Net::AppendBottom(const NetParameter& param, const int layer_id,
    const int bottom_id) {
  // access the layer parameter of the specified layer_id
  const LayerParameter& layer_param = param.layer(layer_id);
  // access the bottom blob name of the specified bottom_id
  const string& blob_name = layer_param.bottom(bottom_id);

  if (blobs_name_to_idx_.find(blob_name) == blobs_name_to_idx_.end()) {
    cerr << "unknown bottom blob '" << blob_name << "' (layer '" 
      << layer_param.name() << "', bottom index " << bottom_id << ")" << endl;
    exit(1);
  }

  const int blob_id = blobs_name_to_idx_[blob_name];
  // insert the corresponding Blob shape to the pointer
  bottom_blobs_shape_ptr_[layer_id].push_back(blobs_shape_[blob_id]);

  cout << "Layer[" << layer_id << "]: " << layer_param.name() << 
    " with bottom blob id " << blob_id << endl;
}

/*
 * Implementation notes: AppendTop
 * --------------------------------
 * In this function, the top_blobs_shape_ptr_ of the specified layer id and blob
 * id will point to the correponding blobs provided by its name. In particular,
 * the new blob shape will be allocated if it has not be defined previously.
 */
void Net::AppendTop(const NetParameter& param, const int layer_id,
    const int top_id) {
  // access the layer parameter of the specified layer_id
  const LayerParameter& layer_param = param.layer(layer_id);
  // access the top blob name of the specified top_id
  const string& blob_name = layer_param.top(top_id);

  // check if in-place computation, top blob name == bottom blob name
  if (layer_param.bottom_size() > top_id && 
      blob_name == layer_param.bottom(top_id)) {
    // NOT require to allocate new blob shape of top blob for the in-place 
    // computation
    const int blob_id = blobs_name_to_idx_[blob_name];
    top_blobs_shape_ptr_[layer_id].push_back(blobs_shape_[blob_id]);

    // LOG info
    cout << "In-place Layer[" << layer_id << "]: " << layer_param.name() <<
      " Top with blob id " << blob_id << endl;
  } else {
    // normal computation, allocate a new blob, and push the name index map
    const int blob_id = blobs_shape_.size();
    blobs_shape_.push_back(new BlobShape());
    blobs_name_.push_back(blob_name);
    blobs_name_to_idx_[blob_name] = blob_id;
    top_blobs_shape_ptr_[layer_id].push_back(blobs_shape_[blob_id]);

    cout << "Layer[" << layer_id << "]: " << layer_param.name() <<
      " with top blob id " << blob_id << endl;
  }
}

/*
 * Implementation notes: InsertSplits
 * -----------------------------------
 * Copy the network parameter from the original one. Do a feedforward pass
 * first, and record the number of blobs has been used. The insert the explicit
 * split layer to the network architecture.
 */
void Net::InsertSplits(const NetParameter& param,
    NetParameter* param_split) {
  // copy all the network parameters, excluding the layers
  param_split->CopyFrom(param);
  param_split->clear_layer();

  // useful data structure for book keeping
  // layer index to layer name
  map<int, string> layer_idx_to_layer_name;
  // blob name to top blob index (layer id, blob id)
  map<string, pair<int, int> > blob_name_to_last_top_idx;
  // map of the bottom blob index to the top blob index
  map<pair<int, int>, pair<int, int> > bottom_idx_to_source_top_idx;
  // the map counter of the top blob (split if greater than 1)
  map<pair<int, int>, int> top_idx_to_bottom_count;
  // record the split index during construction
  map<pair<int, int>, int> top_idx_to_bottom_split_idx;

  // iterate over the layer in the original network
  for (int i = 0; i < param.layer_size(); ++i) {
    const LayerParameter& layer_param = param.layer(i);
    layer_idx_to_layer_name[i] = layer_param.name();
    // iterate over the bottom blobs of the current layer
    for (int blob_id = 0; blob_id < layer_param.bottom_size(); ++blob_id) {
      const string& blob_name = layer_param.bottom(blob_id);
      if (blob_name_to_last_top_idx.find(blob_name) ==
          blob_name_to_last_top_idx.end()) {
        // the input prototxt file should follow the topology order
        cerr << "unknown bottom blob name: " << blob_name << endl;
        exit(1);
      }
      // bottom blob index & the associated top blob index
      const pair<int, int>& bottom_idx = make_pair(i, blob_id);
      const pair<int, int>& top_idx = blob_name_to_last_top_idx[blob_name];
      // record the bottom blob index to the top blob index
      bottom_idx_to_source_top_idx[bottom_idx] = top_idx;
      // increments the mapped number of top index
      ++top_idx_to_bottom_count[top_idx];
    }

    // iterate over the top blobs of the current layer
    for (int blob_id = 0; blob_id < layer_param.top_size(); ++blob_id) {
      const string& blob_name = layer_param.top(blob_id);
      // record the blob name to blob index map
      blob_name_to_last_top_idx[blob_name] = make_pair(i, blob_id);
    }
  }


  // iterate over the layers again
  for (int i = 0; i < param.layer_size(); ++i) {
    // insert the layer
    LayerParameter* layer_param = param_split->add_layer();
    layer_param->CopyFrom(param.layer(i));
    // check any shared bottom blobs with the split ones
    for (int blob_id = 0; blob_id < layer_param->bottom_size(); ++blob_id) {
      // associated top index of the current blob index
      const pair<int, int>& top_idx =
        bottom_idx_to_source_top_idx[make_pair(i, blob_id)];
      // split count of the associated top blob
      const int split_count = top_idx_to_bottom_count[top_idx];

      // rename the bottom blob name if it is larger than 1
      if (split_count > 1) {
        const string& layer_name = layer_idx_to_layer_name[top_idx.first];
        const string& blob_name = layer_param->bottom(blob_id);
        // rename the current blob of the current layer
        layer_param->set_bottom(blob_id, SplitBlobName(layer_name, blob_name,
              top_idx.second, top_idx_to_bottom_split_idx[top_idx]++));
      }
    }

    // insert split layer if the top blob has been mapped by more than once
    for (int blob_id = 0; blob_id < layer_param->top_size(); ++blob_id) {
      const pair<int, int>& top_idx = make_pair(i, blob_id);
      const int split_count = top_idx_to_bottom_count[top_idx];
      // insert the split layer
      if (split_count > 1) {
        const string& layer_name = layer_idx_to_layer_name[i];
        const string& blob_name = layer_param->top(blob_id);
        // insert the split layer and configure it
        LayerParameter* split_layer_param = param_split->add_layer();
        ConfigureSplitLayer(layer_name, blob_name, blob_id, split_count,
            split_layer_param);
      }
    }
  }
}

void Net::ConfigureSplitLayer(const string& layer_name, const string& blob_name,
    const int blob_idx, const int split_count,
    LayerParameter* split_layer_param) {
  split_layer_param->Clear();
  split_layer_param->add_bottom(blob_name);
  split_layer_param->set_name(SplitLayerName(layer_name, blob_name, blob_idx));
  split_layer_param->set_type("Split");
  // iteratively add the top blobs
  for (int k = 0; k < split_count; ++k) {
    split_layer_param->add_top(SplitBlobName(layer_name, blob_name, blob_idx,
          k));
  }
}

string Net::SplitBlobName(const string& layer_name, const string& blob_name,
    const int blob_idx, const int split_idx) {
  ostringstream split_blob_name;
  split_blob_name << blob_name << "_" << layer_name << "_" << blob_idx
    << "_split_" << split_idx;
  return split_blob_name.str();
}

string Net::SplitLayerName(const string& layer_name, const string& blob_name,
    const int blob_idx) {
  ostringstream split_layer_name;
  split_layer_name << blob_name << "_" << layer_name << "_" << blob_idx
    << "_split";
  return split_layer_name.str();
}

/*
 * Implementation notes: FilterTestLayers
 * ---------------------------------------
 * Copy the network parameter to the filtered network parameters. Only layers
 * in the TEST phase will be kept.
 */
void Net::FilterTestLayers(const NetParameter& param, 
    NetParameter* param_filtered) {
  // copy all the network parameters, excluding the layers 
  param_filtered->CopyFrom(param);
  param_filtered->clear_layer();

  // iterate over all layers in the original network 
  for (int i = 0; i < param.layer_size(); ++i) {
    const LayerParameter& layer_param = param.layer(i);
    bool layer_included = (layer_param.include_size() == 0);

    // check layer's include field
    for (int j = 0; j < layer_param.include_size(); ++j) {
      if (layer_param.include(j).has_phase() && 
          layer_param.include(j).phase() == caffe::TEST) {
        layer_included = true;
      }
    }

    // check layer's exclude filed 
    for (int j = 0; j < layer_param.exclude_size(); ++j) {
      if (layer_param.exclude(j).has_phase() && 
          layer_param.exclude(j).phase() == caffe::TEST) {
        layer_included = false;
      }
    }

    // added the layer only when it is in the TEST phase
    if (layer_included) {
      param_filtered->add_layer()->CopyFrom(layer_param);
    }
  }
}

void Net::Summary() const {
  cout << "####### SUMMARY ########" << endl;
  cout << "- Feature map (blob) summary" << endl;
  for (size_t blob_id = 0; blob_id < blobs_name_.size(); ++blob_id) {
    cout << "Blob[" << blob_id << "]: " << blobs_name_[blob_id] 
      << " of shape (";
    const vector<int>& cur_blob_shape = *blobs_shape_[blob_id];
    for (vector<int>::const_iterator iter = cur_blob_shape.begin();
        iter != cur_blob_shape.end(); ++iter) {
      cout << (*iter) << ", ";
    }
    cout << ")" << endl;
  }

  cout << "- Layer summary" << endl;
  for (size_t layer_id = 0; layer_id < layers_name_.size(); ++layer_id) {
    cout << "Layer[" << layer_id << "]: " << layers_name_[layer_id] << endl;
  }
}

void Net::SummarizeNumParameters() const {
  cout << "##### No. parameters ######" << endl;
  for (size_t layer_id = 0; layer_id < layers_name_.size(); ++layer_id) {
    cout << "Layer[" << layer_id << "]: " << layers_name_[layer_id]
      << " contains " << layers_[layer_id]->NumParameters() << endl;
  }
}

void Net::SummarizeNumOperations() const {
  cout << "#### No. Operations ######" << endl;
  for (size_t layer_id = 0; layer_id < layers_name_.size(); ++layer_id) {
    cout << "Layer[" << layer_id << "]: " << layers_name_[layer_id]
      << " no. op " << layers_[layer_id]->GetOperation() << endl;
  }
}

/*
 * Implementation notes: MaxBlobShapeVolume, MinBlobShapeVolume
 * -------------------------------------------------------------
 * Iterate over all the feature maps in the neural network, and return the max
 * or min of the volume (the product of each dimension).
 */
int Net::MaxBlobShapeVolume(int start_id, int end_id) const {
  // sanity check
  assert(start_id >= 0);
  assert(start_id < end_id);
  assert((size_t) end_id <= blobs_shape_.size());

  int max_volume = 0;
  size_t max_blob_id;
  for (size_t blob_id = start_id; blob_id < (size_t) end_id; ++blob_id) {
    if (BlobShapeVolume(*blobs_shape_[blob_id]) > max_volume) {
      max_volume = BlobShapeVolume(*blobs_shape_[blob_id]);
      max_blob_id = blob_id;
    }
  }
  cout << "Max Blob[" << max_blob_id << "] - " << blobs_name_[max_blob_id]
    << endl;
  return max_volume;
}

int Net::MinBlobShapeVolume(int start_id, int end_id) const {
  // sanity check
  assert(start_id >= 0);
  assert(start_id < end_id);
  assert((size_t) end_id <= blobs_shape_.size());

  int min_volume = INT_MAX;
  size_t min_blob_id;
  for (size_t blob_id = start_id; blob_id < (size_t) end_id; ++blob_id) {
    if (BlobShapeVolume(*blobs_shape_[blob_id]) < min_volume) {
      min_volume = BlobShapeVolume(*blobs_shape_[blob_id]);
      min_blob_id = blob_id;
    }
  }
  cout << "Min Blob[" << min_blob_id << "] - " << blobs_name_[min_blob_id]
    << endl;
  return min_volume;
}

/*
 * Helper function to calculate the volume of the blob shape.
 */
int Net::BlobShapeVolume(const BlobShape& blob_shape) const {
  if (blob_shape.size() == 0) {
    return 0;
  }
  int volume = 1;
  for(BlobShape::const_iterator iter = blob_shape.begin();
      iter != blob_shape.end(); ++iter) {
    volume *= *iter;
  }
  return volume;
}
