/*
 * Filename: model.hpp
 * --------------------
 * Abstract class for hardware modeling.
 */

#ifndef __MODEL_HPP__
#define __MODEL_HPP__

class Model {
  public:
    // constructor
    Model(int tech_node, double clk_freq=1.);
    virtual ~Model() {}

    // Area model of the hardware
    virtual double Area() const = 0;
    // Static power model of the hardware
    virtual double StaticPower() const = 0;

    inline int tech_node() const { return tech_node_; }
    inline double clk_freq() const { return clk_freq_; }

  protected:
    // technology node [nm]
    int tech_node_;
    // clock frequency [GHz]
    double clk_freq_;
};

#endif
