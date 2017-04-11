/*
 * Filename: verilog_compiler.cpp
 * -------------------------------
 * This file implements the class VerilogCompiler.
 */

#include "header/verilog/verilog_compiler.hpp"
#include "header/caffe/layer.hpp"
#include "header/caffe/layers/conv_layer.hpp"
#include "header/caffe/layers/inner_product_layer.hpp"
#include "header/caffe/layers/pooling_layer.hpp"
#include <iostream>
#include <fstream>
#include <ctime>
#include <sys/stat.h>
#include <sstream>
#include <math.h>

using namespace std;
using namespace config;

/*
 * Implementation notes: Contructor
 * ---------------------------------
 * Initialize the basic parameters from the configuration file and caffe
 * network.
 */
VerilogCompiler::VerilogCompiler(const Net& net, const ConfigParameter&
    config_param) {
  // pass the network defintion to the net_
  net_ = &net;

  // ConvNet name (replace any space with the underscore)
  convnet_name_ = net.name_;
  for(string::iterator iter = convnet_name_.begin(); iter != convnet_name_.end();
      ++iter) {
    *iter = (*iter == ' ') ? '_' : *iter;
  }

  // TODO: bit width of the data path (from the configuration network)
  bit_width_ = config_param.bit_width();
  // TODO: pipeline stage for a better performace
  mult_pipeline_ = 2;
  add_pipeline_ = 2;
  nonlin_pipeline_ = 1;
  pool_pipeline_ = 1;
  // TODO: buffer depth
  // in configuration file: inter-layer buffer depth is append buffer capacity
  inter_layer_buffer_depth_ = config_param.append_buffer_capacity();
  inception_buffer_depth_ = config_param.inception_buffer_depth();

  // TODO: determine the parallelism
  InitParallelism(net, config_param.pixel_inference_rate());

  // early stop frame size
  early_stop_frame_size_ = config_param.early_stop_frame_size();

  // parse the input & output layers' dimension
  assert(net.layers_[0]->layer_param().type() == "Input");
  Nin_ = net.top_blobs_shape_ptr_[0][0]->at(1);
  const int output_layer_idx = net.layers_.size() - 1;
  // we only expect a single output blob for the inference model
  assert(net.top_blobs_shape_ptr_[output_layer_idx].size() == 1);
  Nout_ = net.top_blobs_shape_ptr_[output_layer_idx][0]->at(1);

  // iterate through all layers to obtain the interconnections: deal with non
  // in-place normalization layer
  for (size_t layer_id = 0; layer_id < net.layers_.size(); ++layer_id) {
    const Layer* layer = net.layers_[layer_id];

    if (layer->layer_param().type() == "Input") {
      const string interconnection = layer->layer_param().top(0);
      interconnections_name_.push_back(interconnection);
      interconnections_to_idx_[interconnection] =
        interconnections_name_.size() - 1;
    } else if (layer->layer_param().type() == "Convolution") {
      const string interconnection = layer->layer_param().top(0);
      interconnections_name_.push_back(interconnection);
      interconnections_to_idx_[interconnection] =
        interconnections_name_.size() - 1;
    } else if (layer->layer_param().type() == "InnerProduct") {
      const string interconnection = layer->layer_param().top(0);
      interconnections_name_.push_back(interconnection);
      interconnections_to_idx_[interconnection] =
        interconnections_name_.size() - 1;
    } else if (layer->layer_param().type() == "Pooling") {
      const string interconnection = layer->layer_param().top(0);
      interconnections_name_.push_back(interconnection);
      interconnections_to_idx_[interconnection] =
        interconnections_name_.size() - 1;
    } else if (layer->layer_param().type() == "Split") {
      // split layer typically has multiple top blobs
      for (int blob_id = 0; blob_id < layer->layer_param().top_size();
          ++blob_id) {
        const string interconnection = layer->layer_param().top(blob_id);
        interconnections_name_.push_back(interconnection);
        interconnections_to_idx_[interconnection] =
          interconnections_name_.size() - 1;
      }
    } else if (layer->layer_param().type() == "Concat") {
      const string interconnection = layer->layer_param().top(0);
      interconnections_name_.push_back(interconnection);
      interconnections_to_idx_[interconnection] =
        interconnections_name_.size() - 1;
    } else {
      // bypass the remaining layers, include dropout, normalization
      const string prev_connection = layer->layer_param().bottom(0);
      const int prev_connection_idx = interconnections_to_idx_[prev_connection];
      for (int top_id = 0; top_id < layer->layer_param().top_size(); ++top_id) {
        const string next_connection = layer->layer_param().top(top_id);
        if (prev_connection != next_connection) {
          // deal with non-inplace connection: map to the previous connection
          // name
          interconnections_to_idx_[next_connection] = prev_connection_idx;
        }
      }
    }
  }
}

VerilogCompiler::~VerilogCompiler() {
}

void VerilogCompiler::InitParallelism(const Net& net, int pixel_inference_rate) {
  // the actual pixel inference rate is:
  // - CONV: ceil(Nin/Pin) * ceil(Nout/Pout) + ALU datapath overhead
  // - POOL: ceil(Nin/Pin) + ALU datapath overhead

  // target layer inference rate (inferred from the input layer)
  int layer_inference_rate;
  // max (worst) layer inference rate
  int max_layer_inference_rate = 0;
  // store the layer inference rate of every layer in ConvNet
  vector< pair<int, int> > layer_inference_rate_array;
  for (size_t layer_id = 0; layer_id < net_->layers_.size(); ++layer_id) {
    const Layer* layer = net_->layers_[layer_id];
    if (layer->layer_param().type() == "Input") {
      // top blob dimension: (N, C, H, W)
      const int h = net_->top_blobs_shape_ptr_[layer_id][0]->at(2);
      const int w = net_->top_blobs_shape_ptr_[layer_id][0]->at(3);
      input_spatial_dim_ = h * w;
      layer_inference_rate = (h * w) * pixel_inference_rate;
      cout << "Inferred the layer inference rate: " << layer_inference_rate
        << endl;
      continue;
    } else if (layer->layer_param().type() == "Convolution") {
      const int Nin = dynamic_cast<const ConvolutionLayer*>(layer)->num_input_;
      const int Nout = dynamic_cast<const ConvolutionLayer*>(layer)->num_output_;
      const int h = net_->top_blobs_shape_ptr_[layer_id][0]->at(2);
      const int w = net_->top_blobs_shape_ptr_[layer_id][0]->at(3);
      parallelism_[layer_id] = CalculateParallelsim(Nin, Nout, h, w,
          layer_inference_rate);
      // obtain the calculated parallelism
      const int Pin = parallelism_[layer_id].first;
      const int Pout = parallelism_[layer_id].second;
      // pipeline includes: memory access, multiplier, adder, nonlinear, wb
      const int datapath_pipeline = 1 + mult_pipeline_ + add_pipeline_
        + nonlin_pipeline_ + 1;
      const int inference_rate = (ceil(static_cast<double>(Nin)/Pin) *
        ceil(static_cast<double>(Nout)/Pout) + datapath_pipeline) * h * w;
      if (max_layer_inference_rate < inference_rate) {
        max_layer_inference_rate = inference_rate;
      }
      layer_inference_rate_array.push_back(make_pair(layer_id, inference_rate));
    } else if (layer->layer_param().type() == "Pooling") {
      const int Nin = dynamic_cast<const PoolingLayer*>(layer)->num_input_;
      const int Nout = 0;   // reserve pooling's Nout = 0
      const int h = net_->top_blobs_shape_ptr_[layer_id][0]->at(2);
      const int w = net_->top_blobs_shape_ptr_[layer_id][0]->at(3);
      parallelism_[layer_id] = CalculateParallelsim(Nin, Nout, h, w,
          layer_inference_rate);
      // obtain the calculated parallelism
      const int Pin = parallelism_[layer_id].first;
      // pipeline includes: mux array, pooling, write back
      const int datapath_pipeline = 1 + pool_pipeline_ + 1;
      const int inference_rate = (ceil(static_cast<double>(Nin)/Pin) +
          datapath_pipeline) * h * w;
      if (max_layer_inference_rate < inference_rate) {
        max_layer_inference_rate = inference_rate;
      }
      layer_inference_rate_array.push_back(make_pair(layer_id, inference_rate));
    } else if (layer->layer_param().type() == "InnerProduct") {
      const int Nin = net_->bottom_blobs_shape_ptr_[layer_id][0]->at(1);
      const int Nout = dynamic_cast<const InnerProductLayer*>(layer)->
        num_output_;
      // inner-product layer has the output feature map dim = 1
      const int h = 1;
      const int w = 1;
      parallelism_[layer_id] = CalculateParallelsim(Nin, Nout, h, w,
          layer_inference_rate);
      // obtain the calculated parallelism
      const int Pin = parallelism_[layer_id].first;
      const int Pout = parallelism_[layer_id].second;
      // pipeline includes: memory access, multiplier, adder, nonlinear, wb
      const int datapath_pipeline = 1 + mult_pipeline_ + add_pipeline_
        + nonlin_pipeline_ + 1;
      const int inference_rate = (ceil(static_cast<double>(Nin)/Pin) *
        ceil(static_cast<double>(Nout)/Pout) + datapath_pipeline) * h * w;
      if (max_layer_inference_rate < inference_rate) {
        max_layer_inference_rate = inference_rate;
      }
      layer_inference_rate_array.push_back(make_pair(layer_id, inference_rate));
    } else {
      continue;
    }
    cout << "- Layer[" << layer_id << "]: " << layer->layer_param().name()
      << " Pin: " << parallelism_[layer_id].first << " Pout: "
      << parallelism_[layer_id].second << endl;
  }
  // print all the resource allocation information
  cout << "#######################################" << endl;
  cout << "# Resulted layer inference" << endl;
  cout << "#######################################" << endl;
  for (size_t i = 0; i < layer_inference_rate_array.size(); ++i) {
    const int layer_id = layer_inference_rate_array[i].first;
    const int inference_rate = layer_inference_rate_array[i].second;
    cout << "- " << net_->layers_[layer_id]->layer_param().name() << " with "
      << "rate " << inference_rate << endl;
  }
  cout << "Max layer inference rate: " << max_layer_inference_rate << endl;
  cout << "Press ENTER to continue";
  cin.get();
}

pair<int, int> VerilogCompiler::CalculateParallelsim(int Nin, int Nout, int h,
    int w, int layer_inference_rate) const {
  // regularize the layer inference rate
  if (layer_inference_rate <= 0) {
    layer_inference_rate = 1;
  }

  if (Nout == 0) {
    // pooling layer: (ceil(Nin/Pin) + datapath_pipeline)*h*w =
    // layer_inference_rate
    const int datapath_pipeline = 1 + pool_pipeline_ + 1;
    double rate = static_cast<double>(layer_inference_rate) / (h * w) -
      datapath_pipeline;
    if (rate <= 0) rate = 1;
    // infer the Pin
    int Pin = round(static_cast<double>(Nin) / rate);
    if (Pin <= 0) Pin = 1;
    if (Pin > Nin) Pin = Nin;
    return make_pair(Pin, 0);
  } else {
    // CONV or FC: (ceil(Nin/Pin)*ceil(Nout/Pout) + datapath_pipeline) * h * w
    // = layer_inference_rate
    const int datapath_pipeline = 1 + mult_pipeline_ + add_pipeline_ +
      nonlin_pipeline_ + 1;
    double rate = static_cast<double>(layer_inference_rate) / (h * w) -
      datapath_pipeline;
    if (rate <= 0) rate = 1;
    return CalculateParallelsimBruteForce(Nin, Nout, rate);
  }
}

// Search for the (Pin, Pout) which provides the closest results:
//  ceil(Nin/Pin) * ceil(Nout/Pout) = rate
// Necessary condition: ceil(Nout/Pout) >= 1 + nonlin_pipeline_
pair<int, int> VerilogCompiler::CalculateParallelsimBruteForce(int Nin,
    int Nout, double rate) const {
  // search through all possible combinations of Pin & Pout
  double min = Nin * Nout;
  int Pin = 1;
  int Pout = 1;
  for (int Pin_ = 1; Pin_ <= Nin; ++Pin_) {
    for (int Pout_ = 1; Pout_ <= Nout; ++Pout_) {
      double current_rate = ceil(static_cast<double>(Nin)/Pin_) *
        ceil(static_cast<double>(Nout)/Pout_);
      if (abs(current_rate - rate) < min &&
          ceil(static_cast<double>(Nout)/Pout_) >= (1 + nonlin_pipeline_)) {
        min = abs(current_rate - rate);
        Pin = Pin_;
        Pout = Pout_;
      }
    }
  }
  // sanity check: make sure the dependency relationship holds
  if (ceil(static_cast<double>(Nout)/Pout) < 1 + nonlin_pipeline_) {
    cout << "[ERROR]: the resource allocation has the dependency issue"
      << endl;
    exit(1);
  }
  return make_pair(Pin, Pout);
}

void VerilogCompiler::GenerateProj(const string& folder) {
  // clear the old folder if exists
  string cmd;
  cmd = "rm -fr " + folder;
  system(cmd.c_str());
  // create new project file
  const string proj_folder = folder;
  const string rtl_folder = proj_folder + "/rtl/";
  const string flist_folder = proj_folder + "/flist/";
  const string sim_folder = proj_folder + "/sim/";
  const string lib_folder = proj_folder + "/lib/";
  cmd = "mkdir -p " + proj_folder;
  system(cmd.c_str());
  cmd = "mkdir -p " + rtl_folder;
  system(cmd.c_str());
  cmd = "mkdir -p " + flist_folder;
  system(cmd.c_str());
  cmd = "mkdir -p " + sim_folder;
  system(cmd.c_str());
  cmd = "mkdir -p " + lib_folder;
  system(cmd.c_str());

  // Generate the RTL of the project
  GenerateRTL(rtl_folder);
  // Generate the library of the project
  GenerateLib(lib_folder);
  // Generate the flist
  GenerateFlist(flist_folder);
  // Generate the simulation script
  GenerateSim(sim_folder);
}

void VerilogCompiler::GenerateLib(const string& folder) const {
  // copy the memory library to the folder
  string cmd = "cp -r ./src/verilog/rom " + folder;
  system(cmd.c_str());
}

void VerilogCompiler::GenerateSim(const string& folder) const {
  // write the simple shell script sample to invoke the VCS simulation
  const string filename = "Makefile";
  ofstream outFile((folder+"/"+filename).c_str());
  if (!outFile) {
    cerr << "[ERROR]: vcs simulation file can NOT open!" << endl;
    exit(1);
  }

  // copy the testbench to the simulation file
  string cmd = "cp ./src/verilog/basic_modules/testbench.v " + folder;
  system(cmd.c_str());
  // Generate the top wrapper for behavior simulation
  const string top_filename = convnet_name_ + "_top.v";
  GenerateSimRTL(folder + "/" + top_filename);

  outFile << "#######################################" << endl;
  outFile << "# Sampled Makefile for the simulation" << endl;
  outFile << "# (TODO) modify the path" << endl;
  outFile << "#######################################" << endl;
  outFile << "VCS_BIN := vcs -full64" << endl;
  outFile << "VCS_OPTION := +v2k -debug_all" << endl;
  outFile << "MACRO := BEHAV_SIM" << endl;
  outFile << "FLIST := " << endl;
  outFile << "SEARCH_PATH := " << endl;
  outFile << "LIB_PATH := " << endl;
  outFile << "TESTBENCH_RTL := " << top_filename << " testbench.v"<< endl;
  outFile << "TIME_SCALE := 1ns/10ps" << endl;
  outFile << "run:" << endl;
  outFile << "\t$(VCS_BIN) $(VCS_OPTION) -top " << convnet_name_+"_top"
    << " -f $(FLIST) $(TESTBENCH_RTL) +define+$(MACRO)"
    << " -timescale=$(TIME_SCALE) +incdir+$(SEARCH_PATH)"
    << " -v $(LIB_PATH)/rom_full_ones.v"
    << " -v $(LIB_PATH)/rom_full_zeros.v" << endl << endl;
  outFile << "clean:" << endl;
  outFile << "\trm -fr csrc simv simv.daidir" << endl;
  outFile << "\trm -fr DVE* ucli* *.vpd" << endl;
  outFile.close();
}

void VerilogCompiler::GenerateFlist(const string& folder) const {
  // write the file list to the folder/*.flist
  const string filename = convnet_name_ + ".flist";
  ofstream outFile((folder+"/"+filename).c_str());
  if (!outFile) {
    cerr << "[ERROR]: File list file can NOT open!" << endl;
    exit(1);
  }

  // wirte all the basic modules (except testbench)
  outFile << "##################################" << endl;
  outFile << "# Generate by Verilog Compiler" << endl;
  outFile << "# (TODO) Edit the relative path" << endl;
  outFile << "##################################" << endl;
  // write the top module
  outFile << convnet_name_+".v" << endl;
  outFile << "add_array.v" << endl;
  outFile << "channel_buffer.v" << endl;
  outFile << "concat.v" << endl;
  outFile << "conv_layer_ctrl.v" << endl;
  outFile << "conv_layer_pe.v" << endl;
  outFile << "fifo_sync.v" << endl;
  outFile << "kernel_mem.v" << endl;
  outFile << "line_buffer.v" << endl;
  outFile << "line_buffer_array.v" << endl;
  outFile << "merge_network.v" << endl;
  outFile << "mult_array.v" << endl;
  outFile << "multiplier.v" << endl;
  outFile << "mux.v" << endl;
  outFile << "mux_array.v" << endl;
  outFile << "nonlinear.v" << endl;
  outFile << "out_regfile.v" << endl;
  outFile << "pool_array.v" << endl;
  outFile << "pool_layer_ctrl.v" << endl;
  outFile << "pool_layer_pe.v" << endl;
  outFile << "row_buffer.v" << endl;
  outFile << "split.v" << endl;

  outFile.close();
}

void VerilogCompiler::GenerateSimRTL(const string& filename) const {
  ofstream outFile(filename.c_str());
  if (!outFile) {
    cerr << "[ERROR]: unable to open the simulate RTL file!" << endl;
    exit(1);
  }
  outFile << "// --------------------------------------" << endl;
  outFile << "// Generated by Verilog Compiler" << endl;
  outFile << "// --------------------------------------" << endl;
  outFile << "module " << convnet_name_ << "_top;" << endl << endl;
  outFile << "localparam CLK_PERIOD = 1.0;" << endl << endl;
  outFile << "// Interconnections" << endl;
  outFile << "reg clk, rst, enable;" << endl;
  outFile << "wire input_layer_valid, input_layer_rdy;" << endl;
  outFile << "wire [" << Nin_*bit_width_-1 << ":0] input_layer_data;" << endl;
  outFile << "wire [" << bit_width_-1 << ":0] input_layer_data_2d ["
    << Nin_-1 << ":0];" << endl;
  outFile << "wire output_layer_valid, output_layer_rdy;" << endl;
  outFile << "wire [" << Nout_*bit_width_-1 << ":0] output_layer_data;" << endl;
  outFile << "wire [" << bit_width_-1 << ":0] output_layer_data_2d ["
    << Nout_-1 << ":0];" << endl;
  outFile << "wire stop_sim;" << endl;
  outFile << "genvar i;" << endl;
  outFile << endl;

  outFile << "// testbench" << endl;
  outFile << "testbench #(" << endl;
  outFile << "\t.Nin (" << Nin_ << ")," << endl;
  outFile << "\t.Nout (" << Nout_ << ")," << endl;
  outFile << "\t.BIT_WIDTH (" << bit_width_ << ")," << endl;
  outFile << "\t.INPUT_SPATIAL_DIM (" << input_spatial_dim_ << ")," << endl;
  outFile << "\t.FRAME_SIZE (" << early_stop_frame_size_ << ")" << endl;
  outFile << ") testbench_inst (" << endl;
  outFile << "\t.clk (clk)," << endl;
  outFile << "\t.rst (rst)," << endl;
  outFile << "\t.input_layer_rdy (input_layer_rdy)," << endl;
  outFile << "\t.input_layer_valid (input_layer_valid)," << endl;
  outFile << "\t.input_layer_data (input_layer_data)," << endl;
  outFile << "\t.output_layer_rdy (output_layer_rdy)," << endl;
  outFile << "\t.output_layer_valid (output_layer_valid)," << endl;
  outFile << "\t.output_layer_data (output_layer_data)," << endl;
  outFile << "\t.stop_sim (stop_sim)" << endl;
  outFile << ");" << endl;

  outFile << "// UUT" << endl;
  outFile << convnet_name_ << " uut (" << endl;
  outFile << "\t.clk (clk)," << endl;
  outFile << "\t.rst (rst)," << endl;
  outFile << "\t.enable (enable)," << endl;
  outFile << "\t.input_layer_valid (input_layer_valid)," << endl;
  outFile << "\t.input_layer_rdy (input_layer_rdy)," << endl;
  outFile << "\t.input_layer_data (input_layer_data)," << endl;
  outFile << "\t.output_layer_valid (output_layer_valid)," << endl;
  outFile << "\t.output_layer_rdy (output_layer_rdy)," << endl;
  outFile << "\t.output_layer_data (output_layer_data)" << endl;
  outFile << ");" << endl;

  outFile << "// 2D data unpack" << endl;
  outFile << "generate" << endl;
  outFile << "for (i = 0; i < " << Nin_ << "; i = i + 1) begin:"
    " input_data_unpack" << endl;
  outFile << "\tassign input_layer_data_2d[i] = input_layer_data[i*"
    << bit_width_ << "+:" << bit_width_ << "];" << endl;
  outFile << "end" << endl;
  outFile << "for (i = 0; i < " << Nout_ << "; i = i + 1) begin:"
    " output_layer_unpack" << endl;
  outFile << "\tassign output_layer_data_2d[i] = output_layer_data[i*"
    << bit_width_ << "+:" << bit_width_ << "];" << endl;
  outFile << "end" << endl;
  outFile << "endgenerate" << endl;

  outFile << endl;
  outFile << "// clock generation" << endl;
  outFile << "initial begin" << endl;
  outFile << "\tclk = 1'b0;" << endl;
  outFile << "\tforever #(CLK_PERIOD/2.0) clk = ~clk;" << endl;
  outFile << "end" << endl;
  outFile << "// rst & enable" << endl;
  outFile << "initial begin" << endl;
  outFile << "\trst = 1'b1;" << endl;
  outFile << "\tenable = 1'b0;" << endl;
  outFile << "\t#(CLK_PERIOD*5);" << endl;
  outFile << "\t@(negedge clk);" << endl;
  outFile << "\trst = 1'b0;" << endl;
  outFile << "\tenable = 1'b1;" << endl;
  outFile << "end" << endl;

  // keep track of the buffer depth
  outFile << endl;
  outFile << "// record the max buffer depth to the file" << endl;
  outFile << "integer fp;" << endl;
  outFile << "initial begin" << endl;
  outFile << "\tfp = $fopen(\"buffer_depth.list\", \"w\");" << endl;
  outFile << "\twait(stop_sim == 1'b1);" << endl;
  for (vector<string>::const_iterator iter = channel_buffer_module_.begin();
      iter != channel_buffer_module_.end(); ++iter) {
    outFile << "\t$fdisplay(fp, \"" << *iter << " %d\", uut."
      << *iter << ".genblk1.fifo_sync_inst.fifo_max_depth);" << endl;
  }
  outFile << "\t$fclose(fp);" << endl;
  outFile << "\t$finish;" << endl;
  outFile << "end" << endl;

  outFile << "endmodule" << endl;
  outFile.close();
}

void VerilogCompiler::GenerateRTL(const string& folder) {
  // copy all the basic RTL files from the basic modules to the target folder
  string cmd = "cp ./src/verilog/basic_modules/*.v " + folder;
  system(cmd.c_str());

  // generate the top RTL file
  GenerateTopRTL(folder);
}

void VerilogCompiler::GenerateTopRTL(const string& folder) {
  cout << "##################################################" << endl;

  // check the existence of folder
  struct stat s;
  if (stat(folder.c_str(), &s) != 0 || !S_ISDIR(s.st_mode)) {
    cout << "# Generate the specified folder " << folder << endl;
    const string mkdir_cmd = "mkdir -p " + folder;
    system(mkdir_cmd.c_str());
  }
  const string filename = folder + "/" + convnet_name_ + ".v";
  cout << "# Dump the RTL to " << filename << "..." << endl;
  ofstream outfile(filename.c_str(), ofstream::out);
  if (!outfile) {
    cerr << "unexpected file dump error" << endl;
    exit(1);
  }

  // generate header info of the RTL
  GenerateRTLHeader(outfile);
  // generate the module definition
  GenerateModuleDef(outfile);
  // generate the interconnections declaration
  GenerateInterconnections(outfile);
  // generate the instantiation of each module
  GenerateInstantiation(outfile);

  // generate the end module statement
  outfile << "endmodule" << endl;
  outfile.close();
  cout << "# Done!" << endl;
  cout << "##################################################" << endl;
}

void VerilogCompiler::GenerateRTLHeader(ostream &os) const {
  // get the current time for generating RTL
  // ref: http://stackoverflow.com/questions/997946/
  // how-to-get-current-time-and-date-in-c
  time_t now = time(0);
  struct tm tstruct = *localtime(&now);
  char buf[80];
  strftime(buf, sizeof(buf), "%Y-%m-%d: %X", &tstruct);
  os << "// ----------------------------------------------" << endl;
  os << "// This file is generated by Verilog Compiler" << endl;
  os << "// Filename: " << convnet_name_ << ".v" << endl;
  os << "// Time: " << buf << endl;
  os << "// Copyright by JY" << endl;
  os << "// Contact: jzhuak@connect.ust.hk" << endl;
  os << "// ----------------------------------------------" << endl;
}

void VerilogCompiler::GenerateModuleDef(ostream& os) const {
  os << "module " << convnet_name_ << " (" << endl;
  os << "\tinput wire clk," << endl;
  os << "\tinput wire rst," << endl;
  os << "\tinput wire enable," << endl;
  os << "\t// input layer data & handshake" << endl;
  os << "\tinput wire input_layer_valid," << endl;
  os << "\toutput wire input_layer_rdy," << endl;
  os << "\tinput wire [" << Nin_*bit_width_-1 << ":0] input_layer_data,"
    << endl;
  os << "\t// output layer data & handshake" << endl;
  os << "\toutput wire output_layer_valid," << endl;
  os << "\tinput wire output_layer_rdy," << endl;
  os << "\toutput wire [" << Nout_*bit_width_-1 << ":0] output_layer_data"
    << endl;
  os << ");" << endl;
  os << endl << endl;
}

void VerilogCompiler::GenerateInterconnections(ostream& os) const {

  os << "// ----------------------------------------------" << endl;
  os << "// Interconnections" << endl;
  os << "// ----------------------------------------------" << endl;
  // iterate through all layers to obtain the interconnections
  for (size_t layer_id = 0; layer_id < net_->layers_.size(); ++layer_id) {
    const Layer* layer = net_->layers_[layer_id];

    if (layer->layer_param().type() == "Input") {
      const string interconnection = layer->layer_param().top(0);
      const int top_depth = net_->top_blobs_shape_ptr_[layer_id][0]->at(1);
      os << "wire " << interconnection << "_valid;" << endl;
      os << "wire " << interconnection << "_rdy;" << endl;
      os << "wire [" << top_depth*bit_width_-1 << ":0] " << interconnection
        << "_data;" << endl;
    } else if (layer->layer_param().type() == "Convolution" ||
        layer->layer_param().type() == "Pooling" ||
        layer->layer_param().type() == "InnerProduct") {
      const string interconnection = layer->layer_param().top(0);
      const int top_depth = net_->top_blobs_shape_ptr_[layer_id][0]->at(1);
      os << "wire " << interconnection << "_valid;" << endl;
      os << "wire " << interconnection << "_rdy;" << endl;
      os << "wire [" << top_depth*bit_width_-1 << ":0] " << interconnection
        << "_data;" << endl;
      // for inter-channel buffer
      os << "wire " << interconnection << "_inter_layer_buffer_valid;" << endl;
      os << "wire " << interconnection << "_inter_layer_buffer_rdy;" << endl;
      os << "wire [" << top_depth*bit_width_-1 << ":0] " << interconnection
        << "_inter_layer_buffer_data;" << endl;
    } else if (layer->layer_param().type() == "Split") {
      // split layer typically has multiple top blobs
      for (int blob_id = 0; blob_id < layer->layer_param().top_size();
          ++blob_id) {
        const string interconnection = layer->layer_param().top(blob_id);
        const int top_depth = net_->top_blobs_shape_ptr_[layer_id][blob_id]
          ->at(1);
        os << "wire " << interconnection << "_valid;" << endl;
        os << "wire " << interconnection << "_rdy;" << endl;
        os << "wire [" << top_depth*bit_width_-1 << ":0] " << interconnection
          << "_data;" << endl;
      }
    } else if (layer->layer_param().type() == "Concat") {
      // all the bottom blobs are required to append inception buffer
      for (int blob_id = 0; blob_id < layer->layer_param().bottom_size();
          ++blob_id) {
        const string interconnection = layer->layer_param().bottom(blob_id);
        const int bottom_depth = net_->bottom_blobs_shape_ptr_[layer_id]
          [blob_id]->at(1);
        os << "wire " << interconnection << "_inception_channel_buffer_valid;"
          << endl;
        os << "wire " << interconnection << "_inception_channel_buffer_rdy;"
          << endl;
        os << "wire [" << bottom_depth*bit_width_-1 << ":0]" << interconnection
          << "_inception_channel_buffer_data;" << endl;
      }
      const string interconnection = layer->layer_param().top(0);
      const int top_depth = net_->top_blobs_shape_ptr_[layer_id][0]->at(1);
      os << "wire " << interconnection << "_valid;" << endl;
      os << "wire " << interconnection << "_rdy;" << endl;
      os << "wire [" << top_depth*bit_width_-1 << ":0] " << interconnection
        << "_data;" << endl;
    }
  }

  os << endl << endl;
}

void VerilogCompiler::GenerateInstantiation(ostream& os) {
  os << "// ----------------------------------------------" << endl;
  os << "// Module instantiations" << endl;
  os << "// ----------------------------------------------" << endl;
  // iterates over all layers in the neural network
  for (size_t layer_id = 0; layer_id < net_->layers_.size(); ++layer_id) {
    const Layer* layer = net_->layers_[layer_id];
    if (layer->layer_param().type() == "Input") {
      GenerateInputLayer(os, layer_id);
    } else if (layer->layer_param().type() == "Convolution") {
      GenerateConvolutionLayer(os, layer_id);
      GenerateInterLayerChannelBuffer(os, layer_id, 0);
    } else if (layer->layer_param().type() == "Pooling") {
      GeneratePoolingLayer(os, layer_id);
      GenerateInterLayerChannelBuffer(os, layer_id, 0);
    } else if (layer->layer_param().type() == "InnerProduct") {
      GenerateInnerProductLayer(os, layer_id);
      GenerateInterLayerChannelBuffer(os, layer_id, 0);
    } else if (layer->layer_param().type() == "Split") {
      GenerateSplitLayer(os, layer_id);
    } else if (layer->layer_param().type() == "Concat") {
      for (int blob_id = 0; blob_id < layer->layer_param().bottom_size();
          ++blob_id) {
        GenerateInceptionChannelBuffer(os, layer_id, blob_id);
      }
      GenerateConcatLayer(os, layer_id);
    }
  }
  GenerateOutputLayer(os, net_->layers_.size()-1);
  os << endl << endl;
}

void VerilogCompiler::GenerateInputLayer(std::ostream& os, int layer_id)
  const {
  const Layer* layer = net_->layers_[layer_id];
  // for input layer, simply pass the primary input port to the interconnections
  os << "// Layer name: " << layer->layer_param().name() << "; Type: "
    << "Input" << endl;
  const string top_name = layer->layer_param().top(0);
  if (top_name != "input_layer") {
    os << "assign input_layer_rdy = " << top_name << "_rdy;" << endl;
    os << "assign " << top_name << "_valid = input_layer_valid;" << endl;
    os << "assign " << top_name << "_data = input_layer_data;" << endl;
  }
}

void VerilogCompiler::GenerateOutputLayer(std::ostream& os, int layer_id)
  const {
  const Layer* layer = net_->layers_[layer_id];
  // for output layer, make the connections to the primary output
  os << "// Connect the primary output port to Layer " << layer->layer_param().
    name() << endl;
  const string connection = layer->layer_param().top(0);
  const int connection_idx = interconnections_to_idx_.find(connection)->second;
  const string connection_name = interconnections_name_[connection_idx];
  if (connection_name != "output_layer") {
    os << "assign output_layer_valid = " << connection_name << "_valid;" << endl;
    os << "assign " << connection_name << "_rdy = output_layer_rdy;" << endl;
    os << "assign output_layer_data = " << connection_name << "_data;" << endl;
  }
}

void VerilogCompiler::GenerateConcatLayer(std::ostream& os, int layer_id)
  const {
  const Layer* layer = net_->layers_[layer_id];
  // instantiate module `concat_layer_pe` for Concat layer
  os << "// Layer name: " << layer->layer_param().name() << "; Type: "
    << "Concat" << endl;
  // parse the concat related parameters
  const int Nout = net_->top_blobs_shape_ptr_[layer_id][0]->at(1);
  const int numSplits = net_->bottom_blobs_shape_ptr_[layer_id].size();
  // find the connection names
  // previous connection usually contains several blobs
  vector<string> prev_name;
  for (int blob_id = 0; blob_id < layer->layer_param().bottom_size();
      ++blob_id) {
    const string prev_connection = layer->layer_param().bottom(blob_id);
    const int prev_connection_idx = interconnections_to_idx_.
      find(prev_connection)->second;
    prev_name.push_back(interconnections_name_[prev_connection_idx] +
        "_inception_channel_buffer");
  }
  string next_name = layer->layer_param().top(0);

  os << "concat #(" << endl;
  os << "\t.Nout\t\t\t\t" << "(" << Nout << ")," << endl;
  os << "\t.NUM_SPLIT\t\t" << "(" << numSplits << ")," << endl;
  os << "\t.BIT_WIDTH\t\t" << "(" << bit_width_ << ")" << endl;
  os << ") " << layer->layer_param().name() << " (" << endl;
  // previous ready (little endian)
  os << "\t.prev_layer_rdy\t\t" << "({";
  for (int blob_id = prev_name.size()-1; blob_id >= 0; --blob_id) {
    os << prev_name[blob_id] << "_rdy";
    if (blob_id != 0) {
      os << ", ";
    }
  }
  os << "})," << endl;
  // previous valid
  os << "\t.prev_layer_valid\t\t" << "({";
  for (int blob_id = prev_name.size()-1; blob_id >= 0; --blob_id) {
    os << prev_name[blob_id] << "_valid";
    if (blob_id != 0) {
      os << ", ";
    }
  }
  os << "})," << endl;
  // previoius data
  os << "\t.prev_layer_data\t\t" << "({";
  for (int blob_id = prev_name.size()-1; blob_id >= 0; --blob_id) {
    os << prev_name[blob_id] << "_data";
    if (blob_id != 0) {
      os << ", ";
    }
  }
  os << "})," << endl;

  // next layer
  os << "\t.next_layer_rdy\t\t" << "(" << next_name << "_rdy)," << endl;
  os << "\t.next_layer_valid\t\t" << "(" << next_name << "_valid)," << endl;
  os << "\t.next_layer_data\t\t" << "(" << next_name << "_data)" << endl;

  os << ");" << endl;
}

void VerilogCompiler::GenerateSplitLayer(std::ostream& os, int layer_id) const {
  const Layer* layer = net_->layers_[layer_id];
  // instantiate module `split_layer_pe` for Split layer
  os << "// Layer name: " << layer->layer_param().name() << "; Type: "
    << "Split" << endl;
  // parse the split related parameters
  const int Nin = net_->bottom_blobs_shape_ptr_[layer_id][0]->at(1);
  const int numSplits = net_->top_blobs_shape_ptr_[layer_id].size();
  const string prev_connection = layer->layer_param().bottom(0);
  const int prev_connection_idx = interconnections_to_idx_.find(prev_connection)
    ->second;
  const string prev_name = interconnections_name_[prev_connection_idx];
  // split layer usually has several connections
  vector<string> next_name;
  for (int blob_id = 0; blob_id < layer->layer_param().top_size(); ++blob_id) {
    next_name.push_back(layer->layer_param().top(blob_id));
  }

  os << "split #(" << endl;
  os << "\t.Nin\t\t\t\t" << "(" << Nin << ")," << endl;
  os << "\t.NUM_SPLIT\t\t" << "(" << numSplits << ")," << endl;
  os << "\t.BIT_WIDTH\t\t" << "(" << bit_width_ << ")" << endl;
  os << ") " << layer->layer_param().name() << " (" << endl;
  os << "\t.prev_layer_valid\t\t" << "(" << prev_name << "_valid)," << endl;
  os << "\t.prev_layer_rdy\t\t" << "(" << prev_name << "_rdy)," << endl;
  os << "\t.prev_layer_data\t\t" << "(" << prev_name << "_data)," << endl;
  os << "\t.next_layer_rdy\t\t" << "({";
  // use concatenation for split connection, follow the convention of little
  // endian
  // next layer ready
  for (int blob_id = next_name.size()-1; blob_id >= 0; --blob_id) {
    os << next_name[blob_id] << "_rdy";
    if (blob_id != 0) {
      os << ", ";
    }
  }
  os << "})," << endl;
  // next layer valid
  os << "\t.next_layer_valid\t\t" << "({";
  for (int blob_id = next_name.size()-1; blob_id >= 0; --blob_id) {
    os << next_name[blob_id] << "_valid";
    if (blob_id != 0) {
      os << ", ";
    }
  }
  os << "})," << endl;
  // next layer data
  os << "\t.next_layer_data\t\t" << "({";
  for (int blob_id = next_name.size()-1; blob_id >= 0; --blob_id) {
    os << next_name[blob_id] << "_data";
    if (blob_id != 0) {
      os << ", ";
    }
  }
  os << "})" << endl;
  os << ");" << endl;
}

void VerilogCompiler::GeneratePoolingLayer(std::ostream& os, int layer_id)
  const {
  const Layer* layer = net_->layers_[layer_id];
  // instantiate module `pool_layer_pe` for Pooling layer
  os << "// Layer name: " << layer->layer_param().name() << "; Type: "
    << "Pooling" << endl;
  // parse all the pooling related parameters
  const int Nin = dynamic_cast<const PoolingLayer*>(layer)->num_input_;
  const int Kh = dynamic_cast<const PoolingLayer*>(layer)->kh_;
  const int Kw = dynamic_cast<const PoolingLayer*>(layer)->kw_;
  const int h = dynamic_cast<const PoolingLayer*>(layer)->h_;
  const int w = dynamic_cast<const PoolingLayer*>(layer)->w_;
  const int Stride_h = dynamic_cast<const PoolingLayer*>(layer)->stride_h_;
  const int Stride_w = dynamic_cast<const PoolingLayer*>(layer)->stride_w_;
  const int Pad_h = dynamic_cast<const PoolingLayer*>(layer)->pad_h_;
  const int Pad_w = dynamic_cast<const PoolingLayer*>(layer)->pad_w_;
  string pool_method;
  if (dynamic_cast<const PoolingLayer*>(layer)->pool_method_ ==
      caffe::PoolingParameter_PoolMethod_MAX) {
    pool_method = "MAX";
  } else if (dynamic_cast<const PoolingLayer*>(layer)->pool_method_ ==
      caffe::PoolingParameter_PoolMethod_AVE) {
    pool_method = "AVG";
  } else {
    cerr << "undefined pooling method" << endl;
    exit(1);
  }
  const int Pin = parallelism_.find(layer_id)->second.first;
  // interconnections: find the name
  const string prev_connection = layer->layer_param().bottom(0);
  const int prev_connection_idx = interconnections_to_idx_.find(prev_connection)
    ->second;
  const string prev_name = interconnections_name_[prev_connection_idx];
  const string next_name = layer->layer_param().top(0) + "_inter_layer_buffer";

  os << "pool_layer_pe #(" << endl;
  os << "\t.Kh\t\t\t\t" << "(" << Kh << ")," << endl;
  os << "\t.Kw\t\t\t\t" << "(" << Kw << ")," << endl;
  os << "\t.h\t\t\t\t" << "(" << h << ")," << endl;
  os << "\t.w\t\t\t\t" << "(" << w << ")," << endl;
  os << "\t.Nin\t\t\t\t" << "(" << Nin << ")," << endl;
  os << "\t.pad_h\t\t" << "(" << Pad_h << ")," << endl;
  os << "\t.pad_w\t\t" << "(" << Pad_w << ")," << endl;
  os << "\t.stride_h\t\t" << "(" << Stride_h << ")," << endl;
  os << "\t.stride_w\t\t" << "(" << Stride_w << ")," << endl;
  os << "\t.Pin\t\t\t\t" << "(" << Pin << ")," << endl;
  os << "\t.BIT_WIDTH\t\t" << "(" << bit_width_ << ")," << endl;
  os << "\t.POOL_METHOD\t\t" << "(\"" << pool_method << "\")," << endl;
  os << "\t.POOL_PIPELINE\t\t" << "(" << pool_pipeline_ << ")" << endl;
  os << ") " << layer->layer_param().name() << " (" << endl;
  os << "\t.clk\t\t\t\t" << "(clk)," << endl;
  os << "\t.rst\t\t\t\t" << "(rst)," << endl;
  os << "\t.enable\t\t" << "(enable)," << endl;
  os << "\t.prev_layer_valid\t\t" << "(" << prev_name << "_valid)," << endl;
  os << "\t.prev_layer_rdy\t\t" << "(" << prev_name << "_rdy)," << endl;
  os << "\t.prev_layer_data\t\t" << "(" << prev_name << "_data)," << endl;
  os << "\t.next_layer_valid\t\t" << "(" << next_name << "_valid)," << endl;
  os << "\t.next_layer_rdy\t\t" << "(" << next_name << "_rdy)," << endl;
  os << "\t.next_layer_data\t\t" << "(" << next_name << "_data)" << endl;
  os << ");" << endl;
}

void VerilogCompiler::GenerateInnerProductLayer(std::ostream& os, int layer_id)
  const {
  const Layer* layer = net_->layers_[layer_id];
  // instantiate module `conv_layer_pe` for InnerProduct layer
  os << "// Layer name: " << layer->layer_param().name() << "; Type: "
    << "InnerProduct" << endl;
  // parse all the inner-product related parameters
  // Innerproduct layer has a complicated situation, it may accept 4D feature
  // map and 2D feature map
  int Nin, Kh, Kw, h, w;
  if (net_->bottom_blobs_shape_ptr_[layer_id][0]->size() == 4) {
    // 4D feature map
    Nin = net_->bottom_blobs_shape_ptr_[layer_id][0]->at(1);
    h = net_->bottom_blobs_shape_ptr_[layer_id][0]->at(2);
    w = net_->bottom_blobs_shape_ptr_[layer_id][0]->at(3);
    // Kh = h & Kw = w
    Kh = h;
    Kw = w;
  } else if (net_->bottom_blobs_shape_ptr_[layer_id][0]->size() == 2) {
    // 2D feature map
    Nin = net_->bottom_blobs_shape_ptr_[layer_id][0]->at(1);
    h = w = Kh = Kw = 1;
  } else {
    // unexpected feature map dimension
    cerr << "unexpected bottom blob shape of InnerProductLayer" << endl;
    exit(1);
  }
  const int Nout = dynamic_cast<const InnerProductLayer*>(layer)->num_output_;
  const int Stride_h = 1;
  const int Stride_w = 1;
  const int Pad_h = 0;
  const int Pad_w = 0;
  const int Pin = parallelism_.find(layer_id)->second.first;
  const int Pout = parallelism_.find(layer_id)->second.second;
  // interconnections: find the name
  const string prev_connection = layer->layer_param().bottom(0);
  const int prev_connection_idx = interconnections_to_idx_.find(prev_connection)
    ->second;
  const string prev_name = interconnections_name_[prev_connection_idx];
  const string next_name = layer->layer_param().top(0) + "_inter_layer_buffer";

  os << "conv_layer_pe #(" << endl;
  os << "\t.Kh\t\t\t\t" << "(" << Kh << ")," << endl;
  os << "\t.Kw\t\t\t\t" << "(" << Kw << ")," << endl;
  os << "\t.h\t\t\t\t" << "(" << h << ")," << endl;
  os << "\t.w\t\t\t\t" << "(" << w << ")," << endl;
  os << "\t.Nin\t\t\t\t" << "(" << Nin << ")," << endl;
  os << "\t.Nout\t\t\t\t" << "(" << Nout << ")," << endl;
  os << "\t.pad_h\t\t\t\t" << "(" << Pad_h << ")," << endl;
  os << "\t.pad_w\t\t\t\t" << "(" << Pad_w << ")," << endl;
  os << "\t.stride_h\t\t\t\t" << "(" << Stride_h << ")," << endl;
  os << "\t.stride_w\t\t\t\t" << "(" << Stride_w << ")," << endl;
  os << "\t.Pin\t\t\t\t" << "(" << Pin << ")," << endl;
  os << "\t.Pout\t\t\t\t" << "(" << Pout << ")," << endl;
  os << "\t.BIT_WIDTH\t\t" << "(" << bit_width_ << ")," << endl;
  os << "\t.MULT_PIPELINE\t\t" << "(" << mult_pipeline_ << ")," << endl;
  os << "\t.ADD_PIPELINE\t\t" << "(" << add_pipeline_ << ")," << endl;
  os << "\t.NONLIN_PIPELINE\t\t" << "(" << nonlin_pipeline_ << ")," << endl;
  // TODO: always enable bias & nonlinear
  os << "\t.BIAS_EN\t\t" << "(1)," << endl;
  os << "\t.NONLIN_EN\t\t" << "(1)" << endl;
  os << ") " << layer->layer_param().name() << " (" << endl;
  os << "\t.clk\t\t\t\t" << "(clk)," << endl;
  os << "\t.rst\t\t\t\t" << "(rst)," << endl;
  os << "\t.enable\t\t" << "(enable)," << endl;
  os << "\t.prev_layer_valid\t\t" << "(" << prev_name << "_valid)," << endl;
  os << "\t.prev_layer_rdy\t\t" << "(" << prev_name << "_rdy)," << endl;
  os << "\t.prev_layer_data\t\t" << "(" << prev_name << "_data)," << endl;
  os << "\t.next_layer_valid\t\t" << "(" << next_name << "_valid)," << endl;
  os << "\t.next_layer_rdy\t\t" << "(" << next_name << "_rdy)," << endl;
  os << "\t.next_layer_data\t\t" << "(" << next_name << "_data)" << endl;
  os << ");" << endl;

}

void VerilogCompiler::GenerateConvolutionLayer(std::ostream& os, int layer_id)
  const {
  const Layer* layer = net_->layers_[layer_id];
  // instantiate module `conv_layer_pe` for Convolutional layer
  os << "// Layer name: " << layer->layer_param().name() << "; Type: "
    << "Convolution" << endl;
  // parse all the convolutional related parameters
  const int Nin = dynamic_cast<const ConvolutionLayer*>(layer)->num_input_;
  const int Nout = dynamic_cast<const ConvolutionLayer*>(layer)->num_output_;
  const int Kh = dynamic_cast<const ConvolutionLayer*>(layer)->kh_;
  const int Kw = dynamic_cast<const ConvolutionLayer*>(layer)->kw_;
  const int h = dynamic_cast<const ConvolutionLayer*>(layer)->h_;
  const int w = dynamic_cast<const ConvolutionLayer*>(layer)->w_;
  const int Stride_h = dynamic_cast<const ConvolutionLayer*>(layer)->stride_h_;
  const int Stride_w = dynamic_cast<const ConvolutionLayer*>(layer)->stride_w_;
  const int Pad_h = dynamic_cast<const ConvolutionLayer*>(layer)->pad_h_;
  const int Pad_w = dynamic_cast<const ConvolutionLayer*>(layer)->pad_w_;
  const int Pin = parallelism_.find(layer_id)->second.first;
  const int Pout = parallelism_.find(layer_id)->second.second;
  // interconnections: find the name
  const string prev_connection = layer->layer_param().bottom(0);
  const int prev_connection_idx = interconnections_to_idx_.find(prev_connection)
    ->second;
  const string prev_name = interconnections_name_[prev_connection_idx];
  const string next_name = layer->layer_param().top(0) + "_inter_layer_buffer";

  os << "conv_layer_pe #(" << endl;
  os << "\t.Kh\t\t\t\t" << "(" << Kh << ")," << endl;
  os << "\t.Kw\t\t\t\t" << "(" << Kw << ")," << endl;
  os << "\t.h\t\t\t\t" << "(" << h << ")," << endl;
  os << "\t.w\t\t\t\t" << "(" << w << ")," << endl;
  os << "\t.Nin\t\t\t\t" << "(" << Nin << ")," << endl;
  os << "\t.Nout\t\t\t\t" << "(" << Nout << ")," << endl;
  os << "\t.pad_h\t\t\t\t" << "(" << Pad_h << ")," << endl;
  os << "\t.pad_w\t\t\t\t" << "(" << Pad_w << ")," << endl;
  os << "\t.stride_h\t\t\t\t" << "(" << Stride_h << ")," << endl;
  os << "\t.stride_w\t\t\t\t" << "(" << Stride_w << ")," << endl;
  os << "\t.Pin\t\t\t\t" << "(" << Pin << ")," << endl;
  os << "\t.Pout\t\t\t\t" << "(" << Pout << ")," << endl;
  os << "\t.BIT_WIDTH\t\t" << "(" << bit_width_ << ")," << endl;
  os << "\t.MULT_PIPELINE\t\t" << "(" << mult_pipeline_ << ")," << endl;
  os << "\t.ADD_PIPELINE\t\t" << "(" << add_pipeline_ << ")," << endl;
  os << "\t.NONLIN_PIPELINE\t\t" << "(" << nonlin_pipeline_ << ")," << endl;
  // TODO: always enable bias & nonlinear
  os << "\t.BIAS_EN\t\t" << "(1)," << endl;
  os << "\t.NONLIN_EN\t\t" << "(1)" << endl;
  os << ") " << layer->layer_param().name() << " (" << endl;
  os << "\t.clk\t\t\t\t" << "(clk)," << endl;
  os << "\t.rst\t\t\t\t" << "(rst)," << endl;
  os << "\t.enable\t\t" << "(enable)," << endl;
  os << "\t.prev_layer_valid\t\t" << "(" << prev_name << "_valid)," << endl;
  os << "\t.prev_layer_rdy\t\t" << "(" << prev_name << "_rdy)," << endl;
  os << "\t.prev_layer_data\t\t" << "(" << prev_name << "_data)," << endl;
  os << "\t.next_layer_valid\t\t" << "(" << next_name << "_valid)," << endl;
  os << "\t.next_layer_rdy\t\t" << "(" << next_name << "_rdy)," << endl;
  os << "\t.next_layer_data\t\t" << "(" << next_name << "_data)" << endl;
  os << ");" << endl;
}

void VerilogCompiler::GenerateInterLayerChannelBuffer(std::ostream& os,
    int layer_id, int blob_id) {
  const Layer* layer = net_->layers_[layer_id];
  os << "// Inter-layer channel buffer of Layer: " << layer->layer_param().
    name() << endl;
  const int num_channel = net_->top_blobs_shape_ptr_[layer_id][blob_id]->at(1);
  // interconnections: find the name
  const string prev_name = layer->layer_param().top(blob_id) +
    "_inter_layer_buffer";
  const string next_name = layer->layer_param().top(blob_id);
  // channel buffer module name
  const string module_name = layer->layer_param().name() +
    "_inter_channel_buffer";

  os << "channel_buffer #(" << endl;
  os << "\t.BIT_WIDTH\t\t" << "(" << bit_width_ << ")," << endl;
  os << "\t.BUFFER_DEPTH\t\t" << "(" << inter_layer_buffer_depth_ << "),"
    << endl;
  os << "\t.NUM_CHANNEL\t\t" << "(" << num_channel << ")" << endl;
  os << ") " << module_name << " (" << endl;
  os << "\t.clk\t\t\t\t" << "(clk)," << endl;
  os << "\t.rst\t\t\t\t" << "(rst)," << endl;
  os << "\t.prev_layer_valid\t\t" << "(" << prev_name << "_valid" << "),"
    << endl;
  os << "\t.prev_layer_rdy\t\t" << "(" << prev_name << "_rdy" << "),"
    << endl;
  os << "\t.prev_layer_data\t\t" << "(" << prev_name << "_data)," << endl;
  os << "\t.next_layer_valid\t\t" << "(" << next_name << "_valid)," << endl;
  os << "\t.next_layer_rdy\t\t" << "(" << next_name << "_rdy)," << endl;
  os << "\t.next_layer_data\t\t" << "(" << next_name << "_data)" << endl;
  os << ");" << endl;

  // record the channel buffer name
  if (inter_layer_buffer_depth_ != 0) {
    channel_buffer_module_.push_back(module_name);
  }
}

void VerilogCompiler::GenerateInceptionChannelBuffer(std::ostream& os,
    int layer_id, int blob_id) {
  const Layer* layer = net_->layers_[layer_id];
  os << "// Inception channel buffer of Concat Layer: "
    << layer->layer_param().name() << endl;
  const int num_channel = net_->bottom_blobs_shape_ptr_[layer_id][blob_id]->
    at(1);
  const string prev_name = layer->layer_param().bottom(blob_id);
  const string next_name = layer->layer_param().bottom(blob_id) +
    "_inception_channel_buffer";
  stringstream module_name_stream;
  module_name_stream << layer->layer_param().name() <<
    "_inception_channel_buffer_" << blob_id;
  const string module_name = module_name_stream.str();

  os << "channel_buffer #(" << endl;
  os << "\t.BIT_WIDTH\t\t" << "(" << bit_width_ << ")," << endl;
  os << "\t.BUFFER_DEPTH\t\t" << "(" << inception_buffer_depth_ << ")," << endl;
  os << "\t.NUM_CHANNEL\t\t" << "(" << num_channel << ")" << endl;
  os << ") " << module_name << " (" << endl;
  os << "\t.clk\t\t\t\t" << "(clk)," << endl;
  os << "\t.rst\t\t\t\t" << "(rst)," << endl;
  os << "\t.prev_layer_valid\t\t" << "(" << prev_name << "_valid)," << endl;
  os << "\t.prev_layer_rdy\t\t" << "(" << prev_name << "_rdy)," << endl;
  os << "\t.prev_layer_data\t\t" << "(" << prev_name << "_data)," << endl;
  os << "\t.next_layer_rdy\t\t" << "(" << next_name << "_rdy)," << endl;
  os << "\t.next_layer_valid\t\t" << "(" << next_name << "_valid)," << endl;
  os << "\t.next_layer_data\t\t" << "(" << next_name << "_data)" << endl;
  os << ");" << endl;

  // record the channel buffer name
  if (inception_buffer_depth_ != 0) {
    channel_buffer_module_.push_back(module_name);
  }
}
