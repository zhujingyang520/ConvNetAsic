/*
 * Filename: concat_pe.hpp
 * ------------------------
 * This file exports the class ConcatPe, which is the essential block for the
 * inception module. The module basically merge the data path together with its
 * handshake protocols.
 */

#ifndef __CONCAT_PE_HPP__
#define __CONCAT_PE_HPP__

#include "header/systemc/data_type.hpp"
#include <systemc.h>
#include <vector>

class ConcatPe : public sc_module {
  // ports
  public:
    // input data of the multiple bottom blobs
    sc_in<bool>* prev_layer_valid;
    sc_out<bool>* prev_layer_rdy;
    sc_in<Payload>* prev_layer_data;
    // output data of the single top blob
    sc_in<bool> next_layer_rdy;
    sc_out<bool> next_layer_valid;
    sc_out<Payload>* next_layer_data;

    SC_HAS_PROCESS(ConcatPe);

  private:
    // instance variables
    int Nin_;         // concatenated feature map depth
    int numSplits_;   // the number of input splits

  public:
    // constructor
    explicit ConcatPe(sc_module_name module_name, int Nin, int numSplits);
    // destructor
    ~ConcatPe();

    // main process
    void ConcatPeNextData();
    void ConcatPeNextValid();
    void ConcatPePreRdy();

    // helper function to detect all valid
    bool PrevLayerAllValid() const;
};

#endif
