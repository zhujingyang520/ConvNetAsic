/*
 * Filename: line_buffer.cpp
 * --------------------------
 * This file implements the class LineBuffer.
 */

#include "header/systemc/line_buffer.hpp"
using namespace std;

/*
 * Implmentation notes: Constructor
 * ---------------------------------
 * Initialize the configuration parameters, including kernel size. And allocate
 * the DFFs together with the SRAM to form the main body of the line buffer.
 */
LineBuffer::LineBuffer(sc_module_name module_name, int Kh, int Kw, int h, int w)
  : sc_module(module_name), Kh_(Kh), Kw_(Kw), h_(h), w_(w) {
    // there are Kh_ * Kw_ DFFs in total
    payload_dff_ = new Payload[Kh_*Kw_];
    // the output ports number is Kh_*Kw_, matches with the DFFs
    output_data = new sc_out<Payload>[Kh_*Kw_];
    // the number of sram is Kh_-1, each sram is of depth w_-Kw_
    if (Kh_-1 <= 0 || w-Kw_ <= 0) {
      payload_sram_ = NULL;
      sram_depth_ = 0;
    } else {
      payload_sram_ = new queue<Payload> [Kh_-1];
      sram_depth_ = w-Kw_;
    }

    // LineBufferProc: synchronous with clock and reset
    SC_METHOD(LineBufferProc);
    sensitive << clock.pos() << reset;
}

/*
 * Implmentation notes: Destructor
 * --------------------------------
 * Free the space allocated for the DFFs, SRAM, etc.
 */
LineBuffer::~LineBuffer() {
  delete [] payload_dff_;
  delete [] output_data;
  if (payload_sram_) {
    delete [] payload_sram_;
  }
}

/*
 * Implmentation notes: LineBufferProc
 * ------------------------------------
 * The bahavior of line buffer is the streaming connection of DFFs and SRAMs.
 */
void LineBuffer::LineBufferProc() {
  if (reset.read()) {
    // reset DFFs
    for (int i = 0; i < Kh_*Kw_; ++i) {
      payload_dff_[i].data = 0;
    }
    // reset SRAMs
    for (int i = 0; i < Kh_-1 && sram_depth_ > 0; ++i) {
      // pop out all the existing elements, and store all 0s in it
      queue<Payload>& cur_payload_sram = payload_sram_[i];
      while (!cur_payload_sram.empty()) {
        cur_payload_sram.pop();
      }
      for (int e = 0; e < sram_depth_; ++e) {
        cur_payload_sram.push(Payload(0));
      }
    }
  } else if (input_data_valid.read()) {
#ifdef DATA_PATH
    // not reset & input data valid
    Payload streamed_data = input_data.read();
    Payload tmp_data;
    for(int i = 0; i < Kh_-1; ++i) {
      for (int j = 0; j < Kw_; ++j) {
        // shift values in DFFs
        tmp_data = payload_dff_[i*Kw_+j];
        payload_dff_[i*Kw_+j] = streamed_data;
        streamed_data = tmp_data;
      }
      // shift values in SRAM (if exists)
      if (sram_depth_ > 0) {
        tmp_data = payload_sram_[i].front();
        payload_sram_[i].pop();
        payload_sram_[i].push(streamed_data);
        streamed_data = tmp_data;

        // sanity check
        assert(payload_sram_[i].size() == static_cast<size_t>(sram_depth_));
      }
    }

    // registers @ Kh-th row
    for (int j = 0; j < Kw_; ++j) {
      // shifted values in DFFs
      tmp_data = payload_dff_[(Kh_-1)*Kw_+j];
      payload_dff_[(Kh_-1)*Kw_+j] = streamed_data;
      streamed_data = tmp_data;
    }
#endif
  }

  // write the payload_dff_ to output data
  for (int i = 0; i < Kh_*Kw_; ++i) {
    output_data[i] = payload_dff_[i];
  }
}

/*
 * Implmentation notes: Area
 * --------------------------
 * Area model of the current line buffer. The area is estimated based on the
 * SRAM size utilized in the line buffer.
 */
double LineBuffer::Area(int bit_width, int tech_node) const {
  // there are Kh_-1 line buffers, each is of depth sram_depth_
  const int memory_size = (Kh_-1)*sram_depth_*bit_width;
  // line buffer is always implemented by SRAM
  MemoryModel memory_model(memory_size/1024., tech_node,
      config::ConfigParameter_MemoryType_RAM);
  return memory_model.Area();
}
