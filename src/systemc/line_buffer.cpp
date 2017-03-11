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
LineBuffer::LineBuffer(sc_module_name module_name, int Kh, int Kw, int h, int w,
    int bit_width, int tech_node, double clk_freq)
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

    // memory model for line buffer
    const int memory_width = MemoryWidth() * bit_width;
    const int memory_depth = MemoryDepth();
    memory_model_ = new MemoryModel(memory_width, memory_depth, tech_node,
        config::ConfigParameter_MemoryType_RAM, clk_freq);
    dynamic_energy_ = 0.;

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

    // write the payload_dff_ to output data
    for (int i = 0; i < Kh_*Kw_; ++i) {
      output_data[i] = payload_dff_[i];
    }
  } else if (input_data_valid.read()) {
    // adds one operation of energy
    dynamic_energy_ += (memory_model_->DynamicEnergyOfReadOperation() +
        memory_model_->DynamicEnergyOfWriteOperation());
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

    // write the payload_dff_ to output data
    for (int i = 0; i < Kh_*Kw_; ++i) {
      output_data[i] = payload_dff_[i];
    }
#endif
  }
}

/*
 * Implmentation notes: Area
 * --------------------------
 * Area model of the current line buffer. The area is estimated based on the
 * SRAM size utilized in the line buffer.
 */
double LineBuffer::Area() const {
  return memory_model_->Area();
}

/*
 * Implmentation notes: Power
 * ---------------------------
 * Total power consumption includes the dynamic power and the static power.
 */
double LineBuffer::StaticPower() const {
  return memory_model_->StaticPower();
}

double LineBuffer::DynamicPower() const {
  sc_time clock_period = dynamic_cast<const sc_clock*>(clock.get_interface())->
    period();
  sc_time sim_time = sc_time_stamp();
  double total_cycles = sim_time / clock_period;
  return dynamic_energy_ / total_cycles;
}

double LineBuffer::TotalPower() const {
  return StaticPower() + DynamicPower();
}
