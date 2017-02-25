/*
 * Filename: pool_array.cpp
 * -------------------------
 * This file implements the class PoolArray, which calculates the output feature
 * map.
 */

#include "header/systemc/pool_array.hpp"
using namespace std;

/*
 * Implmentation notes: Constructor
 * ---------------------------------
 * Allocates the input & output data port of the pool array.
 */
PoolArray::PoolArray(sc_module_name module_name, int Kh, int Kw, int Pin,
    PoolMethod pool_method) : sc_module(module_name) {
  Kh_ = Kh;
  Kw_ = Kw;
  Pin_ = Pin;
  pool_method_ = pool_method;

  // data port width no.: Pin for pooling layer
  pool_array_in_valid = new sc_in<bool> [Pin_];
  pool_array_in_data = new sc_in<Payload> [Pin_*Kh_*Kw_];
  pool_array_out_data = new sc_out<Payload> [Pin_];

  // PoolArrayProc: synchronous with clock and reset
  SC_METHOD(PoolArrayProc);
  sensitive << clock.pos() << reset;
}

PoolArray::~PoolArray() {
  delete [] pool_array_in_valid;
  delete [] pool_array_in_data;
  delete [] pool_array_out_data;
}

void PoolArray::PoolArrayProc() {
  if (reset.read()) {
    for (int i = 0; i < Pin_; ++i) {
      pool_array_out_data[i].write(Payload(0));
    }
  } else if (pool_array_en.read()) {
#ifdef DATA_PATH
    // only do the pooling operation when the corresponding valid signal is
    // asserted
    for (int i = 0; i < Pin_; ++i) {
      if (pool_array_in_valid[i].read()) {
        // print the log info
        cout << "@" << sc_time_stamp() << " PoolArray received sliding window"
          " from Pin " << i << ": ";
        for (int m = 0; m < Kh_; ++m) {
          for (int n = 0; n < Kw_; ++n) {
            cout << pool_array_in_data[i*Kh_*Kw_+m*Kw_+n].read().data << " ";
          }
        }
        cout << endl;

        // do the real computation
        Payload result(0);
        switch (pool_method_) {
          case MAX:
            // search the max value within the sliding window
            result = Payload(pool_array_in_data[0].read());
            for (int m = 0; m < Kh_; ++m) {
              for (int n = 0; n < Kw_; ++n) {
                if (pool_array_in_data[i*Kh_*Kw_+m*Kw_+n].read() > result) {
                  result = pool_array_in_data[i*Kh_*Kw_+m*Kw_+n].read();
                }
              }
            }
            pool_array_out_data[i].write(result);
            break;
          case AVG:
            // compute the average value within the sliding window
            for (int m = 0; m < Kh_; ++m) {
              for (int n = 0; n < Kw_; ++n) {
                result = result + pool_array_in_data[i*Kh_*Kw_+m*Kw_+n].read();
              }
            }
            result = result / Payload(Kh_*Kw_);
            pool_array_out_data[i].write(result);
            break;
          default:
            cerr << "unexpected pooling method: " << pool_method_ << endl;
            exit(1);
        }
      } else {
        // not valid for the current sliding window, simply outputs 0
        pool_array_out_data[i].write(Payload(0));
      }
    }
#else
    //cout << "@" << sc_time_stamp() << " PoolArray recieved data" << endl;
#endif
  }
}

/*
 * Implmentation notes: Area
 * --------------------------
 * Depends on the POOL method, the area model is different.
 */
double PoolArray::Area(int bit_width, int tech_node) const {
  if (pool_method_ == MAX) {
    // TODO: model the comparator area
    const int num_comparators = Pin_ * (Kh_ * Kw_ - 1);
    return 0. * num_comparators;
  } else if (pool_method_ == AVG) {
    const int num_adders = Pin_ * (Kh_ * Kw_ - 1);
    AdderModel adder_model(bit_width, tech_node);
    return num_adders * adder_model.Area();
  } else {
    cerr << "undefined pool method: " << pool_method_ << endl;
    exit(1);
  }
}
