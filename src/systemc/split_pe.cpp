/*
 * Filename: split_pe.cpp
 * ------------------------
 * This file implements the class SplitPe.
 */

#include "header/systemc/split_pe.hpp"
using namespace std;

/*
 * Implementation notes: Constructor
 * ----------------------------------
 * The constructor allocates the ports of the block.
 */
SplitPe::SplitPe(sc_module_name module_name, int Nin, int numSplits):
  sc_module(module_name), Nin_(Nin), numSplits_(numSplits) {
  // sanity check
  assert(numSplits > 1);
  // allocate the ports
  prev_layer_data = new sc_in<Payload> [Nin_];
  next_layer_rdy = new sc_in<bool> [numSplits_];
  next_layer_valid = new sc_out<bool> [numSplits_];
  next_layer_data = new sc_out<Payload> [numSplits_*Nin_];

  // the main process of the split process
  // data path: bypass the previous layer
  SC_METHOD(SplitPeNextData);
  for (int i = 0; i < Nin_; ++i) {
    sensitive << prev_layer_data[i];
  }

  // ready path: AND of next ready signals
  SC_METHOD(SplitPePrevRdy);
  for (int i = 0; i < numSplits_; ++i) {
    sensitive << next_layer_rdy[i];
  }

  // valid path: transmit the previous valid iff all next layers are ready
  SC_METHOD(SplitPeNextValid);
  sensitive << prev_layer_valid;
  for (int i = 0; i < numSplits_; ++i) {
    sensitive << next_layer_rdy[i];
  }
}

SplitPe::~SplitPe() {
  delete [] prev_layer_data;
  delete [] next_layer_rdy;
  delete [] next_layer_valid;
  delete [] next_layer_data;
}

/*
 * Implementation notes: SplitPeNextData
 * --------------------------------------
 * Fanout the previous layer data to the next layer data.
 */
void SplitPe::SplitPeNextData() {
  for (int i = 0; i < numSplits_; ++i) {
    for (int j = 0; j < Nin_; ++j) {
      next_layer_data[i*Nin_+j].write(prev_layer_data[i].read());
    }
  }
}

/*
 * Implementation notes: SplitPePrevRdy
 * -------------------------------------
 * AND all the next layer ready signals.
 */
void SplitPe::SplitPePrevRdy() {
  prev_layer_rdy.write(1);
  for (int i = 0; i < numSplits_; ++i) {
    // deassert the ready signal when one of the next_layer_rdys is false
    if (!next_layer_rdy[i].read()) {
      prev_layer_rdy.write(0);
      break;
    }
  }
}

/*
 * Implementation notes: SplitPeNextValid
 * ---------------------------------------
 * Deassert the valid if the next layer is ready. Only bypass the previous valid
 * when all the following layers are ready.
 */
void SplitPe::SplitPeNextValid() {
  if (NextLayerAllReady()) {
    for (int i = 0; i < numSplits_; ++i) {
      next_layer_valid[i].write(prev_layer_valid);
    }
  } else {
    for (int i = 0; i < numSplits_; ++i) {
      next_layer_valid[i].write(0);
    }
  }
}

bool SplitPe::NextLayerAllReady() const {
  for (int i = 0; i < numSplits_; ++i) {
    if (!next_layer_rdy[i].read()) {
      return false;
    }
  }
  return true;
}
