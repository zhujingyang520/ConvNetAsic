/*
 * Filename: concat_pe.cpp
 * ------------------------
 * This file implements the class ConcatPe.
 */

#include "header/systemc/concat_pe.hpp"
using namespace std;

/*
 * Implementation notes: Constructor
 * ----------------------------------
 * The constructor allocates the ports of the block.
 */
ConcatPe::ConcatPe(sc_module_name module_name, int Nin, int numSplits) :
  sc_module(module_name), Nin_(Nin), numSplits_(numSplits) {
  // sanity check: the number of bottom blobs should be greater than 1
  assert(numSplits > 1);
  // allocates the ports
  prev_layer_valid = new sc_in<bool> [numSplits];
  prev_layer_rdy = new sc_out<bool> [numSplits];
  prev_layer_data = new sc_in<Payload> [Nin];
  next_layer_data = new sc_out<Payload> [Nin];

  // the main process of the concat process
  SC_METHOD(ConcatPeNextData);
  for (int i = 0; i < Nin; ++i) {
    sensitive << prev_layer_data[i];
  }

  SC_METHOD(ConcatPeNextValid);
  for (int i = 0; i < numSplits; ++i) {
    sensitive << prev_layer_valid[i];
  }

  SC_METHOD(ConcatPePreRdy);
  sensitive << next_layer_rdy;
  for (int i = 0; i < numSplits; ++i) {
    sensitive << prev_layer_valid[i];
  }
}

ConcatPe::~ConcatPe() {
  delete [] prev_layer_valid;
  delete [] prev_layer_rdy;
  delete [] prev_layer_data;
  delete [] next_layer_data;
}

/*
 * Implementation notes: ConcatPeNextData
 * ---------------------------------------
 * Bypass the layer data from the previous layer to the next layer.
 */
void ConcatPe::ConcatPeNextData() {
  for (int i = 0; i < Nin_; ++i) {
    next_layer_data[i].write(prev_layer_data[i].read());
  }
}

/*
 * Implementation notes: ConcatPeNextValid
 * ----------------------------------------
 * AND all the previous valid signals.
 */
void ConcatPe::ConcatPeNextValid() {
  next_layer_valid.write(1);
  for (int i = 0; i < numSplits_; ++i) {
    if (!prev_layer_valid[i].read()) {
      next_layer_valid.write(0);
      break;
    }
  }
}

/*
 * Implementation notes: ConcatPePreRdy
 * -------------------------------------
 * Bypass the next layer ready when all the proceeding valids are asserted.
 */
void ConcatPe::ConcatPePreRdy() {
  if (PrevLayerAllValid()) {
    // Fanout the ready signals
    for (int i = 0; i < numSplits_; ++i) {
      prev_layer_rdy[i].write(next_layer_rdy.read());
    }
  } else {
    for (int i = 0; i < numSplits_; ++i) {
      prev_layer_rdy[i].write(0);
    }
  }
}

bool ConcatPe::PrevLayerAllValid() const {
  for (int i = 0; i < numSplits_; ++i) {
    if (!prev_layer_valid[i].read()) {
      return false;
    }
  }
  return true;
}
