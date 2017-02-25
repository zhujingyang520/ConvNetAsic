/*
 * Filename: split_pe.hpp
 * -----------------------
 * This file exports the class SplitPe, which is the essential block for the
 * inception module. The module basically splits out the data path together with
 * its handshake protocols.
 */

#ifndef __SPLIT_PE_HPP__
#define __SPLIT_PE_HPP__

#include "header/systemc/data_type.hpp"
#include <systemc.h>

class SplitPe : public sc_module {
  // port
  public:
    // input data of the single bottom blob
    sc_in<bool> prev_layer_valid;
    sc_out<bool> prev_layer_rdy;
    sc_in<Payload>* prev_layer_data;
    // output data of the multiple top blobs
    sc_in<bool>* next_layer_rdy;
    sc_out<bool>* next_layer_valid;
    sc_out<Payload>* next_layer_data;

    SC_HAS_PROCESS(SplitPe);

  private:
    // instance variables
    int Nin_;         // input feature map depth
    int numSplits_;   // the number of output splits

  public:
    // constructor
    explicit SplitPe(sc_module_name module_name, int Nin, int numSplits);
    // destructor
    ~SplitPe();

    // main process
    void SplitPeNextData();
    void SplitPeNextValid();
    void SplitPePrevRdy();

    // helper function to detect all ready
    bool NextLayerAllReady() const;
};

#endif
