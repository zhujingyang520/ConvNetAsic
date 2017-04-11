// -----------------------------------------------------------------------------
// This file exports the module `out_regfile`, which is the register file for
// the partial computation results in the CONV.
// -----------------------------------------------------------------------------

module out_regfile #(
  parameter                       Nout = 3,         // output feature map number
  parameter                       Pout = 2,         // output feature map parallelism
  parameter                       BIT_WIDTH = 8     // bit width of each element
)
(
  input wire                      clk,              // system clock

  // write path
  input wire                      write_en,         // write enable (active high)
  input wire  [clog2(ceil_div(Nout, Pout))-1:0]
                                  write_addr,       // write address
  input wire  [Pout*BIT_WIDTH-1:0] write_data,      // write data
  // read path
  input wire                      read_en,          // read enable (active high)
  input wire  [clog2(ceil_div(Nout, Pout))-1:0]
                                  read_addr,        // read address
  output reg  [Pout*BIT_WIDTH-1:0]read_data,        // read data (no. = Pout)

  // register file memory: for output feature map
  output wire [Nout*BIT_WIDTH-1:0]regfile           // all the contents of register file
);

`include "functions.v"

// -------------------------------------
// register file memory (infer as DFFs)
// -------------------------------------
reg [BIT_WIDTH-1:0] dff_array [Nout-1:0];

// iterator
integer i;
genvar g;

// -----------------------------
// Write data path
// -----------------------------
always @ (posedge clk) begin
  if (write_en) begin
    // write to the address set by write_addr
    for (i = 0; i < Nout; i = i + 1) begin
      if (i >= write_addr*Pout && i < write_addr*Pout+Pout) begin
        dff_array[i]  <= write_data[(i-write_addr*Pout)*BIT_WIDTH+:BIT_WIDTH];
      end
    end
  end
end

// -----------------------------
// Read data path
// -----------------------------
always @ (posedge clk) begin
  if (read_en) begin
    if (write_en && read_addr == write_addr) begin
      read_data       <= write_data;
    end else begin
      for (i = 0; i < Pout; i = i + 1) begin
        if (read_addr*Pout+i < Nout) begin
          read_data[i*BIT_WIDTH+:BIT_WIDTH] <= dff_array[read_addr*Pout+i];
        end else begin
          read_data[i*BIT_WIDTH+:BIT_WIDTH] <= {BIT_WIDTH{1'b0}};
        end
      end
    end
  end
end

// -----------------------------
// Output regfile export
// -----------------------------
generate
  for (g = 0; g < Nout; g = g + 1) begin: regfile_out_g
    assign regfile[g*BIT_WIDTH+:BIT_WIDTH] = dff_array[g];
  end
endgenerate

endmodule
