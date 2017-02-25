/*
 * Filename: mult_array.cpp
 * -------------------------
 * This file implememts the class MultArray, which is used to multiply the
 * sliding window exposed by the line buffer with the weight memory.
 */

#include "header/systemc/mult_array.hpp"
using namespace std;

/*
 * Implmentation notes: Constructor
 * ---------------------------------
 * Allocates the input & output data port of the multiplier array.
 */
MultArray::MultArray(sc_module_name module_name, int Kh, int Kw, int Pin,
    int Pout)
  : sc_module(module_name), Kh_(Kh), Kw_(Kw), Pin_(Pin), Pout_(Pout) {
    // the input data valid signal w.r.t. each input sliding window
    mult_array_in_valid = new sc_in<bool>[Pout_*Pin_];
    mult_array_act_in_data = new sc_in<Payload>[Pin_*Kh_*Kw_];
    mult_array_weight_in_data = new sc_in<Payload>[Pout_*Pin_*Kh_*Kw_];
    mult_array_output_data = new sc_out<Payload>[Pout_*Pin_*Kh_*Kw_];

    // MultArrayProc: synchronous with clock and reset
    SC_METHOD(MultArrayProc);
    sensitive << clock.pos() << reset;
}

/*
 * Implmentation notes: Destructor
 * --------------------------------
 * Free the allocated memory.
 */
MultArray::~MultArray() {
  delete [] mult_array_in_valid;
  delete [] mult_array_act_in_data;
  delete [] mult_array_weight_in_data;
  delete [] mult_array_output_data;
}

/*
 * Implmentation notes: MultArrayProc
 * -----------------------------------
 * The multiplier works as the multiplication of kernel and input feature map if
 * the enable signal and valid signal are asserted together.
 */
void MultArray::MultArrayProc() {
  if (reset.read()) {
    // reset behavior, reset the output to full 0s
    for (int i = 0; i < Pout_*Pin_*Kh_*Kw_; ++i) {
      mult_array_output_data[i] = Payload(0);
    }
  } else if (mult_array_en.read()) {
#ifdef DATA_PATH
    // only do the calculation when the valid signal is asserted
    for (int o = 0; o < Pout_; ++o) {
      for (int i = 0; i < Pin_; ++i) {
        if (mult_array_in_valid[o*Pin_+i].read()) {
            // print the log info
            if (o == 0) {
            cout << "@" << sc_time_stamp() << " MultArray received sliding "
              "window from Pin " << i <<  ": ";
            for (int m = 0; m < Kh_; ++m) {
              for (int n = 0; n < Kw_; ++n) {
                cout << mult_array_act_in_data[i*Kh_*Kw_+m*Kw_+n].read().data
                  << " ";
              }
            }
            cout << endl;
          }
          // when the input valid signal is valid, do the multiplication over
          // kernel Kh*Kw
          for (int m = 0; m < Kh_; ++m) {
            for (int n = 0; n < Kw_; ++n) {
              mult_array_output_data[o*Pin_*Kh_*Kw_+i*Kh_*Kw_+m*Kw_+n].write(
                mult_array_act_in_data[i*Kh_*Kw_+m*Kw_+n].read() *
                mult_array_weight_in_data[o*Pin_*Kh_*Kw_+i*Kh_*Kw_+m*Kw_+n].
                read());
            }
          }
        } else {
          // otherwise, zero the multiplication results. It occurs when the
          // parallelism is non-divisible by the channel depth
          for (int m = 0; m < Kh_; ++m) {
            for (int n = 0; n < Kw_; ++n) {
              mult_array_output_data[o*Pin_*Kh_*Kw_+i*Kh_*Kw_+m*Kw_+n].write(
                  Payload(0));
            }
          }
        }
      }
    }
#else
    //cout << "@" << sc_time_stamp() << " MultArray received enable" << endl;
#endif
  }
}

/*
 * Implmentation notes: Area
 * --------------------------
 * It counts the number of multipliers in the multiplier array. The area of
 * multipliers is the sum of all multipliers in the array.
 */
double MultArray::Area(int bit_width, int tech_node) const {
  // total number of multipliers in the array
  const int num_mults = Pout_ * Pin_ * Kh_ * Kw_;
  MultModel mult_model(bit_width, tech_node);
  return num_mults * mult_model.Area();
}
