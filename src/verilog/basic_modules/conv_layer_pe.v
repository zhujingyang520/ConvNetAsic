// -----------------------------------------------------------------------------
// This file exports the module `conv_layer_pe, which is the top module of the
// convolutional processing element (pe). It includes the FSM controller to
// coordinate each component within the datapath to operate well.
// -----------------------------------------------------------------------------

module conv_layer_pe #(
  parameter                       Kh = 3,           // convolutional kernel dimension
  parameter                       Kw = 30,
  parameter                       h = 5,            // input feature map spatial dim
  parameter                       w = 50,
  parameter                       Nin = 3,          // input feature map no.
  parameter                       Nout = 3,         // output feature map no.
  parameter                       pad_h = 1,        // feature map padding spatial dim
  parameter                       pad_w = 1,
  parameter                       stride_h = 2,     // stride spatial dim
  parameter                       stride_w = 1,
  parameter                       Pin = 2,          // input feature map parallelism
  parameter                       Pout = 1,         // output feature map parallelism
  parameter                       BIT_WIDTH=8,      // bit width of the data path
  parameter                       MULT_PIPELINE = 2,// multiplier array pipeline stage
  parameter                       ADD_PIPELINE = 2, // adder array pipeline stage
  // ---------------------------------------------------------------------------
  // Nonlinear pipeline stage: it may have the data dependency issue under the
  // extreme case: output partial sum takes very few clock cycle to finish, and
  // the the partial sum result has not been written to the register file. But in
  // the real case, it will never occur because Pout will be much smaller than
  // the output feature map size
  // In the architecture, we should make sure
  // ** ceil(Nout/Pout) >= 1 + NONLIN_PIPELINE
  // Then the dependency issue will not occur
  // ---------------------------------------------------------------------------
  parameter                       NONLIN_PIPELINE = 1,
  // switch for turn on the bias enable and nonlinear enable
  parameter                       BIAS_EN = 1,
  parameter                       NONLIN_EN = 1
) (
  input wire                      clk,              // system clock
  input wire                      rst,              // system reset (active high)

  // controller to enable the computation
  input wire                      enable,           // enable (active high)

  // input data path with handshake
  input wire                      prev_layer_valid, // previous layer data valid
  output wire                     prev_layer_rdy,   // previous layer data ready
  input wire  [Nin*BIT_WIDTH-1:0] prev_layer_data,  // previous layer data
  // output data path with handshake
  input wire                      next_layer_rdy,   // next layer data ready
  output wire                     next_layer_valid, // next layer data valid
  output wire [Nout*BIT_WIDTH-1:0]next_layer_data   // next layer data

`ifdef DEBUG
, // debug io interface
  output wire [Nin*Kh*Kw*BIT_WIDTH-1:0]       line_buffer_output_data,
  output wire [Pin*Kh*Kw*BIT_WIDTH-1:0]       mux_array_output_data,
  output wire [Pout*Pin*Kh*Kw*BIT_WIDTH-1:0]  kernel_mem_output_data,
  output wire [Pout*Pin*Kh*Kw*BIT_WIDTH-1:0]  mult_array_output_data,
  output wire [Pout*BIT_WIDTH-1:0]            add_array_output_data,
  output wire [Pout*BIT_WIDTH-1:0]            nonlinear_output_data
`endif
);

`include "functions.v"

// -----------------------
// Inferred parameters
// -----------------------
localparam    PADDED_H = h + 2*pad_h;               // padded feature map dim
localparam    PADDED_W = w + 2*pad_w;
// tiled input & output parallelism
localparam    TILED_IN_PARALLEL = ceil_div(Nin, Pin);
localparam    TILED_OUT_PARALLEL = ceil_div(Nout, Pout);
// weight memory depth = ceil(Nin/Pin)*ceil(Nout/Pout)
localparam    WEIGHT_MEM_DEPTH = TILED_IN_PARALLEL*TILED_OUT_PARALLEL;
localparam    WEIGHT_ADDR_WIDTH = clog2(WEIGHT_MEM_DEPTH);

// iterator for generation
genvar i, j, k, l;

// -----------------------------
// Interconnections declaration
// -----------------------------
// ----------------------------
// line buffer
// ----------------------------
wire line_buffer_valid; // line buffer valid
wire line_buffer_zero;  // line buffer zero input
// line buffer sliding window data (Nin input feature maps)
wire [Nin*Kh*Kw*BIT_WIDTH-1:0] line_buffer_data;
// --------------------------
// line buffer mux
// --------------------------
wire mux_enable;
wire [clog2(ceil_div(Nin, Pin))-1:0] mux_select;
wire [Pin*Kh*Kw*BIT_WIDTH-1:0] mux_array_data;
// ------------------------
// weight memory
// ------------------------
wire [Pin*Kh*Kw-1:0] kernel_mem_en;
wire [WEIGHT_ADDR_WIDTH-1:0] kernel_mem_addr;
wire [Pout*Pin*Kh*Kw*BIT_WIDTH-1:0] kernel_mem_data;
// -----------------------
// Multiplier array
// -----------------------
wire mult_array_en;
wire mult_array_valid;
wire [Pout*Pin*Kh*Kw*BIT_WIDTH-1:0] mult_array_data;
// -----------------------
// Adder array
// -----------------------
wire add_array_valid;
wire [Pout*BIT_WIDTH-1:0] add_array_data;

// -----------------------
// Nonlinear block
// -----------------------
wire accumulate_en;
wire bias_en;
wire nonlin_en;
wire nonlinear_valid;
wire [Pout*BIT_WIDTH-1:0] nonlinear_data;
wire bias_mem_en;
wire [clog2(TILED_OUT_PARALLEL)-1:0] bias_mem_addr;

// -----------------------
// Output register file
// -----------------------
wire out_regfile_re;
wire [clog2(ceil_div(Nout, Pout))-1:0] out_regfile_raddr, out_regfile_waddr;
wire [Pout*BIT_WIDTH-1:0] out_regfile_rdata;

// --------------------------
// Main blocks instantiation
// --------------------------
// FSM controller to coordinate the data path
conv_layer_ctrl #(
  .Kh                 (Kh),                 // convolutional kernel dimension
  .Kw                 (Kw),
  .h                  (h),                  // input feature map spatial dim
  .w                  (w),
  .Nin                (Nin),                // input feature map no.
  .Nout               (Nout),               // output feature map no.
  .pad_h              (pad_h),              // feature map padding spatial dim
  .pad_w              (pad_w),
  .stride_h           (stride_h),           // stride spatial dim
  .stride_w           (stride_w),
  .Pin                (Pin),                // input feature map parallelism
  .Pout               (Pout),               // output feature map parallelism
  .BIT_WIDTH          (BIT_WIDTH),          // bit width of data
  .MULT_PIPELINE      (MULT_PIPELINE),      // multiplier array pipeline stage
  .ADD_PIPELINE       (ADD_PIPELINE),       // adder array pipeline stage
  .NONLIN_PIPELINE    (NONLIN_PIPELINE),    // nonlinear pipeline stage
  .BIAS_EN            (BIAS_EN),            // switch for enable bias & nonlinear op
  .NONLIN_EN          (NONLIN_EN)
) conv_layer_ctrl_inst
(
  .clk                (clk),                // system clock
  .rst                (rst),                // system reset (active high)

  // global control to enable the computation
  .enable             (enable),             // enable computation

  // handshake of previous layer & next layer
  .prev_layer_valid   (prev_layer_valid),   // previous layer valid
  .prev_layer_rdy     (prev_layer_rdy),     // previous layer ready
  .next_layer_rdy     (next_layer_rdy),     // next layer ready
  .next_layer_valid   (next_layer_valid),   // next layer valid

  // data path: line buffer
  .line_buffer_valid  (line_buffer_valid),  // line buffer valid
  .line_buffer_zero   (line_buffer_zero),   // line buffer zero flag

  // data path: line buffer mux array
  .mux_enable         (mux_enable),         // line buffer mux enable
  .mux_select         (mux_select),         // line buffer mux select

  // data path: weight memory access
  .mux_array_data     (mux_array_data),     // line buffer mux data (mem gating)
  .kernel_mem_en      (kernel_mem_en),      // kernel memory access enable (active low)
  .kernel_mem_addr    (kernel_mem_addr),    // kernel memory access address

  // data path: multiplier array
  .mult_array_en      (mult_array_en),      // multiplier array enable

  // data path: nonlinear module: includes accumulation, bias, and
  // nonlinear opeartion
  .accumulate_en      (accumulate_en),      // accumulate enable
  .bias_en            (bias_en),            // bias enable
  .nonlin_en          (nonlin_en),          // nonlinear enable
  // bias memory access within the nonlinear module
  .bias_mem_en        (bias_mem_en),        // bias memory enable (active low)
  .bias_mem_addr      (bias_mem_addr),      // bias memory access address

  // data path: output register file read path
  .out_regfile_re     (out_regfile_re),     // output register file read enable
  .out_regfile_raddr  (out_regfile_raddr),  // output register file read address
  // data path: output register file write path
  // write enable is provided by the nonlinear unit
  .out_regfile_waddr  (out_regfile_waddr)   // output register file write address
);

// -----------------------------------------------------------------------------
// Line buffer array: includes Nin line buffers. It is possible to share all Nin
// line buffers as a single SRAM to save the hardware resources (TODO)
// -----------------------------------------------------------------------------
line_buffer_array #(
  .Kh                 (Kh),                 // convolutional kernel dimension
  .Kw                 (Kw),
  .h                  (h),                  // input feature map spatial dimension
  .w                  (w),
  .Nin                (Nin),                // input feature map no.
  .pad_h              (pad_h),              // padding dimension
  .pad_w              (pad_w),
  .BIT_WIDTH          (BIT_WIDTH)           // bit width of the data path
) line_buffer_array_inst
(
  .clk                (clk),                // system clock
  .rst                (rst),                // system reset

  // control signal
  .line_buffer_valid  (line_buffer_valid),  // line buffer valid
  .line_buffer_zero   (line_buffer_zero),   // line buffer zero flag
  // data path
  .prev_layer_data    (prev_layer_data),    // previous layer data
  .line_buffer_data   (line_buffer_data)    // line buffer sliding window data
);

// ---------------------------------------------------------------------------
// Line buffer mux: the mux array is a seperate pipeline stage. It select the
// desired Pin data from the Nin input feature maps
// ---------------------------------------------------------------------------
mux_array #(
  .Nin                (Nin),                // input feature map number
  .Pin                (Pin),                // input feature map parallelism
  .BIT_WIDTH          (Kh*Kw*BIT_WIDTH)     // bit width
) mux_array_inst (
  .clk                (clk),                // system clock
  .mux_enable         (mux_enable),         // enable (active high)
  .mux_select         (mux_select),         // mux select signal
  .line_buffer_data   (line_buffer_data),   // line buffer sliding window data
  .mux_array_out_data (mux_array_data)      // mux array output data
);

// --------------------------------------------------------------------------
// TODO
// Weight memory instantiation (only valid for a specific parameter)
// Width: 8 (BIT WIDTH x Pout)
// Depth: 1024 (conservative for a large number of kernel size)
// Should be greater than ceil(Nout/Pout)*ceil(Nin/Pin)
// -------------------------------------------------------------------------
kernel_mem #(
  .Kh                 (Kh),                  // convolutional kernel dimension
  .Kw                 (Kw),
  .Nin                (Nin),                 // input feature map no.
  .Nout               (Nout),                // output feature map no.
  .Pin                (Pin),                 // input feature map parallelism
  .Pout               (Pout),                // output feature map parallelism
  .BIT_WIDTH          (BIT_WIDTH)            // bit width of the data path
) kernel_mem_inst (
  .clk                (clk),                 // system clock
  .mem_en             (kernel_mem_en),       // memory enable (active low)
  .mem_addr           (kernel_mem_addr),     // memory address

  .mem_data           (kernel_mem_data)      // memory data
);

// -----------------------------------
// Multiplier array
// -----------------------------------
mult_array #(
  .Kh                     (Kh),             // convolutional kernel dimension
  .Kw                     (Kw),
  .Pin                    (Pin),            // input feature map parallelism
  .Pout                   (Pout),           // output feature map parallelism
  .BIT_WIDTH              (BIT_WIDTH),      // bit width of the data path
  // advanced setting: pipeline stage (retiming for a better performance)
  // Note: should be at least 1 pipeline stage
  .PIPELINE_STAGE         (MULT_PIPELINE)
) mult_array_inst (
  .clk                    (clk),            // system clock
  .rst                    (rst),            // system reset
  .mult_array_en          (mult_array_en),  // multiplier array enable (active high)
  .feat_map_in            (mux_array_data), // feature map input (from line buffer)
  .kernel_in              (kernel_mem_data),// kernel input (from weight mem)

  .mult_array_valid       (mult_array_valid), // multiplier array valid
  .mult_array_data        (mult_array_data) // multiplier array data output
);

// ----------------------------------
// Adder array
// ----------------------------------
add_array #(
  .Kh                     (Kh),             // convolutional kernel dimension
  .Kw                     (Kw),
  .Pin                    (Pin),            // input feature map parallelism
  .Pout                   (Pout),           // output feature map parallelism
  .BIT_WIDTH              (BIT_WIDTH),      // bit width of the data path
  // advanced setting: pipeline stage (retiming for a better performance)
  // Note: should be at least 1 pipeline stage
  .PIPELINE_STAGE         (ADD_PIPELINE)
) add_array_inst (
  .clk                    (clk),            // system clock
  .rst                    (rst),            // system reset
  .add_array_en           (mult_array_valid),   // adder array enable (active high)
  .mult_array_data        (mult_array_data),// data from the multiplier array

  .add_array_valid        (add_array_valid),// add array valid
  .add_array_data         (add_array_data)  // add array output data
);

// ----------------------------------
// Nonlinear unit
// ----------------------------------
nonlinear #(
  .Nout                   (Nout),           // output feature map no.
  .Pout                   (Pout),           // output feature map parallelism
  .BIT_WIDTH              (BIT_WIDTH),      // bit width of the data path
  // advanced setting: pipeline stage (retiming for a better performance)
  // Note: should be at least 1 pipeline stage
  .PIPELINE_STAGE         (NONLIN_PIPELINE),
  // bias & nonlinear module enable
  .BIAS_EN                (BIAS_EN),
  .NONLIN_EN              (NONLIN_EN)
) nonlinear_inst (
  .clk                    (clk),            // system clock
  .rst                    (rst),            // system reset
  // data path control signal
  .add_array_valid        (add_array_valid),// add array valid
  .accumulate_en          (accumulate_en),  // accumulate enable
  .bias_en                (bias_en),        // bias enable
  .nonlin_en              (nonlin_en),      // nonlinear enable
  // bias memory access control signal
  .bias_mem_en            (bias_mem_en),    // memory enable (active low)
  .bias_mem_addr          (bias_mem_addr),  // memory address

  // input data path
  .add_array_data         (add_array_data), // merge results from the adder array
  .out_regfile_data       (out_regfile_rdata),  // output register file read data

  // output data path
  .nonlinear_valid        (nonlinear_valid),// nonlinear valid
  .nonlinear_data         (nonlinear_data)  // nonlinear data
);

// ----------------------------------
// Output register file
// ----------------------------------
out_regfile #(
  .Nout                   (Nout),           // output feature map number
  .Pout                   (Pout),           // output feature map parallelism
  .BIT_WIDTH              (BIT_WIDTH)       // bit width of each element
) out_regfile_inst
(
  .clk                    (clk),            // system clock

  // write path
  .write_en               (nonlinear_valid),// write enable (active high)
  .write_addr             (out_regfile_waddr),  // write address
  .write_data             (nonlinear_data), // write data
  // read path
  .read_en                (out_regfile_re), // read enable (active high)
  .read_addr              (out_regfile_raddr),  // read address
  .read_data              (out_regfile_rdata),  // read data (no. = Pout)

  // register file memory: for output feature map
  .regfile                (next_layer_data) // all the contents of register file
);


// ------------------------
// Output signal for debug
// ------------------------
`ifdef DEBUG
  assign line_buffer_output_data = line_buffer_data;
  assign mux_array_output_data = mux_array_data;
  assign kernel_mem_output_data = kernel_mem_data;
  assign mult_array_output_data = mult_array_data;
  assign add_array_output_data = add_array_data;
  assign nonlinear_output_data = nonlinear_data;
`endif

endmodule

