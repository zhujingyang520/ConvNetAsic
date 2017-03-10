/*
 * Filename: conv_layer_pe.hpp
 * ----------------------------
 * This file exports the class ConvLayerPe, which is the processing element for
 * a specific convolutional layer. The module mainly consists:
 *
 * a) ConvLayerCtrl: FSM control module to coordinate each arithmetic module to
 *    do the calculation.
 *
 * b) LineBuffer: intermediate line buffers storing the temporary results of the
 *    feature map and expose the sliding window of the convolution operation.
 *
 * c) LineBufferMux: mux to select the corresponding input feature maps from all
 *    input feature maps for the following computations.
 *
 * d) WeightMem: kernel memory.
 *
 * e) MultArray: multiplication array.
 *
 * f) AddArray: adder tree (array).
 *
 * g) DemuxOutReg: output register holding the partial results.
 */

#ifndef __CONV_LAYER_PE_HPP__
#define __CONV_LAYER_PE_HPP__

#include "header/systemc/data_type.hpp"
#include "header/systemc/conv_layer_ctrl.hpp"
#include "header/systemc/line_buffer_array.hpp"
#include "header/systemc/line_buffer_mux.hpp"
#include "header/systemc/weight_mem.hpp"
#include "header/systemc/mult_array.hpp"
#include "header/systemc/add_array.hpp"
#include "header/systemc/demux_out_reg.hpp"
#include "proto/config.pb.h"
#include <systemc.h>

class ConvLayerPe : public sc_module {
  friend class Top;
  // ports
  public:
    sc_in<bool> clock;
    sc_in<bool> reset;
    // input data from the previous layer
    sc_in<bool> prev_layer_valid;
    sc_out<bool> prev_layer_rdy;
    sc_in<Payload>* prev_layer_data;

    // output data to the next layer
    sc_in<bool> next_layer_rdy;
    sc_out<bool> next_layer_valid;
    sc_out<Payload>* next_layer_data;

    SC_HAS_PROCESS(ConvLayerPe);

    double Area() const;
    double StaticPower() const;
    double DynamicPower() const;
    double TotalPower() const;

  private:
    // internal modules
    ConvLayerCtrl* conv_layer_ctrl_;
    LineBufferArray* line_buffer_array_;
    LineBufferMux* line_buffer_mux_;
    WeightMem* weight_mem_;
    MultArray* mult_array_;
    AddArray* add_array_;
    DemuxOutReg* demux_out_reg_;

  private:
    int Nin_;     // input feature map depth
    int Nout_;    // output feature map depth
    int Pout_;    // parallelism of output feature map
    int Pin_;     // parallelism of input feature map
  public:
    // internal interconnections
    sc_signal<bool> line_buffer_valid_;
    sc_signal<Payload>* line_buffer_in_data_;
    sc_signal<bool> line_buffer_zero_in_;
    sc_signal<bool> line_buffer_mux_en_;
    sc_signal<int> line_buffer_mux_select_;
    sc_signal<bool> weight_mem_rd_en_;
    sc_signal<int> weight_mem_rd_addr_;
    sc_signal<bool> mult_array_en_;
    sc_signal<Payload>* line_buffer_out_data_;
    sc_signal<Payload>* line_buffer_mux_out_data_;
    sc_signal<Payload>* weight_mem_rd_data_;
    sc_signal<bool>* mult_array_in_valid_;
    sc_signal<Payload>* mult_array_out_data_;
    sc_signal<bool> add_array_en_;
    sc_signal<bool>* add_array_in_valid_;
    sc_signal<int> add_array_out_reg_select_;
    sc_signal<Payload>* add_array_reg_in_data_;
    sc_signal<Payload>* add_array_out_data_;
    sc_signal<bool> demux_out_reg_clear_;
    sc_signal<bool> demux_out_reg_enable_;
    sc_signal<int> demux_select_;
    sc_signal<Payload>* out_reg_data_;

  public:
    // constructor
    explicit ConvLayerPe(sc_module_name module_name, int Kh, int Kw, int h,
        int w, int Nin, int Nout, int Pin, int Pout, int Pad_h=0,
        int Pad_w=0, int Stride_h=1, int Stride_w=1,
        config::ConfigParameter_MemoryType memory_type=
        config::ConfigParameter_MemoryType_ROM, int bit_width=8,
        int tech_node=28, double clk_freq=1.);
    // destructor
    ~ConvLayerPe();

    // line buffer zero padding mux
    void LineBufferInMux();

    // output register partial results
    void AddArrayRegInMux();

    // next_layer_data connection
    void NextLayerDataConnect();
};

#endif

