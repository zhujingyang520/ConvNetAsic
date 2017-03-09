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
    int Pout, int bit_width, int tech_node)
  : sc_module(module_name), Kh_(Kh), Kw_(Kw), Pin_(Pin), Pout_(Pout) {
    // input valid no. = Pout
    add_array_in_valid = new sc_in<bool> [Pout_];
    // multiplier array input no. = Pout*Pin*Kh*Kw
    add_array_mult_in_data = new sc_in<Payload> [Pout_*Pin_*Kh_*Kw_];
    // partial results registers no. = Pout
    add_array_reg_in_data = new sc_in<Payload> [Pout_];
    // add array results no. = Pout
    add_array_out_data = new sc_out<Payload> [Pout_];

    // initialize the adder model
    adder_model_ = new AdderModel(bit_width, tech_node);
    dynamic_energy_ = 0.;

    // AddArrayProc: synchronous with clock and reset
    SC_METHOD(AddArrayProc);
    sensitive << clock.pos() << reset;
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
        // there are Pin*Kh*Kw-1 adders
        dynamic_energy_ += adder_model_->DynamicEnergyOfOneOperation()*(Pin_*Kh_
          *Kw_-1);
#ifdef DATA_PATH
        // summing the Pin kernel activation results & partial results
        Payload result = add_array_reg_in_data[o].read();
        // add over Pin Kh*Kw kernels
        for (int i = 0; i < Pin_; ++i) {
          for (int m = 0; m < Kh_; ++m) {
            for (int n = 0; n < Kw_; ++n) {
              result = result + add_array_mult_in_data[i*Kh_*Kw_+m*Kw_+n].
                read();
            }
          }
        }
        // write the partial results to the output
        add_array_out_data[o].write(result);
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
  const int num_adders = Pout_ * (Kh_ * Kw_ * Pin_ - 1);
  return num_adders * adder_area;
}

/*
 * Implementation notes: StaticPower
 * ----------------------------------
 * Static power is a constant in the system during simulation.
 */
double AddArray::StaticPower() const {
  const int num_adders = Pout_ * (Kh_ * Kw_ * Pout_ - 1);
  return num_adders * adder_model_->StaticPower();
}

double AddArray::DynamicPower() const {
  sc_time clock_period = dynamic_cast<const sc_clock*>(clock.get_interface())->
    period();
  sc_time sim_time = sc_time_stamp();
  int total_cycles = sim_time / clock_period;
  return dynamic_energy_ / total_cycles;
}

double AddArray::TotalPower() const {
  return StaticPower() + DynamicPower();
}
