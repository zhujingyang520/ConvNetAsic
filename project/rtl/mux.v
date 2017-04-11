// -----------------------------------------------------------------------------
// This file exports the module `mux`, which select one data from the given
// input data array.
// -----------------------------------------------------------------------------

module mux #(
  parameter                             NUM_INPUT = 4,  // the number of inputs
  parameter                             BIT_WIDTH = 8   // bit width of each data
)
(
  input wire  [NUM_INPUT*BIT_WIDTH-1:0] input_data,     // input data array
  input wire  [clog2(NUM_INPUT)-1:0]    select,         // select control
  output wire [BIT_WIDTH-1:0]           output_data     // output data
);

`include "functions.v"

// ----------------------------------------------------------------------------
// 2D input array: Verilog does NOT support 2D array as IO port, work around to
// unpack the flatten 1D array to 2D
// -----------------------------------------------------------------------------
genvar i;
// 2D array of input data
wire [BIT_WIDTH-1:0] input_data_2d [NUM_INPUT-1:0];
generate
  for (i = 0; i < NUM_INPUT; i = i + 1) begin: unpack_input_data
    assign input_data_2d[i] = input_data[i*BIT_WIDTH+:BIT_WIDTH];
  end
endgenerate

// ------------------------------------------------------------
// select the desired output data from the input data 2D array
// ------------------------------------------------------------
assign output_data = input_data_2d[select];

endmodule
