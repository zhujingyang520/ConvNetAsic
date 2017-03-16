/*
 * Filename: testbench.cpp
 * ------------------------
 * This file implements the class Testbench.
 */

#include "header/systemc/testbench.hpp"
using namespace std;

void Testbench::InputLayerProc() {
  // reset behavior
  input_layer_valid.write(0);
  for (int i = 0; i < Nin_; ++i) {
    input_layer_data[i].write(Payload(0));
  }
  start_of_frame_ = end_of_frame_ = sc_time(0, SC_NS);
  start_frame_data_ = 0;
  // synthetic data for ConvNetAcc
  int data = 0;
  wait();

  while (true) {
    input_layer_valid.write(1);
    data++;
    for (int i = 0; i < Nin_; ++i) {
      input_layer_data[i].write(Payload(data));
    }
    // record the packet injection time
    inject_time_.push_back(sc_time_stamp());
    if (received_output_ && start_of_frame_ == sc_time(0, SC_NS)) {
      start_of_frame_ = sc_time_stamp();
      start_frame_data_ = data;
    }
    // early stop
    if (received_output_ && data == (1+start_frame_data_+early_stop_frame_size_*
          input_spatial_dim_)) {
      cout << "Early stop. Sent a complete frame after pipeline stage is fully"
        " warmed up!" << endl;
      end_of_frame_ = sc_time_stamp();
      sc_stop();
    }

    // handshake for valid-rdy pair
    do {
      wait();
    } while (!input_layer_rdy.read());

    // output info for tracking status
    cout << "@" << sc_time_stamp() << " Testbench sends data " << data << endl;

    // not require to wait for 1 CC
    input_layer_valid.write(0);
  }
}

void Testbench::OutputLayerProc() {
  // reset behavior
  output_layer_rdy.write(0);
  received_output_ = false;
  wait();

  while (true) {
    output_layer_rdy.write(1);
    do {
      wait();
    } while (!output_layer_valid.read());

    // output info
    cout << "@" << sc_time_stamp() << " Testbench receives output layer: ";
    received_output_ = true;
    for (int i = 0; i < Nout_; ++i) {
      cout << output_layer_data[i].read().data << " ";
    }
    cout << endl;

    // does not matter whether we require to wait for 1 CC
    output_layer_rdy.write(0);
  }
}

void Testbench::ReportStatistics() const {
  cout << "#############################" << endl;
  cout << "# Statistics of ConvNet Asic " << endl;
  cout << "#############################" << endl;
  if (inject_time_.size() < 2) {
    cout << "INFO: Not enough simulation time" << endl;
    return;
  }
  sc_time min_interval = inject_time_[1] - inject_time_[0];
  sc_time max_interval = inject_time_[1] - inject_time_[0];

  for (size_t i = 0; i < inject_time_.size()-1; ++i) {
    if ((inject_time_[i+1] - inject_time_[i]) < min_interval) {
      min_interval = inject_time_[i+1] - inject_time_[i];
    }
    if ((inject_time_[i+1] - inject_time_[i]) > max_interval) {
      max_interval = inject_time_[i+1] - inject_time_[i];
    }
  }
  // Avg interval: throughput
  sc_time avg_interval;
  if (start_of_frame_.to_double() != 0 && end_of_frame_.to_double() != 0) {
    avg_interval = (end_of_frame_ - start_of_frame_) / (early_stop_frame_size_*
        input_spatial_dim_-1);
  } else {
    avg_interval = (inject_time_.back() - inject_time_.front()) /
      (inject_time_.size() - 1);
  }
  // obtain the clock period
  sc_time clock_period = dynamic_cast<const sc_clock *>(clock.get_interface())->
    period();
  cout << "# Total injection pixels no.: " << inject_time_.size() << endl;
  cout << "# Min injection interval [cycles]: " << min_interval / clock_period
    << endl;
  cout << "# Max injection interval [cycles]: " << max_interval / clock_period
    << endl;
  cout << "# Avg injection interval (throughput) [cycles]: " << avg_interval /
    clock_period << endl;
}
