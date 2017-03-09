/*
 * Filename: demux_out_reg.hpp
 * ----------------------------
 * This file exports the class DemuxOutReg, which emulates the data path from
 * the output of the adders storing partial results back to the registers.
 */

#ifndef __DEMUX_OUT_REG_HPP__
#define __DEMUX_OUT_REG_HPP__

#include "header/systemc/data_type.hpp"
#include "header/systemc/models/demux_model.hpp"
#include <systemc.h>

class DemuxOutReg : public sc_module {
  // ports
  public:
    sc_in<bool> clock;
    sc_in<bool> reset;
    // DemuxOutReg synchronous clear
    sc_in<bool> demux_out_reg_clear;
    // DemuxOutReg enable
    sc_in<bool> demux_out_reg_enable;
    // demux select
    sc_in<int> demux_select;
    // demux input
    sc_in<Payload>* in_data;
    // output data to the next layer & partial results of adder
    sc_out<Payload>* out_data;

    SC_HAS_PROCESS(DemuxOutReg);

  public:
    // constructor
    explicit DemuxOutReg(sc_module_name module_name, int Nout, int Pout,
        int bit_width=8, int tech_node=28);
    ~DemuxOutReg();

    // main process of the DemuxOutReg
    void DemuxOutRegProc();

    // area model of the demux
    double Area() const;
    // power model of the demux
    double StaticPower() const;
    double DynamicPower() const;
    double TotalPower() const;

  private:
    int Nout_;      // output feature map depth
    int Pout_;      // output parallelism of the feature map

    // demux model
    DemuxModel* demux_model_;
    double dynamic_energy_;
};

#endif
