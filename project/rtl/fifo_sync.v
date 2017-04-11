// -----------------------------------------------------------------------------
// This file exports the module `fifo_sync`, which is the synchronous FIFO. The
// memory logic is implemented by the registers (DFFs). It is possible to use
// dual port SRAM for a more area-efficient implementation under large FIFO
// depth.
// -----------------------------------------------------------------------------

module fifo_sync #(
  parameter                     BIT_WIDTH = 8,        // bit width
  parameter                     FIFO_DEPTH = 8        // fifo depth
) (
  input wire                    clk,                  // system clock
  input wire                    rst,                  // system reset

  // read path
  input wire                    read_en,              // read enable (active high)
  output wire [BIT_WIDTH-1:0]   read_data,            // read data

  // write path
  input wire                    write_en,             // write enable (active high)
  input wire  [BIT_WIDTH-1:0]   write_data,           // write data

  // status indicator
  output wire                   fifo_empty,           // empty indicator
  output wire                   fifo_full             // full indicator
);

`include "functions.v"

localparam ADDR_WIDTH = clog2(FIFO_DEPTH);

// -----------------------------
// Read pointer & Write pointer
// -----------------------------
reg [ADDR_WIDTH-1:0] read_ptr_reg, read_ptr_next;
reg [ADDR_WIDTH-1:0] write_ptr_reg, write_ptr_next;
wire [ADDR_WIDTH-1:0] read_ptr_increment;
wire [ADDR_WIDTH-1:0] write_ptr_increment;

// -----------------------------
// FIFO memory (regitsers)
// -----------------------------
reg [BIT_WIDTH-1:0] fifo_mem [FIFO_DEPTH-1:0];

// -----------------------------
// 2 status DFFs (full & empty)
// -----------------------------
reg fifo_full_reg, fifo_full_next;
reg fifo_empty_reg, fifo_empty_next;

// ---------------------------------
// Increment of pointers (adder)
// ---------------------------------
assign read_ptr_increment = (read_ptr_reg == FIFO_DEPTH-1) ? {ADDR_WIDTH{1'b0}} :
  read_ptr_reg + 1;
assign write_ptr_increment = (write_ptr_reg == FIFO_DEPTH-1) ? {ADDR_WIDTH{1'b0}} :
  write_ptr_reg + 1;

// ---------------------------------
// Registers for control logic
// ---------------------------------
always @ (posedge clk or posedge rst) begin
  if (rst) begin
    read_ptr_reg      <= {ADDR_WIDTH{1'b0}};
    write_ptr_reg     <= {ADDR_WIDTH{1'b0}};
    fifo_full_reg     <= 1'b0;
    fifo_empty_reg    <= 1'b1;
  end else begin
    read_ptr_reg      <= read_ptr_next;
    write_ptr_reg     <= write_ptr_next;
    fifo_full_reg     <= fifo_full_next;
    fifo_empty_reg    <= fifo_empty_next;
  end
end

// -------------------------------
// Write path
// -------------------------------
always @ (posedge clk) begin
  if (~fifo_full_reg && write_en) begin
    fifo_mem[write_ptr_reg]   <= write_data;
  end
end

// ---------------------------------
// Read path (no pipeline of read)
// ---------------------------------
assign read_data = (fifo_empty_reg == 1'b1 || read_en == 1'b0) ? {BIT_WIDTH{1'b0}} :
                    fifo_mem[read_ptr_reg];

// ---------------------------------
// next logic combination logic
// ---------------------------------
always @ (*) begin
  // default value: keep the old value
  read_ptr_next   = read_ptr_reg;
  write_ptr_next  = write_ptr_reg;
  fifo_full_next  = fifo_full_reg;
  fifo_empty_next = fifo_empty_reg;

  case ({read_en, write_en})
    2'b00: begin
      // no operation: keep the old value
    end
    2'b01: begin
      // write operation
      if (fifo_full_reg == 1'b0) begin
        write_ptr_next  = write_ptr_increment;
        fifo_empty_next = 1'b0;
        if (write_ptr_increment == read_ptr_reg) begin
          fifo_full_next = 1'b1;
        end
      end
    end
    2'b10: begin
      // read operation
      if (fifo_empty_reg == 1'b0) begin
        read_ptr_next   = read_ptr_increment;
        fifo_full_next  = 1'b0;
        if (read_ptr_increment == write_ptr_reg) begin
          fifo_empty_next = 1'b1;
        end
      end
    end
    2'b11: begin
      // read & write simultanuously
      read_ptr_next   = read_ptr_increment;
      write_ptr_next  = write_ptr_increment;
    end
  endcase
end

// ------------------------------------------------------------------
// Behavior simulation: additional counter tracking the buffer depth
// and the max buffer depth during the simulation
// ------------------------------------------------------------------
`ifdef BEHAV_SIM

reg [ADDR_WIDTH-1:0] fifo_depth_reg;
reg [ADDR_WIDTH-1:0] fifo_max_depth;
wire fifo_depth_incre = write_en && !fifo_full_reg && !read_en;
wire fifo_depth_decre = read_en && !fifo_empty_reg && !write_en;

always @ (posedge clk or posedge rst) begin
  if (rst) begin
    fifo_depth_reg    <= {ADDR_WIDTH{1'b0}};
  end else if (fifo_depth_incre) begin
    fifo_depth_reg    <= fifo_depth_reg + 1;
  end else if (fifo_depth_decre) begin
    fifo_depth_reg    <= fifo_depth_reg - 1;
  end
end

always @ (posedge clk or negedge rst) begin
  if (rst) begin
    fifo_max_depth    <= {ADDR_WIDTH{1'b0}};
  end else if (fifo_max_depth < fifo_depth_reg) begin
    fifo_max_depth    <= fifo_depth_reg;
  end
end

`endif

// --------------------------
// Primary output assignment
// --------------------------
assign fifo_empty = fifo_empty_reg;
assign fifo_full = fifo_full_reg;

endmodule
