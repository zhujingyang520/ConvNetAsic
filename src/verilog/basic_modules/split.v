// -----------------------------------------------------------------------------
// This file exports the module `split`, which is the basic module to implement
// the inception module in the ConvNet. In the inception module, there are
// always several branches in the inception module. In order to transfer the
// ready-valid handshake pair correctly before/after split layer, the split
// module simply makes sure the protocal is consistent.
// -----------------------------------------------------------------------------

module split #(
  parameter                           Nin = 3,            // input feature map no.
  parameter                           NUM_SPLIT = 3,      // number of splits (at least 2)
  parameter                           BIT_WIDTH = 8       // bit width of the data path
) (
  input wire                          prev_layer_valid,   // previous layer valid
  output wire                         prev_layer_rdy,     // previous layer ready
  input wire  [Nin*BIT_WIDTH-1:0]     prev_layer_data,    // previous layer data

  // next layer data & its handshake of the NUM_SPLIT branches
  input wire  [NUM_SPLIT-1:0]         next_layer_rdy,     // next layer ready
  output reg  [NUM_SPLIT-1:0]         next_layer_valid,   // next layer valid
  output wire [NUM_SPLIT*Nin*BIT_WIDTH-1:0]
                                      next_layer_data     // next layer data
);

// generation iterator
genvar g;

// ------------------------------------------------
// Next layer data: fanout the previous layer data
// ------------------------------------------------
generate
  for (g = 0; g < NUM_SPLIT; g = g + 1) begin: next_layer_data_g
    assign next_layer_data[g*Nin*BIT_WIDTH +: Nin*BIT_WIDTH] =
      prev_layer_data;
  end
endgenerate

// --------------------------------------------------
// Previous layer ready: AND of the next layer ready
// --------------------------------------------------
assign prev_layer_rdy = &next_layer_rdy;

// ---------------------------------------------------------------------------
// Next layer valid: only transfer the previous layer valid when all the next
// layer ready signals are asserted
// ----------------------------------------------------------------------------
generate
  for (g = 0; g < NUM_SPLIT; g = g + 1) begin: next_layer_valid_g
    always @ (*) begin
      if (&next_layer_rdy) begin
        next_layer_valid[g]           = prev_layer_valid;
      end else begin
        next_layer_valid[g]           = 1'b0;
      end
    end
  end
endgenerate

endmodule
