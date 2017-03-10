/*
 * Filename: line_buffer.hpp
 * --------------------------
 * This file exports the behavior model of LineBuffer.
 */

#ifndef __LINE_BUFFER_HPP__
#define __LINE_BUFFER_HPP__

#include "header/systemc/data_type.hpp"
#include "header/systemc/models/memory_model.hpp"
#include <systemc.h>
#include <queue>

class LineBuffer : public sc_module {
  // ports
  public:
    sc_in<bool> clock;
    sc_in<bool> reset;
    // input data & valid
    sc_in<Payload> input_data;
    sc_in<bool> input_data_valid;
    // output data: expose Kh * Kw data, determined dynamically
    sc_out<Payload>* output_data;

    SC_HAS_PROCESS(LineBuffer);

    // hardware-related model
    // area model of the line buffer
    double Area() const;
    // power model of the line buffer
    double StaticPower() const;
    double DynamicPower() const;
    double TotalPower() const;

    // return the memory depth & width
    inline int MemoryDepth() const { return sram_depth_; }
    inline int MemoryWidth() const { return Kh_-1; }
    // memory size [no. entries]
    inline int MemorySize() const { return MemoryDepth()*MemoryWidth(); }

  public:
    // constructor
    explicit LineBuffer(sc_module_name module_name, int Kh, int Kw, int h,
        int w, int bit_width=8, int tech_node=28, double clk_freq=1.);
    // destructor
    ~LineBuffer();

    // main process of the line buffer
    void LineBufferProc();

  private:
    // instance variables
    int Kh_, Kw_;     // kernel spatial dimension
    int h_, w_;       // feature map dimension

    // payload DFF: expose the sliding window
    Payload* payload_dff_;
    // payload SRAM
    std::queue<Payload>* payload_sram_;
    int sram_depth_;  // sram depth

    MemoryModel* memory_model_;
    double dynamic_energy_;
};

#endif
