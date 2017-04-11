// -----------------------------------------------------------------------------
// This file exports the module `row_buffer`, which is the basic element in the
// line buffer. The row buffer written here will be inferred to an array of
// registers rather than SRAM.
// -----------------------------------------------------------------------------

module row_buffer #(
  parameter                       BIT_WIDTH = 16,   // bit width of each entry
  parameter                       BUF_DEPTH = 15    // buffer depth
)
(
  input wire                      clk,              // system clock
  input wire                      rst,              // system reset (active high)

  // control signal: read & write happen simultaniously for row buffer
  input wire                      enable,           // enable read & write path
  // read data: the oldest element in the buffer
  output reg [BIT_WIDTH-1:0]      read_data,        // read data
  // write data: the latest element in the buffer
  input wire  [BIT_WIDTH-1:0]     write_data        // write data
);

`include "functions.v"

// --------------------------------------------
// memory element (synthesized into DFF array)
// --------------------------------------------
reg [BIT_WIDTH-1:0] dff_array [BUF_DEPTH-1:0];

generate
  if (BUF_DEPTH == 1) begin
    // corner case: buffer depth = 1
    always @ (posedge clk) begin
      if (enable) begin
        dff_array[0]  <= write_data;
      end
    end

    always @(*) begin
      read_data       = dff_array[0];
    end
  end
  else begin
    // read & write pointer register
    reg [clog2(BUF_DEPTH)-1:0] rw_ptr_reg;

    // -----------------------------
    // Read pointer register update
    // -----------------------------
    always @ (posedge clk or posedge rst) begin
      if (rst) begin
        rw_ptr_reg    <= {clog2(BUF_DEPTH){1'b0}};
      end else if (enable) begin
        rw_ptr_reg    <= (rw_ptr_reg == BUF_DEPTH-1) ? {clog2(BUF_DEPTH){1'b0}} :
          rw_ptr_reg + 1;
      end
    end

    // ----------------
    // Write data path
    // ----------------
    always @ (posedge clk) begin
      if (enable) begin
        dff_array[rw_ptr_reg]  <= write_data;
      end
    end

    // ---------------
    // Read data path
    // ---------------
    always @ (*) begin
      read_data       = dff_array[rw_ptr_reg];
    end
  end
endgenerate

endmodule
