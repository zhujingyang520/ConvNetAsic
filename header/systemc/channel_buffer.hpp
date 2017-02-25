/*
 * Filename: channel_buffer.hpp
 * -----------------------------
 * This file exports the channel buffer, which buffers the feature map pixels to
 * synchronize the uneven datapath in the inception module.
 */

#ifndef __CHANNEL_BUFFER_HPP__
#define __CHANNEL_BUFFER_HPP__

#include "header/systemc/data_type.hpp"
#include "header/systemc/models/memory_model.hpp"
#include <queue>
#include <limits.h>
#include <systemc.h>

class ChannelBuffer : public sc_module {
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

    SC_HAS_PROCESS(ChannelBuffer);

    inline int BufferSize() const {
      return buffer_[0].size();
    }

    inline int MaxBufferSize() const {
      return max_buffer_size_;
    }

    // area model of the channel buffer
    double Area(int bit_width, int tech_node=28) const;

  private:
    int Nin_;                 // input channel depth
    int capacity_;            // max channel buffer capacity
    std::queue<Payload>* buffer_;
    int max_buffer_size_;     // max buffer size in the simulation

  public:
    // constructor
    explicit ChannelBuffer(sc_module_name module_name, int Nin,
        int capacity=INT_MAX);
    // destructor
    ~ChannelBuffer();

    // channel buffer main process
    void ChannelBufferRX();   // channel buffer receiving process
    void ChannelBufferTX();   // channel buffer transmitting process

    // speical condition for capacity == 0 (purely combinational logic)
    void ChannelBufferCombLogic();

    void ChannelBufferMonitor();
};

#endif
