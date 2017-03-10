/*
 * Filename: comparator_model.hpp
 * -------------------------------
 * This file exports the class ComparatorModel, which models the area, energy of
 * the general N-bit signed comparator.
 */

#ifndef __COMPARATOR_MODEL_HPP__
#define __COMPARATOR_MODEL_HPP__

#include "proto/config.pb.h"
#include "header/systemc/models/model.hpp"

class ComparatorModel : public Model {
  public:
    // constructor: provide the comparator bit width, technology node [nm]
    ComparatorModel(int bit_width, int tech_node=28, double clk_freq=1.);
    virtual ~ComparatorModel() {}

    // Area & POwer Metrics of the comparator
    // Return the Area of adder [um2]
    virtual double Area() const;
    // Return the static power of the comparator [uW]
    virtual double StaticPower() const;
    // Return the dynamic energy of one operation [uW]
    double DynamicEnergyOfOneOperation() const;

  private:
    // comparator bit width
    int bit_width_;

};

#endif
