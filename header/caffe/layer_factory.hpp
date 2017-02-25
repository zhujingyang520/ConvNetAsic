/*
 * Filename: layer_factory.hpp
 * ----------------------------
 * This file exports the class LayerRegistry, which can simplify the
 * instantiation of new type of Layer.
 */

#ifndef __LAYER_FACTORY_HPP__
#define __LAYER_FACTORY_HPP__

#include "header/caffe/layer.hpp"
#include "proto/caffe.pb.h"
#include <string>
#include <map>
#include <iostream>

using namespace caffe;

/* Class: LayerRegistry
 * ---------------------
 * LayerRegistry is a static class, which can not be instantiated. It is mainly
 * used to register different types of Layers to the map.
 */
class LayerRegistry {
  public:
    // define the function pointer `Creator`, which takes the LayerParameter as
    // input and return the pointer to the Layer constructor
    typedef Layer* (*Creator)(const LayerParameter&);

    // define the map of function pointer (layer constructor)
    typedef std::map<std::string, Creator> CreatorRegistry;

    // Method: Registry for constructing the Registry map
    static CreatorRegistry& Registry() {
      static CreatorRegistry* g_registry_ = new CreatorRegistry();
      return *g_registry_;
    }

    // Adds a creator (a new type of Layer) to g_registry_
    static void AddCreator(const std::string& type, Creator creator) {
      CreatorRegistry& registry = Registry();
      if (registry.count(type) > 0) {
        std::cerr << "Layer type: " << type << " already registered."
          << std::endl;
        exit(1);
      }
      // insert the new Layer function into the map
      registry[type] = creator;
    }

    // Creates a new layer using the LayerParameter
    static Layer* CreateLayer(const LayerParameter& param) {
      const std::string& type = param.type();
      CreatorRegistry& registry = Registry();
      if (registry.count(type) == 0) {
        std::cerr << "Unknown layer type: " << type << std::endl;
        return NULL;
      } else {
        return registry[type](param);
      }
    }

  private:
    // private constructor: disable the instantiation of static class
    LayerRegistry() {}
};

// Use MACRO to simplfy the instantiation of new layer 
class LayerRegisterer {
  public:
    // Constructor: for one instantiation of LayerRegisterer, we will add one
    // Layer Creator to the g_registry_
    LayerRegisterer(const std::string& type, 
        Layer* (*creator)(const LayerParameter&)) {
      LayerRegistry::AddCreator(type, creator);
    }
};

#define REGISTER_LAYER_CREATOR(type, creator)                         \
  static LayerRegisterer g_creator_##type(#type, creator)

#define REGISTER_LAYER_CLASS(type)                                    \
  Layer* Creator_##type##Layer(const caffe::LayerParameter& param) {  \
    return new type##Layer(param);                                    \
  }                                                                   \
  REGISTER_LAYER_CREATOR(type, Creator_##type##Layer)

#endif
