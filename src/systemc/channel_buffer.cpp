/*
 * Filename: channel_buffer.cpp
 * -----------------------------
 * This file implements the channel buffer.
 */

#include "header/systemc/channel_buffer.hpp"
using namespace std;

ChannelBuffer::ChannelBuffer(sc_module_name module_name, int Nin, int capacity)
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

  SC_METHOD(ChannelBufferMonitor);
  sensitive << clock.pos();
}

ChannelBuffer::~ChannelBuffer() {
  delete [] prev_layer_data;
  delete [] next_layer_data;
  delete [] buffer_;
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
      // push the data into channel buffer
      prev_layer_rdy.write(0);
      for (int i = 0; i < Nin_; ++i) {
        buffer_[i].push(prev_layer_data[i]);
      }
      // update the max buffer size
      if (max_buffer_size_ < BufferSize()) {
        max_buffer_size_ = BufferSize();
      }
      // we can eliminate the wait cycle. we add here for the consistency with
      // ConvLayerPe
      wait();
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
      do {
        wait();
      } while (!next_layer_rdy.read());

      // pop the data out
      for (int i = 0; i < Nin_; ++i) {
        buffer_[i].pop();
      }

      // similarly, the following part can be eliminate. We keep it here for the
      // consistency
      next_layer_valid.write(0);
      //for (int i = 0; i < Nin_; ++i) {
      //  next_layer_data[i].write(Payload(0));
      //}
      wait();
    } else {
      // empty buffer: no data to be transmitted
      next_layer_valid.write(0);
      for (int i = 0; i < Nin_; ++i) {
        next_layer_data[i].write(Payload(0));
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
double ChannelBuffer::Area(int bit_width, int tech_node) const {
  // memory size: Nin channel buffer, and each of them stores max_buffer_size_
  // activations in the feature map
  // the addtional max_buffer_size_-1 due to we insert the channel buffer at each
  // split in the inception module, and we can save 1 activations in each split
  if (capacity_ <= 0 || max_buffer_size_ == 0) {
    return 0.;
  }
  const int memory_size = Nin_*(max_buffer_size_)*bit_width;
  // channel buffer is SRAM-based
  MemoryModel memory_model(memory_size/1024., tech_node,
      config::ConfigParameter_MemoryType_RAM);
  return memory_model.Area();
}
