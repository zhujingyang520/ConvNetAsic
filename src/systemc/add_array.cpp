/*
 * Filename: add_array.cpp
 * -----------------------
 * This file implements the class AddArray.
 */

#include "header/systemc/add_array.hpp"
using namespace std;

/*
 * Implmentation notes: Constructor
 * ---------------------------------
 * Allocate the input & output data port of AddArray.
 */
AddArray::AddArray(sc_module_name module_name, int Kh, int Kw, int Pin,
    int Pout, int Pk, int bit_width, int tech_node, double clk_freq)
  : sc_module(module_name), Kh_(Kh), Kw_(Kw), Pin_(Pin), Pout_(Pout), Pk_(Pk) {
    // input valid no. = Pout
    add_array_in_valid = new sc_in<bool> [Pout_];
    // multiplier array input no. = Pout*Pin*Kh*Kw
    add_array_mult_in_data = new sc_in<Payload> [Pout_*Pin_*Pk_];
    // partial results registers no. = Pout
    add_array_reg_in_data = new sc_in<Payload> [Pout_];
    // add array results no. = Pout
    add_array_out_data = new sc_out<Payload> [Pout_];

    // initialize the adder model
    adder_model_ = new AdderModel(bit_width, tech_node, clk_freq);
    dynamic_energy_ = 0.;

    // AddArrayProc: synchronous with clock and reset
    SC_METHOD(AddArrayProc);
    sensitive << clock.pos() << reset;

    add_reg_ = new Payload [Pout_];
  }

/*
 * Implmentation notes: Destructor
 * --------------------------------
 * Free the dynamic allocated memory.
 */
AddArray::~AddArray() {
  delete [] add_array_in_valid;
  delete [] add_array_mult_in_data;
  delete [] add_array_reg_in_data;
  delete [] add_array_out_data;
  delete adder_model_;
  delete [] add_reg_;
}

/*
 * Implementation notes: AddArrayProc
 * -----------------------------------
 * Add the results from every multiplier array.
 */
void AddArray::AddArrayProc() {
  if (reset.read()) {
    for (int i = 0; i < Pout_; ++i) {
      add_array_out_data[i].write(Payload(0));
    }
  } else if (add_array_enable.read()) {
    // only do the calculation when the valid signal is asserted
    for (int o = 0; o < Pout_; ++o) {
      if (add_array_in_valid[o].read()) {
        // adds the dynamic energy of one operation
        // there are Pin*Pk adders
        dynamic_energy_ += adder_model_->DynamicEnergyOfOneOperation()*Pin_*Pk_;
#ifdef DATA_PATH
        // summing the Pin kernel activation results & partial results
        Payload result;
        // add over Pin Kh*Kw kernels
        for (int i = 0; i < Pin_; ++i) {
          for (int k = 0; k < Pk_; ++k) {
            result = result + add_array_mult_in_data[o*Pin_*Pk_+i*Pk_+k].read();
          }
        }
        if (add_array_accumulate_kernel.read()) {
          // accumulate with the output register
          result = result + add_reg_[o];
        }
        if (add_array_accumulate_out_reg.read()) {
          // accumulate with the output register
          result = result + add_array_reg_in_data[o].read();
        }
        // write the partial results to the output
        add_reg_[o] = result;
        add_array_out_data[o].write(add_reg_[o]);
#endif
      } else {
        // current output degree is idle
        add_array_out_data[o].write(Payload(0));
      }
    }
  }
}

/*
 * Implementation notes: Area
 * ---------------------------
 * We simply count the number of adders in the add array. The total area of all
 * adders within the array is accumulated.
 */
double AddArray::Area() const {
  const double adder_area = adder_model_->Area();
  const int num_adders = Pout_ * Pin_ * Pk_;
  return num_adders * adder_area;
}

/*
 * Implementation notes: StaticPower
 * ----------------------------------
 * Static power is a constant in the system during simulation.
 */
double AddArray::StaticPower() const {
  const int num_adders = Pout_ * Pin_ * Pk_;
  return num_adders * adder_model_->StaticPower();
}

double AddArray::DynamicPower() const {
  sc_time clock_period = dynamic_cast<const sc_clock*>(clock.get_interface())->
    period();
  sc_time sim_time = sc_time_stamp();
  double total_cycles = sim_time / clock_period;
  return dynamic_energy_ / total_cycles;
}

double AddArray::TotalPower() const {
  return StaticPower() + DynamicPower();
}
