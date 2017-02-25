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
    int Pout)
  : sc_module(module_name), Kh_(Kh), Kw_(Kw), Pin_(Pin), Pout_(Pout) {
    // input valid no. = Pout
    add_array_in_valid = new sc_in<bool> [Pout_];
    // multiplier array input no. = Pout*Pin*Kh*Kw
    add_array_mult_in_data = new sc_in<Payload> [Pout_*Pin_*Kh_*Kw_];
    // partial results registers no. = Pout
    add_array_reg_in_data = new sc_in<Payload> [Pout_];
    // add array results no. = Pout
    add_array_out_data = new sc_out<Payload> [Pout_];

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
#else
        Payload result = Payload(0);
#endif
        // write the partial results to the output
        add_array_out_data[o].write(result);
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
double AddArray::Area(int bit_width, int tech_node) const {
  // area model of one adder
  AdderModel adder_model(bit_width, tech_node);
  // A N-input adder tree contains N-1 adders
  // Adder array contains Pout adder trees, each of them accumulates the input
  // of Pin_*Kh_*Kw_ values
  int num_adders = Pout_ * (Kh_ * Kw_ * Pin_ - 1);
  return adder_model.Area() * num_adders;
}
