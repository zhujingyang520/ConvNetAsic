// -----------------------------------------------------------------------------
// This file exports the module `concat`, which is the basic module for the
// inception module. In inception module, there exists several branches and
// finally they will concatenate along the depth dimension of the feature map.
// This module mainly deal with the concatenating data as well as the
// valid-ready pair.
// -----------------------------------------------------------------------------

module concat #(
  // Note: Nout is also the concatenated dimension of the all the input feature
  // map no.
  parameter                           Nout = 3,           // output feature map no.
  parameter                           NUM_SPLIT = 3,      // number of splits to be concatenated
  parameter                           BIT_WIDTH = 8       // bit width of the data path
) (
  // previous layer data & valid-ready pair
  input wire  [NUM_SPLIT-1:0]         prev_layer_valid,   // previous layer valid
  output reg  [NUM_SPLIT-1:0]         prev_layer_rdy,     // previous layer ready
  input wire  [Nout*BIT_WIDTH-1:0]    prev_layer_data,    // previous layer data
  // next layer data & valid-ready pair
  input wire                          next_layer_rdy,     // next layer ready
  output wire                         next_layer_valid,   // next layer valid
  output wire [Nout*BIT_WIDTH-1:0]    next_layer_data     // next layer data
);

// generation iterator
genvar g;

// --------------------------------------------------------------
// Next layer data: pass the previous layer data to the next one
// --------------------------------------------------------------
assign next_layer_data = prev_layer_data;

// ----------------------------------------------------------------------------
// Previous layer ready: only pass the next layer ready when all the previous
// layer splits are valid
// ----------------------------------------------------------------------------
generate
  for (g = 0; g < NUM_SPLIT; g = g + 1) begin: prev_layer_rdy_g
    always @ (*) begin
      if (&prev_layer_valid) begin
        prev_layer_rdy[g]           = next_layer_rdy;
      end else begin
        prev_layer_rdy[g]           = 1'b0;
      end
    end
  end
endgenerate

// ----------------------------------------------------
// Next layer valid: AND all previous valid signals
// ----------------------------------------------------
assign next_layer_valid = &prev_layer_valid;

endmodule
