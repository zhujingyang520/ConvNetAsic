/*
 * Filename: pool_layer_pe.hpp
 * ----------------------------
 * This file exports the class PoolLayerPe, which is the processing element for
 * a spicific pooling layer. The module mainly contains:
 *
 * a) PoolLayerCtrl: FSM control module to coordinate each arithmetic module to
 *    do the pooling operation.
 *
 * b) LineBuffer: intermediate line buffers storing the temporary results of the
 *    feature map and expose the sliding window of the pooling operation.
 *
 * c) LineBufferMux: mux to select the corresponding input feature maps from all
 *    input feature maps for the on-going arithmetic operations.
 *
 * d) PoolArray: pool array, core module conducting the max / avg pooling
 *    operation.
 *
 * e) DemuxOutReg: output register holding the partial results.
 */

#ifndef __POOL_LAYER_PE_HPP__
#define __POOL_LAYER_PE_HPP__

#include "header/systemc/data_type.hpp"
#include "header/systemc/pool_layer_ctrl.hpp"
#include "header/systemc/line_buffer_array.hpp"
#include "header/systemc/line_buffer_mux.hpp"
#include "header/systemc/pool_array.hpp"
#include "header/systemc/demux_out_reg.hpp"
#include <systemc.h>

class PoolLayerPe : public sc_module {
  friend class Top;
  // ports
  public:
    sc_in<bool> clock;
    sc_in<bool> reset;
    // input data from the previous layer
    sc_in<bool> prev_layer_valid;
    sc_out<bool> prev_layer_rdy;
    sc_in<Payload>* prev_layer_data;

    // output to the next layer
    sc_in<bool> next_layer_rdy;
    sc_out<bool> next_layer_valid;
    sc_out<Payload>* next_layer_data;

    SC_HAS_PROCESS(PoolLayerPe);

    double Area() const;
    double StaticPower() const;
    double DynamicPower() const;
    double TotalPower() const;

  private:
    // internal modules
    PoolLayerCtrl* pool_layer_ctrl_;
    LineBufferArray* line_buffer_array_;
    LineBufferMux* line_buffer_mux_;
    PoolArray* pool_array_;
    DemuxOutReg* demux_out_reg_;

  private:
    int Nin_;     // input feature map channel number
    int Pin_;     // input parallelism

  public:
    // internal interconnections
    sc_signal<bool> line_buffer_valid_;
    sc_signal<Payload>* line_buffer_in_data_;
    sc_signal<bool> line_buffer_zero_in_;
    sc_signal<Payload>* line_buffer_out_data_;
    sc_signal<bool> line_buffer_mux_en_;
    sc_signal<int> line_buffer_mux_select_;
    sc_signal<Payload>* line_buffer_mux_out_data_;
    sc_signal<bool> pool_array_en_;
    sc_signal<bool>* pool_array_in_valid_;
    sc_signal<Payload>* pool_array_out_data_;
    sc_signal<bool> demux_out_reg_clear_;
    sc_signal<bool> demux_out_reg_enable_;
    sc_signal<int> demux_select_;

  public:
    // constructor
    explicit PoolLayerPe(sc_module_name module_name, int Kh, int Kw, int h,
        int w, int Nin, int Pin, int Pad_h=0, int Pad_w=0, int Stride_h=1,
        int Stride_w=1, PoolArray::PoolMethod pool_method=PoolArray::MAX,
        int bit_width=8, int tech_node=28);
    // destructor
    ~PoolLayerPe();

    // line buffer zero padding mux
    void LineBufferInMux();
};

#endif
