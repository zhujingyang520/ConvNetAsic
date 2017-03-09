/*
 * Filename: top.hpp
 * ------------------
 * This file implements the class Top, which consists of the Testbench and
 * ConvNetAcc in it.
 */

#ifndef __TOP_HPP__
#define __TOP_HPP__

#include "header/systemc/data_type.hpp"
#include "header/systemc/convnet_acc.hpp"
#include "header/systemc/testbench.hpp"
#include "proto/config.pb.h"
#include <systemc.h>

class Top : public sc_module {
  public:
    // port
    sc_in<bool> clock;
    sc_in<bool> reset;

  public:
    // interconnections
    sc_signal<bool> input_layer_valid;
    sc_signal<bool> input_layer_rdy;
    sc_signal<Payload>* input_layer_data;
    sc_signal<bool> output_layer_rdy;
    sc_signal<bool> output_layer_valid;
    sc_signal<Payload>* output_layer_data;

    // modules
    Testbench* testbench;
    ConvNetAcc* convnet_acc;

  public:
    // constructor
    Top(sc_module_name module_name, const Net& net,
        const config::ConfigParameter& config_param, sc_trace_file* tf=NULL);
    // destructor
    ~Top();

    inline void ReportStatistics() const { testbench->ReportStatistics(); }

    inline double Area() const {
      return convnet_acc->Area();
    }
    inline double StaticPower() const {
      return convnet_acc->StaticPower();
    }
    inline double DynamicPower() const {
      return convnet_acc->DynamicPower();
    }
    inline double TotalPower() const {
      return convnet_acc->TotalPower();
    }

    void ReportAreaBreakdown() const;
    void ReportPowerBreakdown() const;
    void ReportMemoryDistribution() const;
};

#endif
