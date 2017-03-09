/*
 * Filename: line_buffer_array.hpp
 * --------------------------------
 * This file exports the behavior model of LineBufferArray. It is the
 * aggregation of the Nin line buffers.
 */

#ifndef __LINE_BUFFER_ARRAY_HPP__
#define __LINE_BUFFER_ARRAY_HPP__

#include "header/systemc/data_type.hpp"
#include "header/systemc/models/memory_model.hpp"
#include "header/systemc/line_buffer.hpp"
#include <systemc.h>

class LineBufferArray : public sc_module {
  // ports
  public:
    sc_in<bool> clock;
    sc_in<bool> reset;
    // input data & valid
    sc_in<bool> input_data_valid;
    sc_in<Payload>* input_data;
    // output data: expose Nin * Kh * Kw data
    sc_out<Payload>* output_data;

    // line buffer
    LineBuffer** line_buffer_;

    SC_HAS_PROCESS(LineBufferArray);

    MemoryModel* memory_model_;
    double dynamic_energy_;

    // hardware-related model
    // area model of the line buffer array
    double Area() const;
    // power model of the line buffer array
    double StaticPower() const;
    double DynamicPower() const;
    double TotalPower() const;

    // getters of the memory configuration
    inline int MemoryWidth() const { return memory_model_->memory_width(); }
    inline int MemoryDepth() const { return memory_model_->memory_depth(); }

  public:
    // constructor
    explicit LineBufferArray(sc_module_name module_name, int Kh, int Kw, int h,
        int w, int Nin, int bit_width=8, int tech_node=28);
    ~LineBufferArray();

    // dynamic power management
    void PowerManagement();

  private:
    int Kh_, Kw_;     // kernel spatial dimension
    int h_, w_;       // input feature map dimension
    int Nin_;         // input feature map number
};

#endif
