/*
 * Filename: pool_layer_ctrl.hpp
 * ------------------------------
 * This file exports the class PoolLayerCtrl, which is the core control module
 * of the pooling layer. It coordinates the different components in the current
 * pooling layer computation as well as provdes the handshake protocol
 * (valid-ready pair) to notify the downstreaming & upstreaming whether the
 * computation is finished or not.
 */

#ifndef __POOL_LAYER_CTRL_HPP__
#define __POOL_LAYER_CTRL_HPP__

#include <systemc.h>
#include <utility>

class PoolLayerCtrl : public sc_module {
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

    // constrol signal for each arithmetic unit
    // line buffer
    sc_out<bool> line_buffer_valid;
    sc_out<bool> line_buffer_zero_in;
    // line buffer mux
    sc_out<bool> line_buffer_mux_en;
    sc_out<int> line_buffer_mux_select;
    // pooling array (Avg or Max)
    sc_out<bool> pool_array_en;
    sc_out<bool>* pool_array_in_valid;
    // demux output register
    sc_out<bool> demux_out_reg_clear;
    sc_out<bool> demux_out_reg_enable;
    sc_out<int> demux_select;

    SC_HAS_PROCESS(PoolLayerCtrl);

  public:
    // constructor
    explicit PoolLayerCtrl(sc_module_name module_name, int Kh, int Kw, int h,
        int w, int Nin, int Pin, int Pad_h=0, int Pad_w=0,
        int Stride_h=1, int Stride_w=1);
    // destructor
    ~PoolLayerCtrl();

    // main process of the PoolLayerCtrl
    void PoolLayerCtrlProc();   // main control process
    void PoolArrayCtrlProc();   // pool array control process
    void DemuxOutRegCtrlProc(); // demux output register control process

  private:
    int Kh_, Kw_;               // kernel spatial dimension
    int h_, w_;                 // input feature map spatial dimension
    int Nin_;                   // channel depth for input feature map
    int Pin_;                   // input parallelism (no output for POOL)
    int Pad_h_, Pad_w_;         // pad dimension
    int Stride_h_, Stride_w_;   // stride dimension

    // internal pipeline stage variables
    static const int PIPELINE_STAGE = 3;
    // flag for each pipeline stage
    bool pipeline_flags_[PIPELINE_STAGE];

    // temporary variables used for pipelining
    // input feature map index (used by PoolArrayCtrlProc)
    int in_feat_idx_pool_ctrl_;
    // input feature map index (used by DemuxOutRegCtrlProc)
    int in_feat_idx_demux_out_ctrl_;
};

#endif
