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

    // area model
    double Area(int bit_width, int tech_node=28) const;
    inline double MemorySize() const { return (Kh_-1)*sram_depth_; }

  public:
    // constructor
    explicit LineBuffer(sc_module_name module_name, int Kh, int Kw, int h,
        int w);
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
};

#endif
