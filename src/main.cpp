/*
 * Filename: main.cpp
 * -------------------
 * This is the main file of the program.
 */

#include "header/cmd_parser.hpp"
#include "header/caffe/net.hpp"
#include "header/systemc/top.hpp"
#include "proto/config.pb.h"
#include <iostream>
#include <systemc.h>

using namespace std;

/* Main gateway function */
int sc_main (int argc, char **argv) {
  // parse the inline commands
  CmdParser cmd_parser(argc, argv);
  cmd_parser.Parse();

  // parse the network architecture prototxt file
  Net caffe_net(cmd_parser.model_filename);

  // create trace file
  sc_trace_file* tf = NULL;
  sc_set_time_resolution(100, SC_PS);
  if (cmd_parser.config_param.trace_file() != "") {
    tf = sc_create_vcd_trace_file(cmd_parser.config_param.trace_file().c_str());
  }

  // TODO: hardcode clock period
  sc_clock clock("clock", 1, SC_NS);
  sc_signal<bool> reset;
  sc_trace(tf, clock, "clock");
  sc_trace(tf, reset, "reset");

  // top module: testbench & convnet_acc
  Top top("top", caffe_net, cmd_parser.config_param, tf);
  top.clock(clock);
  top.reset(reset);

  cout << "reset: " << cmd_parser.config_param.reset_period() << endl;
  cout << "sim: " << cmd_parser.config_param.sim_period() << endl;

  // run the simulation
  const int reset_period = cmd_parser.config_param.reset_period();
  cout << "starts reset for " << reset_period << " cycles ..." << endl;
  reset.write(1);
  sc_start(reset_period, SC_NS);
  reset.write(0);

  const int sim_period = cmd_parser.config_param.sim_period();
  cout << "starts run simulation for " << sim_period << " cycles ..." << endl;
  sc_start(sim_period, SC_NS);

  // report the statistics of accelerator
  top.ReportStatistics();

  // report the area
  top.ReportAreaBreakdown();
  cout << "####################################################" << endl;
  cout << "Total Area: " << top.Area() << endl;
  cout << "####################################################" << endl;

  // report the power
  top.ReportPowerBreakdown();
  cout << "####################################################" << endl;
  cout << "# Power report [uW]" << endl;
  cout << "####################################################" << endl;
  cout << "Total Static Power: " << top.StaticPower() << endl;
  cout << "Total Dynamic Power: " << top.DynamicPower() << endl;
  cout << "Total Power: " << top.TotalPower() << endl;
  cout << "####################################################" << endl;

  // report the memory distribution
  //top.ReportMemoryDistribution();

  // close the trace file
  if (tf) {
    sc_close_vcd_trace_file(tf);
  }

  return 0;
}
