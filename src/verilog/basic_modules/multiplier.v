// -----------------------------------------------------------------------------
// This file exports the basic arithmetic unit of `multiplier`. Here we use the
// signed multiplier.
// -----------------------------------------------------------------------------

module multiplier #(parameter BIT_WIDTH = 8) (
  input wire signed   [BIT_WIDTH-1:0]       a,      // 2 input operands
  input wire signed   [BIT_WIDTH-1:0]       b,
  output wire signed  [2*BIT_WIDTH-1:0]     result  // multiplication result
);

assign result = (a == {BIT_WIDTH{1'b0}} || b == {BIT_WIDTH{1'b0}}) ?
  {(2*BIT_WIDTH){1'b0}} : a * b;

endmodule
