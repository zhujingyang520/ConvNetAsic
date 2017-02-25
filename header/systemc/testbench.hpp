/*
 * Filename: testbench.hpp
 * ------------------------
 * This file exports the class Testbench, which provides the input stimulus &
 * monitors output signals.
 */

#ifndef __TESTBENCH_HPP__
#define __TESTBENCH_HPP__

#include "header/systemc/data_type.hpp"
#include <systemc.h>
#include <vector>

class Testbench : public sc_module {
  // ports
  public:
    sc_in<bool> clock;
    sc_in<bool> reset;
    // handshake input signals for ConvNetAcc
    sc_out<bool> input_layer_valid;
    sc_in<bool> input_layer_rdy;
    sc_out<Payload>* input_layer_data;
    // handshake output signals for ConvNetAcc
    sc_out<bool> output_layer_rdy;
    sc_in<bool> output_layer_valid;
    sc_in<Payload>* output_layer_data;

    SC_HAS_PROCESS(Testbench);


  public:
    // constructor
    explicit Testbench(sc_module_name module_name, int Nin, int Nout,
        int input_spatial_dim) :
      sc_module(module_name), Nin_(Nin), Nout_(Nout),
      input_spatial_dim_(input_spatial_dim) {
      input_layer_data = new sc_out<Payload> [Nin];
      output_layer_data = new sc_in<Payload> [Nout];

      start_of_frame_ = end_of_frame_ = sc_time(0, SC_NS);

      SC_CTHREAD(InputLayerProc, clock.pos());
      reset_signal_is(reset, true);

      SC_CTHREAD(OutputLayerProc, clock.pos());
      reset_signal_is(reset, true);
    }
    // destructor
    ~Testbench() { delete [] input_layer_data; delete [] output_layer_data; }

    // main process for Testbench
    void InputLayerProc();      // input layer process
    void OutputLayerProc();     // output layer process

    // report the statistics after the simulation
    void ReportStatistics() const;

  private:
    int Nin_;                   // input feature map depth
    int Nout_;                  // output feature map depth
    int input_spatial_dim_;     // input image spatial dimension
    // injection time for each pixel
    std::vector<sc_time> inject_time_;

    // time slot for start of 2nd frame & end of 2nd frame
    sc_time start_of_frame_;
    sc_time end_of_frame_;
};

#endif
