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
  // synthetic data for ConvNetAcc
  int data = 0;
  wait();

  while (true) {
    input_layer_valid.write(1);
    data++;
    for (int i = 0; i < Nin_; ++i) {
      input_layer_data[i].write(Payload(data));
    }
    inject_time_.push_back(sc_time_stamp());
    if (data == input_spatial_dim_ + 1) {
      start_of_frame_ = sc_time_stamp();
    } else if (data == 2*input_spatial_dim_) {
      // early stop: run for a complete 2nd frame, which is enough for deriving
      // the throughput and the buffer status
      cout << "Early stop: already sent 2 frames of the input." << endl;
      end_of_frame_ = sc_time_stamp();
      sc_stop();
    }

    // handshake for valid-rdy pair
    do {
      wait();
    } while (!input_layer_rdy.read());

    // output info for tracking status
    cout << "@" << sc_time_stamp() << " Testbench sends data " << data << endl;

    // wait for 1 CC
    input_layer_valid.write(0);
    wait();
  }
}

void Testbench::OutputLayerProc() {
  // reset behavior
  output_layer_rdy.write(0);
  wait();

  while (true) {
    output_layer_rdy.write(1);
    do {
      wait();
    } while (!output_layer_valid.read());

    // output info
    cout << "@" << sc_time_stamp() << " Testbench receives output layer: ";
    for (int i = 0; i < Nout_; ++i) {
      cout << output_layer_data[i].read().data << " ";
    }
    cout << endl;

    // wait for 1 CC
    output_layer_rdy.write(0);
    wait();
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
    avg_interval = (end_of_frame_ - start_of_frame_) / (input_spatial_dim_ - 1);
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
