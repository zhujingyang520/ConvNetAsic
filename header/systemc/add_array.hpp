/*
 * Filename: add_array.hpp
 * ------------------------
 * This file exports the class AddArray. It emulates the data path from the
 * output of the multiplier array going through the adders.
 */

#ifndef __ADD_ARRAY_HPP__
#define __ADD_ARRAY_HPP__

#include "header/systemc/data_type.hpp"
#include "header/systemc/models/adder_model.hpp"
#include <systemc.h>

class AddArray : public sc_module {
  // ports
  public:
    sc_in<bool> clock;
    sc_in<bool> reset;
    // add array enable
    sc_in<bool> add_array_enable;
    // add array input data valid
    sc_in<bool>* add_array_in_valid;
    // add array input data from mult array
    sc_in<Payload>* add_array_mult_in_data;
    // add array input data from registers (partial results)
    sc_in<Payload>* add_array_reg_in_data;
    // add array calculated results
    sc_out<Payload>* add_array_out_data;

    SC_HAS_PROCESS(AddArray);

  public:
    // constructor
    explicit AddArray(sc_module_name module_name, int Kh, int Kw, int Pin,
        int Pout, int bit_width=8, int tech_node=28, double clk_freq=1.);
    // destructor
    ~AddArray();

    // main process of the adder array
    void AddArrayProc();

    // area model of the adder array
    double Area() const;
    // power model of the adder array
    double StaticPower() const;
    double DynamicPower() const;
    double TotalPower() const;

  private:
    int Kh_, Kw_;     // spatial dimension of kernel
    int Pin_, Pout_;  // input parallelism & output parallelism
    AdderModel* adder_model_;
    double dynamic_energy_;
};

#endif
