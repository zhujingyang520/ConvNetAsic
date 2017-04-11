/*
 * Filename: verilog_compiler.hpp
 * -------------------------------
 * This file exports the class of verilog compiler. It will automatically
 * generate the RTL code of the top level ConvNet accelerator using the
 * predefined Verilog Code of the basic module.
 */

#ifndef __VERILOG_COMPILER_HPP__
#define __VERILOG_COMPILER_HPP__

#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include "header/caffe/net.hpp"
#include "proto/config.pb.h"

class VerilogCompiler {
  public:
    // constructor
    explicit VerilogCompiler(const Net& net,
        const config::ConfigParameter& config_param);
    // destructor
    ~VerilogCompiler();
    /*
     * Method: GenerateProj
     * ---------------------
     * Generate project. Including RTL, testbench, and simulation settings.
     */
    void GenerateProj(const std::string& folder="./project");

    /*
     * Method: GenerateRTL
     * --------------------
     * Generate all the required RTL file for the specified convnet.
     */
    void GenerateRTL(const std::string& folder);

    /*
     * Method: GenerateTopRTL
     * -----------------------
     * Core method to generate the RTL of the Top module to the specified
     * folder location.
     */
    void GenerateTopRTL(const std::string& folder);

    // Generate memory library
    void GenerateLib(const std::string& folder) const;
    // Generate file list
    void GenerateFlist(const std::string& folder) const;
    // Generate behavior simulation script
    void GenerateSim(const std::string& folder) const;
    // Generate the top simulation file
    void GenerateSimRTL(const std::string& filename) const;

  private:
    // channel buffer module name
    std::vector<std::string> channel_buffer_module_;

  private:
    // parallelism determined (modified from the systemC)
    // instance variable: layer idx -> Pin & Pout
    std::map<int, std::pair<int, int> > parallelism_;
    // initialize the parallelism_ of each layer with the specified inference
    // rate in the ConvNet
    void InitParallelism(const Net& net, int pixel_inference_rate);
    // Return the Pin & Pout given the specified layer parameters
    std::pair<int, int> CalculateParallelsim(int Nin, int Nout, int h, int w,
        int layer_inference_rate) const;
    // Calculate parallelsim using brute force
    std::pair<int, int> CalculateParallelsimBruteForce(int Nin, int Nout,
        double rate) const;

  private:
    // instance variables for the convenet definition
    std::string convnet_name_;    // convnet name
    int Nin_;                     // input feature map depth
    int Nout_;                    // output feature map depth
    int bit_width_;               // bit width of the data path
    const Net* net_;

    // pipeline stage for ALU datapath
    int mult_pipeline_;
    int add_pipeline_;
    int nonlin_pipeline_;
    int pool_pipeline_;
    // inter-channel & inception synchronous buffer depth
    int inter_layer_buffer_depth_;
    int inception_buffer_depth_;

    // interconnections between layer and layer
    // unique interconnections name
    std::vector<std::string> interconnections_name_;
    // name of the interconnections to idx
    std::map<std::string, int> interconnections_to_idx_;

    // Helper functions to generate the RTL file
    void GenerateRTLHeader(std::ostream& os) const;
    void GenerateModuleDef(std::ostream& os) const;
    void GenerateInterconnections(std::ostream& os) const;
    void GenerateInstantiation(std::ostream& os);
    // Sub module generation
    void GenerateInputLayer(std::ostream& os, int layer_id) const;
    void GenerateInterLayerChannelBuffer(std::ostream& os, int layer_id,
        int blob_id);
    void GenerateInceptionChannelBuffer(std::ostream& os, int layer_id,
        int blob_id);
    void GenerateConvolutionLayer(std::ostream& os, int layer_id) const;
    void GeneratePoolingLayer(std::ostream& os, int layer_id) const;
    void GenerateInnerProductLayer(std::ostream& os, int layer_id) const;
    void GenerateSplitLayer(std::ostream& os, int layer_id) const;
    void GenerateConcatLayer(std::ostream& os, int layer_id) const;
    void GenerateOutputLayer(std::ostream& os, int layer_id) const;

    // input spatial 2D dimension
    int input_spatial_dim_;
    // early stop frame size
    int early_stop_frame_size_;
};

#endif
