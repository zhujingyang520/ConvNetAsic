/*
 * Filename: channel_buffer.cpp
 * -----------------------------
 * This file implements the channel buffer.
 */

#include "header/systemc/channel_buffer.hpp"
using namespace std;

ChannelBuffer::ChannelBuffer(sc_module_name module_name, int Nin, int capacity,
    int bit_width, int tech_node, double clk_freq)
  : sc_module(module_name), Nin_(Nin), capacity_(capacity) {
  // allocate the port width
  prev_layer_data = new sc_in<Payload> [Nin];
  next_layer_data = new sc_out<Payload> [Nin];
  // allocate the buffer for each channel
  buffer_ = new queue<Payload> [Nin];

  if (capacity > 0) {
    // model the channel buffer if capacity > 0
    SC_CTHREAD(ChannelBufferRX, clock.pos());
    reset_signal_is(reset, true);

    SC_CTHREAD(ChannelBufferTX, clock.pos());
    reset_signal_is(reset, true);
  } else {
    // simplify the buffer into purly combination logic
    SC_METHOD(ChannelBufferCombLogic);
    sensitive << prev_layer_valid << next_layer_rdy;
    for (int i = 0;i < Nin; ++i) {
      sensitive << prev_layer_data[i];
    }
  }

  // we will use the default Memory Model depth as the initial memory depth
  // when we report the power consumption, we will scale the dynamic energy to
  // the actual buffer depth (i.e. max_buffer_size_)
  const int memory_width = Nin * bit_width;
  const int memory_depth = INIT_MEM_DEPTH;
  memory_model_ = new MemoryModel(memory_width, memory_depth, tech_node,
      config::ConfigParameter_MemoryType_RAM, clk_freq);
  // initialize the dynamic energy
  dynamic_write_energy_ = 0.;
  dynamic_read_energy_ = 0.;

  //SC_METHOD(ChannelBufferMonitor);
  //sensitive << clock.pos();
}

ChannelBuffer::~ChannelBuffer() {
  delete [] prev_layer_data;
  delete [] next_layer_data;
  delete [] buffer_;
  delete memory_model_;
}

/*
 * Implementation notes: ChannelBufferRX
 * --------------------------------------
 * Model the receiving process of the channel buffer. For channel buffer it will
 * always accept the upstreaming data and push it into the buffer.
 */
void ChannelBuffer::ChannelBufferRX() {
  // reset behavior
  prev_layer_rdy.write(0);
  max_buffer_size_ = 0;
  wait();

  while (true) {
    if (BufferSize() < capacity_) {
      // wait for the previous layer valid signal
      prev_layer_rdy.write(1);
      do {
        wait();
      } while (!prev_layer_valid.read());
      // if there is no no element in the fifo and the next stage is ready to
      // receive, we will not push the element in the FIFO and bypass the data
      // to the output of the channel
      if (BufferSize() == 0 && next_layer_rdy.read()) {
        continue;
      }

      // push the data into channel buffer
      prev_layer_rdy.write(0);
      for (int i = 0; i < Nin_; ++i) {
        buffer_[i].push(prev_layer_data[i]);
      }
      // update the max buffer size
      if (max_buffer_size_ < BufferSize()) {
        max_buffer_size_ = BufferSize();
      }
      // increments memory energy
      dynamic_write_energy_ += memory_model_->DynamicEnergyOfWriteOperation();
    } else {
      // the buffer is full, wait for drained
      prev_layer_rdy.write(0);
      wait();
    }
  }
}

/*
 * Implementation notes: ChannelBufferTX
 * --------------------------------------
 * Transmit the data when the channel buffer is non-empty.
 */
void ChannelBuffer::ChannelBufferTX() {
  // reset behavior
  next_layer_valid.write(0);
  for (int i = 0; i < Nin_; ++i) {
    next_layer_data[i].write(Payload(0));
  }
  wait();

  while (true) {
    if (BufferSize() > 0) {
      next_layer_valid.write(1);
      for (int i = 0; i < Nin_; ++i) {
        next_layer_data[i].write(buffer_[i].front());
      }
      // increments the dynamic energy
      dynamic_read_energy_ += memory_model_->DynamicEnergyOfReadOperation();
      do {
        wait();
      } while (!next_layer_rdy.read());

      // pop the data out
      for (int i = 0; i < Nin_; ++i) {
        buffer_[i].pop();
      }
      // deassert the valid
      next_layer_valid.write(0);
    } else {
      // bypass the data from the input to the output
      if (prev_layer_valid.read() && next_layer_rdy.read()) {
        next_layer_valid.write(1);
        for (int i = 0; i < Nin_; ++i) {
          next_layer_data[i].write(prev_layer_data[i].read());
        }
      } else {
        // empty buffer: no data to be transmitted
        next_layer_valid.write(0);
        for (int i = 0; i < Nin_; ++i) {
          next_layer_data[i].write(Payload(0));
        }
      }
      wait();
    }
  }
}

/*
 * Implementation notes: ChannelBufferCombLogic
 * ---------------------------------------------
 * Model the case when there is no buffer between the convolutional layer.
 */
void ChannelBuffer::ChannelBufferCombLogic() {
  // write the input to the output
  next_layer_valid.write(prev_layer_valid.read());
  prev_layer_rdy.write(next_layer_rdy.read());
  for (int i = 0; i < Nin_; ++i) {
    next_layer_data[i].write(prev_layer_data[i].read());
  }
}

void ChannelBuffer::ChannelBufferMonitor() {
  //cout << "Channel buffer: " << sc_get_current_process_b()->get_parent_object()
  //  ->basename() << " with depth " << buffer_[0].size() << endl;
}

/*
 * Implementation notes: Area
 * ---------------------------
 * The area of the channel buffer depends on the memory size, which should be
 * inferred from the system simulation. The maximum buffer size will be recorded
 * during simulation and the final area is determined by the maximum buffer
 * size.
 */
double ChannelBuffer::Area() const {
  // update the memory depth
  const int memory_width = memory_model_->memory_width();
  const int memory_depth = max_buffer_size_;
  const int tech_node = memory_model_->tech_node();
  const double clk_freq = memory_model_->clk_freq();
  // use the updated memory depth
  MemoryModel updated_memory_model(memory_width, memory_depth, tech_node,
      config::ConfigParameter_MemoryType_RAM, clk_freq);
  return updated_memory_model.Area();
}

/*
 * Implementation notes: Power
 * ----------------------------
 * Power is divied into the static power and dynamic power. The static power is
 * a constant for a fixed channel buffer. The dynamic power depends on the
 * running of the system.
 */
double ChannelBuffer::StaticPower() const {
  // update the memory depth
  const int memory_width = memory_model_->memory_width();
  const int memory_depth = max_buffer_size_;
  const int tech_node = memory_model_->tech_node();
  const double clk_freq = memory_model_->clk_freq();
  // use the updated memory depth
  MemoryModel updated_memory_model(memory_width, memory_depth, tech_node,
      config::ConfigParameter_MemoryType_RAM, clk_freq);
  return updated_memory_model.StaticPower();
}

/*
 * Implementation notes: DynamicPower
 * -----------------------------------
 * We use the buffer depth of capacity_ as the memory model for dynamic memory
 * energy. We will scale the result back to the actual buffer depth.
 */
double ChannelBuffer::DynamicPower() const {
  sc_time clock_period = dynamic_cast<const sc_clock*>(clock.get_interface())->
    period();
  sc_time sim_time = sc_time_stamp();
  int total_cycles = sim_time / clock_period;
  // updated memory depth
  const int memory_width = memory_model_->memory_width();
  const int memory_depth = max_buffer_size_;
  const int tech_node = memory_model_->tech_node();
  const double clk_freq = memory_model_->clk_freq();
  if (memory_depth == 0 || memory_width == 0) {
    return 0.;
  }
  MemoryModel updated_memory_model(memory_width, memory_depth, tech_node,
      config::ConfigParameter_MemoryType_RAM, clk_freq);
  double read_energy_ratio = (memory_model_->DynamicEnergyOfReadOperation() > 0)
    ? (updated_memory_model.DynamicEnergyOfReadOperation() /
     memory_model_->DynamicEnergyOfReadOperation()) : 0;
  double write_energy_ratio = (memory_model_->DynamicEnergyOfWriteOperation()
      > 0) ? (updated_memory_model.DynamicEnergyOfWriteOperation() /
        memory_model_->DynamicEnergyOfWriteOperation()) : 0;
  // scale the energy to the correct ammount
  const double dynamic_energy_ = dynamic_write_energy_ * write_energy_ratio +
    dynamic_read_energy_ * read_energy_ratio;
  return dynamic_energy_ / total_cycles;
}

double ChannelBuffer::TotalPower() const {
  return StaticPower() + DynamicPower();
}
