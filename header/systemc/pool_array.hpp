/*
 * Filename: pool_array.hpp
 * -------------------------
 * This file exports the class PoolArray. It does the arithmetic computation of
 * Pooling layer (MAX or AVG).
 */

#ifndef __POOL_ARRAY_HPP__
#define __POOL_ARRAY_HPP__

#include "header/systemc/data_type.hpp"
#include "header/systemc/models/adder_model.hpp"
#include <systemc.h>

class PoolArray : public sc_module {
  // ports
  public:
    sc_in<bool> clock;
    sc_in<bool> reset;

    // pool array enable
    sc_in<bool> pool_array_en;
    // pool array input valid (invalid when non-divisible for parallelism)
    sc_in<bool>* pool_array_in_valid;
    // pool array input data
    sc_in<Payload>* pool_array_in_data;
    // pool array output data
    sc_out<Payload>* pool_array_out_data;

    SC_HAS_PROCESS(PoolArray);

    // Pool Methdod: MAX or AVG
    enum PoolMethod {
      MAX, AVG
    };

    // area model of the pool array
    double Area() const;
    // power model of the pool array
    double StaticPower() const;
    double DynamicPower() const;
    double TotalPower() const;

  public:
    // constructor
    explicit PoolArray(sc_module_name module_name, int Kh, int Kw, int Pin,
        PoolMethod pool_method=MAX, int bit_width=8, int tech_node=28);
    // destructor
    ~PoolArray();

    // main process of the PoolArray
    void PoolArrayProc();

  private:
    int Pin_;                 // input parallelism
    int Kh_, Kw_;             // spatial dimension of kernel
    PoolMethod pool_method_;  // pool method: max or avg
    // adder model
    AdderModel* adder_model_;
    double dynamic_energy_;
};

#endif
