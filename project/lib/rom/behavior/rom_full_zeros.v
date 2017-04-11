// -----------------------------------------------------------------------------
// This file exports the behavior model of a full 0s ROM `rom_full_zeros`. It
// always outputs 0 when the chip enable is asserted.
// -----------------------------------------------------------------------------

// --------------------------------------
// Active low rom access
// --------------------------------------
`define ROM_ENABLE 1'b0
`define ROM_DISABLE 1'b1

module rom_full_zeros #(
  parameter                   ROM_DEPTH = 1024,   // rom depth
  parameter                   NUM_DATA = 1,       // the number of data in a memory word
  parameter                   BIT_WIDTH = 16      // bit width of one data
) (
  input wire                  clk,                // system clock
  input wire                  cen,                // chip enable (active low)
  input wire  [clog2(ROM_DEPTH)-1:0]
                              A,                  // rom read address
  output reg  [NUM_DATA*BIT_WIDTH-1:0]
                              Q                   // rom read data
);

// ---------------------------
// Ceiling of log2
// ---------------------------
function integer clog2(input integer n);
  integer i;
  begin
    clog2 = 0;
    for (i = n - 1; i > 0; i = i >> 1) begin
      clog2   = clog2 + 1;
    end
  end
endfunction

// -----------------------------------------------------------
// Behavior of the ROM of full 0s (output 0 when chip enable)
// -----------------------------------------------------------
always @(posedge clk) begin
  if (cen == `ROM_ENABLE) begin
    Q         <= {(NUM_DATA*BIT_WIDTH){1'b0}};
  end else begin
    Q         <= {(NUM_DATA*BIT_WIDTH){1'bx}};
  end
end

endmodule
