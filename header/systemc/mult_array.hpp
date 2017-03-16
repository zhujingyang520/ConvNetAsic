/*
 * Filename: mult_array.hpp
 * -------------------------
 * This file exports the class MultArray. It emulates the data path from the
 * outputs of line buffers going through the multipliers.
 */

#ifndef __MULT_ARRAY_HPP__
#define __MULT_ARRAY_HPP__

#include "header/systemc/data_type.hpp"
#include "header/systemc/models/mult_model.hpp"
#include <systemc.h>

class MultArray : public sc_module {
  // ports
  public:
    sc_in<bool> clock;
    sc_in<bool> reset;
    // multiplier enable (disable when stride or cross row)
    sc_in<bool> mult_array_en;
    // multiplier array input data
    sc_in<bool>* mult_array_in_valid;           // multiplier array input valid
    sc_in<int> mult_array_kernel_idx;           // multiplier array kernel index
    sc_in<Payload>* mult_array_act_in_data;     // feature map input
    sc_in<Payload>* mult_array_weight_in_data;  // weight input
    // multiplier array output data: of shape [Pout, Pin, Pk]
    sc_out<Payload>* mult_array_output_data;

    SC_HAS_PROCESS(MultArray);

  public:
    // constructor
    explicit MultArray(sc_module_name module_name, int Kh, int Kw, int Pin,
        int Pout, int Pk, int bit_width=8, int tech_node=28,
        double clk_freq=1.);
    // destructor
    ~MultArray();

    // main process of the multiplier array
    void MultArrayProc();

    // area model of the multiplier array
    double Area() const;
    // power model of the multiplier array
    double StaticPower() const;
    double DynamicPower() const;
    double TotalPower() const;

  private:
    int Kh_, Kw_;     // spatial dimension of kernel
    int Pin_, Pout_;  // input parallelism & output parallelism
    int Pk_;          // kernel parallelism
    // multipler model
    MultModel* mult_model_;
    double dynamic_energy_;
};

#endif
