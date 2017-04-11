// -----------------------------------------------------------------------------
// This file exports the module `mux_array`, which is a seperate pipeline stage
// to select the Pin sliding windows from Nin line buffers for the further
// computation.
// -----------------------------------------------------------------------------


module mux_array #(
  parameter                           Nin = 3,        // input feature map number
  parameter                           Pin = 2,        // input feature map parallelism
  parameter                           BIT_WIDTH = 8,  // bit width

  // unmodified parameter
  parameter                           NUM_INPUT = ceil_div(Nin, Pin)
) (
  input wire                          clk,            // system clock
  input wire                          mux_enable,     // enable (active high)
  input wire  [clog2(NUM_INPUT)-1:0]  mux_select,     // mux select signal
  input wire  [Nin*BIT_WIDTH-1:0]     line_buffer_data,   // line buffer sliding window data
  output reg  [Pin*BIT_WIDTH-1:0]     mux_array_out_data  // mux array output data
);

`include "functions.v"

generate
if (Nin == Pin) begin
  // ---------------------------------
  // corner case: Nin = Pin
  // Simple 1 more pipeline stage
  // ---------------------------------
  always @(posedge clk) begin
    if (mux_enable) begin
      mux_array_out_data    <= line_buffer_data;
    end
  end
end else begin
  // -----------------------
  // General case: with MUX
  // -----------------------
  genvar i, j;
  wire [NUM_INPUT*BIT_WIDTH-1:0] mux_input_data [Pin-1:0];
  wire [BIT_WIDTH-1:0] mux_output_data [Pin-1:0];

  // --------------------------
  // assign the mux input data
  // --------------------------
  for (i = 0; i < Pin; i = i + 1) begin: mux_input_data_i
    for (j = 0; j < NUM_INPUT; j = j + 1) begin: mux_input_data_j
      if (i+j*Pin < Nin) begin
        assign mux_input_data[i][j*BIT_WIDTH+:BIT_WIDTH] =
          line_buffer_data[(i+j*Pin)*BIT_WIDTH+:BIT_WIDTH];
      end else begin
        // tailing case: non-divisble Nin / Pin
        assign mux_input_data[i][j*BIT_WIDTH+:BIT_WIDTH] =
          {BIT_WIDTH{1'b0}};
      end
    end
  end

  // ------------------
  // mux instantiation
  // ------------------
  for (i = 0; i < Pin; i = i + 1) begin: mux_inst_i
    mux #(.NUM_INPUT(NUM_INPUT), .BIT_WIDTH(BIT_WIDTH)) mux_inst (
      .input_data    (mux_input_data[i]),                 // input data array
      .select        (mux_select[clog2(NUM_INPUT)-1:0]),  // select control
      .output_data   (mux_output_data[i])                 // output data
    );
  end

  // ----------------
  // Output pipeline
  // ----------------
  for (i = 0; i < Pin; i = i + 1) begin: output_reg_i
    always @(posedge clk) begin
      if (mux_enable) begin
        mux_array_out_data[i*BIT_WIDTH+:BIT_WIDTH] <=
          mux_output_data[i];
      end
    end
  end
end // generate (Nin > Pin)
endgenerate

endmodule
