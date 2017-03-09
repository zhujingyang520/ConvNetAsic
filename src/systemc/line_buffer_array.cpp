/*
 * Filename: line_buffer_array.cpp
 * --------------------------------
 * This file implements the class LineBufferArray.
 */

#include "header/systemc/line_buffer_array.hpp"
using namespace std;

/*
 * Implementation notes: Constructor
 * ----------------------------------
 * The constructor mainly initializes Nin line buffers and makes the
 * interconnections.
 */
LineBufferArray::LineBufferArray(sc_module_name module_name, int Kh, int Kw,
    int h, int w, int Nin, int bit_width, int tech_node)
  : sc_module(module_name), Kh_(Kh), Kw_(Kw), h_(h), w_(w), Nin_(Nin) {
  // allocate the ports
  input_data = new sc_in<Payload> [Nin];
  output_data = new sc_out<Payload> [Nin*Kh*Kw];

  // allocate Nin line buffers
  char name [100];
  line_buffer_ = new LineBuffer* [Nin];
  for (int i = 0; i < Nin; ++i) {
    sprintf(name, "line_buffer_%d", i);
    line_buffer_[i] = new LineBuffer(name, Kh, Kw, h, w, bit_width, tech_node);
    line_buffer_[i]->clock(clock);
    line_buffer_[i]->reset(reset);
    line_buffer_[i]->input_data(input_data[i]);
    line_buffer_[i]->input_data_valid(input_data_valid);
    for (int j = 0; j < Kh*Kw; ++j) {
      line_buffer_[i]->output_data[j](output_data[i*Kh*Kw+j]);
    }
  }

  // we use the centralized model of Nin line buffers (data width is incremented
  // by Nin times)
  const int centralized_memory_depth = line_buffer_[0]->MemoryDepth();
  const int centralized_memory_width = bit_width * Nin * line_buffer_[0]->
    MemoryWidth();
  memory_model_ = new MemoryModel(centralized_memory_width,
      centralized_memory_depth, tech_node,
      config::ConfigParameter_MemoryType_RAM);
  dynamic_energy_ = 0.;

  SC_METHOD(PowerManagement);
  sensitive << clock.pos();
}

/*
 * Implementation notes: destructor
 * ---------------------------------
 * Free the allocated dynamic space.
 */
LineBufferArray::~LineBufferArray() {
  for (int i = 0; i < Nin_; ++i) {
    delete line_buffer_[i];
  }
  delete [] line_buffer_;
  delete memory_model_;
}

/*
 * Implementation notes: Area
 * ---------------------------
 * We will aggregate all Nin buffer together, i.e. the equivalent width of the
 * line buffer will be increased by Nin times.
 */
double LineBufferArray::Area() const {
  return memory_model_->Area();
}

double LineBufferArray::StaticPower() const {
  return memory_model_->StaticPower();
}

double LineBufferArray::DynamicPower() const {
  sc_time clock_period = dynamic_cast<const sc_clock*>(clock.get_interface())->
    period();
  sc_time sim_time = sc_time_stamp();
  int total_cycles = sim_time / clock_period;
  return dynamic_energy_ / total_cycles;
}

double LineBufferArray::TotalPower() const {
  return StaticPower() + DynamicPower();
}

/*
 * Implementation notes: PowerManagement
 * --------------------------------------
 * Update the dynamic power consumption when the input data is valid. I.e one
 * memory access is underwent.
 */
void LineBufferArray::PowerManagement() {
  if (input_data_valid.read()) {
    dynamic_energy_ += (memory_model_->DynamicEnergyOfReadOperation() +
      memory_model_->DynamicEnergyOfWriteOperation());
  }
}
