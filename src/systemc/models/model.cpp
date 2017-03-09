/*
 * Filename: model.cpp
 * --------------------
 * This file implements the class Model.
 */

#include "header/systemc/models/model.hpp"

Model::Model(int tech_node, double clk_freq) {
  tech_node_ = tech_node;
  clk_freq_ = clk_freq;
}
