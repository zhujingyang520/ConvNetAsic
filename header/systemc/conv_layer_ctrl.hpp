/*
 * Filename: conv_layer_ctrl.hpp
 * ------------------------------
 * This file exports the class ConvLayerCtrl, which is the core control module
 * of the convolutional layer. It coordinates the different components in the
 * current convolutional layer computation as well as provides the handshake
 * protocol (valid-ready pair) to notify the downstreaming & upstreaming whether
 * the computation is finished or not.
 */

#ifndef __CONV_LAYER_CTRL_HPP__
#define __CONV_LAYER_CTRL_HPP__

#include <systemc.h>
#include <utility>

class ConvLayerCtrl : public sc_module {
  // ports
  public:
    sc_in<bool> clock;
    sc_in<bool> reset;
    // handshake for the previous layer
    sc_in<bool> prev_layer_valid;
    sc_out<bool> prev_layer_rdy;
    // handshake for the next layer
    sc_in<bool> next_layer_rdy;
    sc_out<bool> next_layer_valid;

    // control signal for each arithmetic unit
    // line buffer
    sc_out<bool> line_buffer_valid;
    sc_out<bool> line_buffer_zero_in;
    // line buffer mux
    sc_out<bool> line_buffer_mux_en;
    sc_out<int> line_buffer_mux_select;
    // kernel memory
    sc_out<bool> weight_mem_rd_en;
    sc_out<int> weight_mem_rd_addr;
    // multiplier array
    sc_out<bool> mult_array_en;
    sc_out<bool>* mult_array_in_valid;
    // add array
    sc_out<bool> add_array_en;
    sc_out<bool>* add_array_in_valid;
    sc_out<int> add_array_out_reg_select;
    // demux output register
    sc_out<bool> demux_out_reg_clear;
    sc_out<bool> demux_out_reg_enable;
    sc_out<int> demux_select;

    SC_HAS_PROCESS(ConvLayerCtrl);

  public:
    // constructor
    explicit ConvLayerCtrl(sc_module_name module_name, int Kh, int Kw, int h,
        int w, int Nin, int Nout, int Pin, int Pout, int Pad_h=0,
        int Pad_w=0, int Stride_h=1, int Stride_w=1);
    // destructor
    ~ConvLayerCtrl();

    // main process of the ConvLayerCtrl
    void ConvLayerCtrlProc();   // main control process
    void MultArrayCtrlProc();   // mult array control process
    void AddArrayCtrlProc();    // add array control process
    void DemuxOutRegCtrlProc(); // demux output register control process

  private:
    int Kh_, Kw_;               // kernel spatial dimension
    int h_, w_;                 // input feature map spatial dimension
    int Nin_, Nout_;            // channel depth for input & output feature map
    int Pin_, Pout_;            // input & output parallelism
    int Pad_h_, Pad_w_;         // pad dimension
    int Stride_h_, Stride_w_;   // stride dimension

    // internal pipeline stage variables
    static const int PIPELINE_STAGE = 4;
    // flag for start pipeline
    bool pipeline_flags_[PIPELINE_STAGE];
    int line_buffer_mux_select_;
    int weight_mem_rd_addr_;
    // feature map location tracks the current computation progress
    // where the pair (i, j) indicates the i-th input feature map & j-th output
    // feature map (recorded by ConvLayerCtrlProc, used by MultArrayCtrlProc)
    std::pair<int, int> feat_map_loc_;

    // output feature map index (used by AddArrayCtrlProc)
    int out_feat_idx_add_ctrl_;

    // output feature map index (used by DemuxOutCtrlReg)
    int out_feat_idx_demux_out_ctrl_;
};

#endif
