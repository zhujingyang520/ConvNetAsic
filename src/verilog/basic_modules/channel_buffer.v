// -----------------------------------------------------------------------------
// This file exports the module `channel_buffer`, including the synchronous FIFO
// as the basic memory system to store the data.
// -----------------------------------------------------------------------------

module channel_buffer #(
  parameter                           BIT_WIDTH = 8,        // bit width
  parameter                           BUFFER_DEPTH = 8,     // buffer depth
  parameter                           NUM_CHANNEL = 3       // channel number
) (
  input wire                          clk,                  // system clock
  input wire                          rst,                  // system reset

  // input data path & its handshake
  input wire                          prev_layer_valid,     // previous layer valid
  output wire                         prev_layer_rdy,       // previous layer ready
  input wire  [NUM_CHANNEL*BIT_WIDTH-1:0]
                                      prev_layer_data,      // previous layer data

  // output data path & its handshake
  input wire                          next_layer_rdy,       // next layer ready
  output wire                         next_layer_valid,     // next layer valid
  output wire [NUM_CHANNEL*BIT_WIDTH-1:0]
                                      next_layer_data       // next layer data
);

generate
if (BUFFER_DEPTH == 0) begin: genblk0
  // no buffer depth: simple wire connection
  assign next_layer_valid = prev_layer_valid;
  assign prev_layer_rdy   = next_layer_rdy;
  assign next_layer_data  = prev_layer_data;
end else begin: genblk1
  // use FIFO as the storage element
  reg fifo_write_en, fifo_read_en;
  reg [NUM_CHANNEL*BIT_WIDTH-1:0] fifo_write_data;
  wire [NUM_CHANNEL*BIT_WIDTH-1:0] fifo_read_data;
  wire fifo_empty, fifo_full;
  localparam  TX_IDLE         = 1'b0,
              TX_WAIT_FOR_RDY = 1'b1;
  reg next_layer_valid_reg, next_layer_valid_next;
  reg [NUM_CHANNEL*BIT_WIDTH-1:0] next_layer_data_reg, next_layer_data_next;
  reg state_reg, state_next;
  // FIFO bypass: it is possible to directly output the signal
  wire fifo_bypass = (state_reg == TX_IDLE && next_layer_rdy);
  fifo_sync #(
    .BIT_WIDTH            (NUM_CHANNEL*BIT_WIDTH),  // bit width
    .FIFO_DEPTH           (BUFFER_DEPTH)            // fifo depth
  ) fifo_sync_inst (
    .clk                  (clk),                    // system clock
    .rst                  (rst),                    // system reset

    // read path
    .read_en              (fifo_read_en),           // read enable (active high)
    .read_data            (fifo_read_data),         // read data

    // write path
    .write_en             (fifo_write_en),          // write enable (active high)
    .write_data           (fifo_write_data),        // write data

    // status indicator
    .fifo_empty           (fifo_empty),             // empty indicator
    .fifo_full            (fifo_full)               // full indicator
  );

  // -------------------------
  // RX path
  // -------------------------
  // previous layer ready: if the fifo is full, deassert the ready
  assign prev_layer_rdy   = (fifo_full == 1'b0) && (rst == 1'b0);
  // FIFO write path
  always @ (*) begin
    if (!fifo_full && prev_layer_valid && !fifo_bypass) begin
      fifo_write_en       = 1'b1;
      fifo_write_data     = prev_layer_data;
    end else begin
      fifo_write_en       = 1'b0;
      fifo_write_data     = {(NUM_CHANNEL*BIT_WIDTH){1'b0}};
    end
  end

  // -------------------------
  // TX path
  // -------------------------

  always @ (posedge clk or posedge rst) begin
    if (rst == 1'b1) begin
      state_reg             <= TX_IDLE;
      next_layer_valid_reg  <= 1'b0;
    end else begin
      state_reg             <= state_next;
      next_layer_valid_reg  <= next_layer_valid_next;
    end
  end
  always @ (posedge clk) begin
    next_layer_data_reg   <= next_layer_data_next;
  end

  always @ (*) begin
    state_next            = state_reg;
    next_layer_valid_next = next_layer_valid_reg;
    next_layer_data_next  = next_layer_data_reg;
    fifo_read_en          = 1'b0;
    case (state_reg)
      TX_IDLE: begin
        if (~fifo_empty) begin
          fifo_read_en          = 1'b1;
          next_layer_valid_next = 1'b1;
          next_layer_data_next  = fifo_read_data;
          state_next            = TX_WAIT_FOR_RDY;
        end
      end

      TX_WAIT_FOR_RDY: begin
        if (next_layer_rdy) begin
          next_layer_valid_next = 1'b0;
          if (fifo_empty) begin
            state_next    = TX_IDLE;
          end else begin
            state_next    = TX_WAIT_FOR_RDY;
            fifo_read_en  = 1'b1;
            next_layer_valid_next = 1'b1;
            next_layer_data_next  = fifo_read_data;
          end
        end
      end
    endcase
  end

  // ---------------------------
  // Primiary output assignment
  // ---------------------------
  assign next_layer_data = fifo_bypass ? prev_layer_data : next_layer_data_reg;
  assign next_layer_valid = fifo_bypass ? prev_layer_valid : next_layer_valid_reg;

end
endgenerate

endmodule
