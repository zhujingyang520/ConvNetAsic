// -----------------------------------------------------------------------------
// This file exports the module `line_buffer_array`, which is the basic module
// to export Nin line buffers.
// -----------------------------------------------------------------------------

module line_buffer_array #(
  parameter                       Kh = 3,           // convolutional kernel dimension
  parameter                       Kw = 3,
  parameter                       h = 5,            // input feature map spatial dimension
  parameter                       w = 5,
  parameter                       Nin = 3,          // input feature map no.
  parameter                       pad_h = 1,        // padding dimension
  parameter                       pad_w = 1,
  parameter                       BIT_WIDTH = 8     // bit width of the data path
)
(
  input wire                      clk,              // system clock
  input wire                      rst,              // system reset

  // control signal
  input wire                      line_buffer_valid,// line buffer valid
  input wire                      line_buffer_zero, // line buffer zero flag
  // data path
  input wire  [Nin*BIT_WIDTH-1:0] prev_layer_data,  // previous layer data
  output wire [Nin*Kh*Kw*BIT_WIDTH-1:0]
                                  line_buffer_data  // line buffer sliding window data
);

// ----------------------------------------------------------------------------
// Instantiates Nin line buffers. It is not optimized if the row buffer is
// implemented by SRAM (TODO). The more efficient way is concatenating all Nin
// channels to form a wider SRAM.
// ----------------------------------------------------------------------------

// Nin line buffers instantiation
genvar i;
generate
  for (i = 0; i < Nin; i = i + 1) begin: line_buffer_gen_i
    line_buffer #(
      .Kh                 (Kh),                 // convolutional kernel dim
      .Kw                 (Kw),
      .h                  (h),                  // input feature map dimension
      .w                  (w),
      .pad_h              (pad_h),              // padding dimension
      .pad_w              (pad_w),
      .BIT_WIDTH          (BIT_WIDTH)           // bit width of each entry
    ) line_buffer_inst
    (
      .clk                (clk),                // system clock
      .rst                (rst),                // system reset

      // control signal
      .line_buffer_valid  (line_buffer_valid),  // line buffer valid
      .line_buffer_zero   (line_buffer_zero),   // line buffer 0 input

      // input data path
      .input_data     (prev_layer_data[i*BIT_WIDTH+:BIT_WIDTH]),
      // output data (sliding window)
      .output_data    (line_buffer_data[i*Kh*Kw*BIT_WIDTH+:Kh*Kw*BIT_WIDTH])
    );
  end
endgenerate

endmodule
