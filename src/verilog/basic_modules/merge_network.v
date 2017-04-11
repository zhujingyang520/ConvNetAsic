// -----------------------------------------------------------------------------
// This file exports the module `merge_network`, which is a basic module to do
// the merge operation of the given N inputs.
// -----------------------------------------------------------------------------

module merge_network #(
  parameter                       NUM_INPUTS = 18,   // number of inputs to be merged
  parameter                       BIT_WIDTH = 8     // bit width of each element
) (
  input wire  [NUM_INPUTS*BIT_WIDTH-1:0]
                                  in_data,          // input data to be merged
  output reg  [BIT_WIDTH-1:0]     merge_result      // merge result
);

// ---------------------------------------------------------------------------
// The Synthesized Tool will automatically optimize the data path into
// a tree-based structure
// ---------------------------------------------------------------------------
integer i;
always @(*) begin
  merge_result    = {BIT_WIDTH{1'b0}};
  for (i = 0; i < NUM_INPUTS; i = i + 1) begin
    merge_result  = merge_result + in_data[i*BIT_WIDTH+:BIT_WIDTH];
  end
end

endmodule
