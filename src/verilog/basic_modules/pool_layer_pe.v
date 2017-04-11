// -----------------------------------------------------------------------------
// This file exports the module `pool_layer_pe`, which is the top module of the
// pooling processing element (pe). It includes the FSM controller to coordinate
// each arithmetic unit together.
// -----------------------------------------------------------------------------

module pool_layer_pe #(
  parameter                       Kh = 3,           // pooling kernel dimension
  parameter                       Kw = 3,
  parameter                       h = 5,            // input feature map spatial dim
  parameter                       w = 5,
  parameter                       Nin = 3,          // input feature map no.
  parameter                       pad_h = 1,        // feature map padding spatial dim
  parameter                       pad_w = 1,
  parameter                       stride_h = 1,     // stride spatial dim
  parameter                       stride_w = 1,
  parameter                       Pin = 1,          // input feature map parallelism
  parameter                       BIT_WIDTH = 8,    // bit width of data
  // pooling method: {MAX, AVG}
  parameter                       POOL_METHOD = "AVG",
  parameter                       POOL_PIPELINE = 2 // pooling array pipeline stage
) (
  input wire                      clk,              // system clock
  input wire                      rst,              // system reset

  // controller enable
  input wire                      enable,           // enable (active high)

  // input data path with handshake
  input wire                      prev_layer_valid, // previous layer valid
  output wire                     prev_layer_rdy,   // previous layer ready
  input wire  [Nin*BIT_WIDTH-1:0] prev_layer_data,  // previous layer data
  // output data path with handshake
  input wire                      next_layer_rdy,   // next layer ready
  output wire                     next_layer_valid, // next layer data valid
  output wire [Nin*BIT_WIDTH-1:0] next_layer_data   // next layer data

`ifdef DEBUG
, // debug io interface
  output wire [Nin*Kh*Kw*BIT_WIDTH-1:0]       line_buffer_output_data,
  output wire [Pin*Kh*Kw*BIT_WIDTH-1:0]       mux_array_output_data,
  output wire [Pin*BIT_WIDTH-1:0]             pool_array_output_data
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
// -------------------------------
// pool array unit
// -------------------------------
wire pool_array_en;
wire [Pin*BIT_WIDTH-1:0] pool_array_data;
wire pool_array_valid;
// -------------------------------
// output register file
// -------------------------------
wire [clog2(ceil_div(Nin, Pin))-1:0] out_regfile_waddr;

// -------------------------------
// Main block instantiation
// -------------------------------
// FSM controller to coordinate the data path
pool_layer_ctrl #(
  .Kh                 (Kh),                 // pooling kernel dimension
  .Kw                 (Kw),
  .h                  (h),                  // input feature map spatial dim
  .w                  (w),
  .Nin                (Nin),                // input feature map no.
  .pad_h              (pad_h),              // feature map padding spatial dim
  .pad_w              (pad_w),
  .stride_h           (stride_h),           // stride spatial dim
  .stride_w           (stride_w),
  .Pin                (Pin),                // input feature map parallelism
  .BIT_WIDTH          (BIT_WIDTH),          // bit width of data
  .POOL_PIPELINE      (POOL_PIPELINE)       // pooling array pipeline stage
) pool_layer_ctrl_inst
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

  // -----------------------
  // data path contol logic
  // -----------------------
  // data path: line buffer
  .line_buffer_valid  (line_buffer_valid),  // line buffer valid
  .line_buffer_zero   (line_buffer_zero),   // line buffer zero flag

  // data path: line buffer mux array
  .mux_enable         (mux_enable),         // line buffer mux enable
  .mux_select         (mux_select),         // line buffer mux select

  // data path: pooling unit
  .pool_array_en      (pool_array_en),      // pool array enable

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

// -----------------------------------------------------------------------------
// Pooling array unit: do the max pooling or avg pooling of the provided sliding
// window.
// -----------------------------------------------------------------------------
pool_array #(
  .Kh                 (Kh),                 // pooling kernel dimension
  .Kw                 (Kw),
  .Pin                (Pin),                // input feature map parallelism
  .BIT_WIDTH          (BIT_WIDTH),          // bit width of the data path
  // pooling method: support for {MAX, AVG}
  .POOL_METHOD        (POOL_METHOD),
  // advanced setting: pipeline stage (retiming for a better performance)
  // Note: should be at least 1 pipeline stage
  .PIPELINE_STAGE     (POOL_PIPELINE)
) pool_array_inst (
  .clk                (clk),                // system clock
  .rst                (rst),                // system reset
  .pool_array_en      (pool_array_en),      // pool array enable
  .pool_data          (mux_array_data),     // pool data (from line buffer)

  .pool_array_valid   (pool_array_valid),   // pool array valid signal
  .pool_array_data    (pool_array_data)
);

// -----------------------------------------------------------------------
// Output register file: pooling unit does NOT require the read enable
// -----------------------------------------------------------------------
out_regfile #(
  .Nout                   (Nin),            // output feature map number
  .Pout                   (Pin),            // output feature map parallelism
  .BIT_WIDTH              (BIT_WIDTH)       // bit width of each element
) out_regfile_inst
(
  .clk                    (clk),            // system clock

  // write path
  .write_en               (pool_array_valid),   // write enable (active high)
  .write_addr             (out_regfile_waddr),  // write address
  .write_data             (pool_array_data),    // write data
  // read path (disable for pooling unit)
  .read_en                (1'b0),           // read enable (active high)
  .read_addr              ({clog2(ceil_div(Nin, Pin)){1'b0}}),  // read address
  .read_data              (/* floating */),     // read data (no. = Pout)

  // register file memory: for output feature map
  .regfile                (next_layer_data) // all the contents of register file
);

// --------------------------------
// Output signals for debug
// --------------------------------
`ifdef DEBUG
  assign line_buffer_output_data = line_buffer_data;
  assign mux_array_output_data = mux_array_data;
  assign pool_array_output_data = pool_array_data;
`endif

endmodule
