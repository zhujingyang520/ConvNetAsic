// -----------------------------------------------------------------------------
// This file exports the module `pool_layer_ctrl`, which tracks the computation
// status for a pooling layer, and generates the corresponding control
// signals for the data path.
// -----------------------------------------------------------------------------

module pool_layer_ctrl #(
  parameter                       Kh = 3,           // pooling kernel dimension
  parameter                       Kw = 3,
  parameter                       h = 5,            // input feature map spatial dim
  parameter                       w = 5,
  parameter                       Nin = 3,          // input feature map no.
  parameter                       pad_h = 1,        // feature map padding spatial dim
  parameter                       pad_w = 1,
  parameter                       stride_h = 1,     // stride spatial dim
  parameter                       stride_w = 1,
  parameter                       Pin = 1,          // input feature map parallelism
  parameter                       BIT_WIDTH = 8,    // bit width of data
  parameter                       POOL_PIPELINE = 2 // pooling array pipeline stage
)
(
  input wire                      clk,              // system clock
  input wire                      rst,              // system reset (active high)

  // global control to enable the computation
  input wire                      enable,           // enable computation

  // handshake of previous layer & next layer
  input wire                      prev_layer_valid, // previous layer valid
  output reg                      prev_layer_rdy,   // previous layer ready
  input wire                      next_layer_rdy,   // next layer ready
  output reg                      next_layer_valid, // next layer valid

  // -----------------------
  // data path contol logic
  // -----------------------
  // data path: line buffer
  output reg                      line_buffer_valid,// line buffer valid
  output reg                      line_buffer_zero, // line buffer zero flag

  // data path: line buffer mux array
  output reg                      mux_enable,       // line buffer mux enable
  output reg  [clog2(ceil_div(Nin, Pin))-1:0]
                                  mux_select,       // line buffer mux select

  // data path: pooling unit
  output reg                      pool_array_en,    // pool array enable

  // data path: output register file write path
  // write enable is provided by the nonlinear unit
  output reg  [clog2(ceil_div(Nin, Pin))-1:0]
                                  out_regfile_waddr // output register file write address
);

`include "functions.v"

// --------------------
// Inferred parameters
// --------------------
localparam      PADDED_H = h + 2*pad_h;             // padded feature map dim
localparam      PADDED_W = w + 2*pad_w;
// tiled input & output parallelism
localparam      TILED_IN_PARALLEL = ceil_div(Nin, Pin);
// output register file write pipeline stage
localparam      OUT_REGFILE_WRITE_PIPELINE = 1 + POOL_PIPELINE;
// output ready status counter
localparam      OUT_READY_CNT = 1 + POOL_PIPELINE + 1;

// ---------------------------
// FSM related states
// ---------------------------
localparam      STATE_IDLE = 3'd0,
                STATE_PAD_LEADING_ZERO_ROWS = 3'd1,
                STATE_PAD_LEADING_ZEROS = 3'd2,
                STATE_PAD_TAILING_ZEROS = 3'd3,
                STATE_WAIT_FOR_INPUT = 3'd4,
                STATE_DO_POOLING = 3'd5,
                STATE_PAD_TAILING_ZERO_ROWS = 3'd6,
                STATE_WAIT_FOR_OUT_READY = 3'd7;

// generation iterator
genvar g;

// FSM state register
reg [2:0] state_reg, state_next;
// feature map track index
reg [clog2(PADDED_H)-1:0] h_idx_reg, h_idx_next;
wire [clog2(PADDED_H)-1:0] h_idx_increment;
reg [clog2(PADDED_W)-1:0] w_idx_reg, w_idx_next;
wire [clog2(PADDED_W)-1:0] w_idx_increment;
// stride counter (bypass the strided rows or cols)
reg [clog2(stride_w):0] stride_w_reg, stride_w_next;
wire [clog2(stride_w):0] stride_w_increment;
reg [clog2(stride_h):0] stride_h_reg, stride_h_next;
wire [clog2(stride_h):0] stride_h_increment;
// output ready status counter
reg [clog2(OUT_READY_CNT)-1:0] out_ready_counter, out_ready_counter_next;

// computation related register
// input tiled parallelism & output tiled parallelism index
reg [clog2(TILED_IN_PARALLEL)-1:0] tiled_in_idx_reg, tiled_in_idx_next;
wire [clog2(TILED_IN_PARALLEL)-1:0] tile_in_idx_increment;

// output register module control signals (pipeline to align with the data path)
reg [clog2(ceil_div(Nin, Pin))-1:0] out_regfile_waddr_;
// pipeline signals
reg [clog2(ceil_div(Nin, Pin))-1:0] out_regfile_waddr_pipeline
    [OUT_REGFILE_WRITE_PIPELINE-1:0];

// -----------------------------
// increments of tracking index
// -----------------------------
assign h_idx_increment = h_idx_reg + 1;
assign w_idx_increment = w_idx_reg + 1;

// ---------------------------------------
// increments of the stride counter index
// ---------------------------------------
assign stride_h_increment = stride_h_reg + 1;
assign stride_w_increment = stride_w_reg + 1;

// ------------------------------
// increments of the tiled index
// ------------------------------
assign tile_in_idx_increment    = tiled_in_idx_reg + 1;

// ---------------
// FSM definition
// ---------------
always @ (posedge clk or posedge rst) begin
  if (rst) begin
    state_reg   <= STATE_IDLE;
  end else begin
    state_reg   <= state_next;
  end
end
// FSM next state logic
always @ (*) begin
  // default values (avoid latches)
  h_idx_next        = h_idx_reg;
  w_idx_next        = w_idx_reg;
  stride_h_next     = stride_h_reg;
  stride_w_next     = stride_w_reg;
  state_next        = state_reg;
  tiled_in_idx_next = tiled_in_idx_reg;
  out_ready_counter_next  = out_ready_counter;
  // disable line buffer by default
  line_buffer_valid = 1'b0;
  line_buffer_zero  = 1'b0;
  // disable the ready signal for the previous layer
  prev_layer_rdy    = 1'b0;
  // disbale the valid signal for the next layer
  next_layer_valid  = 1'b0;

  case (state_reg)
    STATE_IDLE: begin
      if (rst == 1'b0 && enable == 1'b1) begin
        if (0 < pad_h) begin
          // pad leading zero rows
          state_next  = STATE_PAD_LEADING_ZERO_ROWS;
        end else if (0 < pad_w) begin
          // pad leading zeros
          state_next  = STATE_PAD_LEADING_ZEROS;
        end else begin
          state_next  = STATE_WAIT_FOR_INPUT;
        end
      end
    end

    STATE_PAD_LEADING_ZERO_ROWS: begin
      // pad pad_h leading rows
      line_buffer_valid   = 1'b1;
      line_buffer_zero    = 1'b1;
      // increment the track index
      w_idx_next          = (w_idx_reg == PADDED_W-1) ? {clog2(PADDED_W){1'b0}} :
        w_idx_increment;
      h_idx_next          = (w_idx_reg == PADDED_W-1) ? h_idx_increment : h_idx_reg;

      // transfer the states
      if (h_idx_reg == pad_h-1 && w_idx_reg == PADDED_W-1) begin
        if (pad_w > 0) begin
          state_next      = STATE_PAD_LEADING_ZEROS;
        end else begin
          state_next      = STATE_WAIT_FOR_INPUT;
        end
      end
    end

    STATE_PAD_LEADING_ZEROS: begin
      // pad pad_w leading zeros of each line
      line_buffer_valid   = 1'b1;
      line_buffer_zero    = 1'b1;
      // increment the track index
      w_idx_next          = w_idx_increment;

      // we also make an assumption that the stride index will hold the original
      // value

      // transfer the state: here we make the assumption that there will be no
      // convolution operation occurs in the leading 0 padding stage. It does
      // make sense for a practical convolutional layer setting otherwise it
      // will create a full 0s feature map
      if (w_idx_reg == pad_w-1) begin
        state_next        = STATE_WAIT_FOR_INPUT;
      end
    end

    STATE_PAD_TAILING_ZEROS: begin
      // pad pad_w zeros of each line
      line_buffer_valid   = 1'b1;
      line_buffer_zero    = 1'b1;
      // increments the track index
      w_idx_next          = (w_idx_reg == PADDED_W-1) ? {clog2(PADDED_W){1'b0}} :
        w_idx_increment;
      if (w_idx_reg == PADDED_W-1 && h_idx_reg == PADDED_H-1) begin
        h_idx_next        = {clog2(PADDED_H){1'b0}};
      end else begin
        h_idx_next        = (w_idx_reg == PADDED_W-1) ? h_idx_increment : h_idx_reg;
      end

      // increment the stride index
      if (stride_w == 1) begin
        // never increments the unit stride counter
        stride_w_next   = 0;
      end else if (w_idx_reg == PADDED_W-1) begin
        // initialize the stride_w_next after each row
        stride_w_next   = 0;
      end else if (w_idx_reg >= Kw-1) begin
        // only enable counting after the 1st sliding window
        stride_w_next   = (stride_w_reg == stride_w-1) ? 0 : stride_w_increment;
      end
      if (stride_h == 1) begin
        // never increments the unit stride counter
        stride_h_next   = 0;
      end else if (w_idx_reg == PADDED_W-1 && h_idx_reg == PADDED_H-1) begin
        // reset stride h when finishing one feature map
        stride_h_next   = 0;
      end else if (h_idx_reg >= Kh-1) begin
        // only enable counting after 1st sliding window
        stride_h_next   = (w_idx_reg != PADDED_W-1) ? stride_h_reg :
          ((stride_h_reg == stride_h-1) ? 0 : stride_h_increment);
      end

      // state transfer
      if (w_idx_reg >= Kw-1 && h_idx_reg >= Kh-1 &&
          stride_h_reg == {(clog2(stride_h)+1){1'b0}} &&
          stride_w_reg == {(clog2(stride_w)+1){1'b0}}) begin
        // do the computation in the next state
        state_next    = STATE_DO_POOLING;
        // reset the computation registers
        tiled_in_idx_next   = {clog2(TILED_IN_PARALLEL){1'b0}};
      end else if (w_idx_reg == PADDED_W-1 && h_idx_reg == PADDED_H-1) begin
        // go to idle
        state_next    = STATE_IDLE;
      end else if (w_idx_reg == PADDED_W-1 && h_idx_reg == h+pad_h-1 &&
        pad_h > 0) begin
        // pad tailing 0 rows
        state_next    = STATE_PAD_TAILING_ZERO_ROWS;
      end else if (w_idx_reg == PADDED_W-1 && pad_w > 0) begin
        // go to pad leading 0 state
        state_next    = STATE_PAD_LEADING_ZEROS;
      end else if (w_idx_reg >= pad_w+w-1 && pad_w > 0) begin
        // go to pad tailing 0 state
        state_next    = STATE_PAD_TAILING_ZEROS;
      end else begin
        state_next    = STATE_WAIT_FOR_INPUT;
      end
    end

    STATE_WAIT_FOR_INPUT: begin
      prev_layer_rdy      = 1'b1;
      if (prev_layer_valid) begin
        // line buffer valid
        line_buffer_valid = 1'b1;

        // increments the track index
        w_idx_next        = (w_idx_reg == PADDED_W-1) ? {clog2(PADDED_W){1'b0}} :
          w_idx_increment;
        if (w_idx_reg == PADDED_W-1 && h_idx_reg == PADDED_H-1) begin
          h_idx_next      = {clog2(PADDED_H){1'b0}};
        end else begin
          h_idx_next      = (w_idx_reg == PADDED_W-1) ? h_idx_increment : h_idx_reg;
        end

        // increment the stride index
        if (stride_w == 1) begin
          // never increments the unit stride counter
          stride_w_next   = 0;
        end else if (w_idx_reg == PADDED_W-1) begin
          // initialize the stride_w_next after each row
          stride_w_next   = 0;
        end else if (w_idx_reg >= Kw-1) begin
          // only enable counting after the 1st sliding window
          stride_w_next   = (stride_w_reg == stride_w-1) ? 0 : stride_w_increment;
        end
        if (stride_h == 1) begin
          // never increments the unit stride counter
          stride_h_next   = 0;
        end else if (w_idx_reg == PADDED_W-1 && h_idx_reg == PADDED_H-1) begin
          stride_h_next   = 0;
        end else if (h_idx_reg >= Kh-1) begin
          // only enable counting after 1st sliding window
          stride_h_next   = (w_idx_reg != PADDED_W-1) ? stride_h_reg :
            ((stride_h_reg == stride_h-1) ? 0 : stride_h_increment);
        end

        // state transfer
        if (w_idx_reg >= Kw-1 && h_idx_reg >= Kh-1 &&
            stride_h_reg == {(clog2(stride_h)+1){1'b0}} &&
            stride_w_reg == {(clog2(stride_w)+1){1'b0}}) begin
          // do the computation in the next state
          state_next    = STATE_DO_POOLING;
          // reset the computation registers
          tiled_in_idx_next   = {clog2(TILED_IN_PARALLEL){1'b0}};
        end else if (w_idx_reg == PADDED_W-1 && h_idx_reg == PADDED_H-1) begin
          // finish computation, go to idle states
          state_next    = STATE_IDLE;
        end else if (w_idx_reg == PADDED_W-1 && pad_w > 0) begin
          // go to pad leading 0 state
          state_next    = STATE_PAD_LEADING_ZEROS;
        end else if (w_idx_reg == pad_w+w-1 && pad_w > 0) begin
          // go to pad tailing 0 state
          state_next    = STATE_PAD_TAILING_ZEROS;
        end else if (w_idx_reg == PADDED_W-1 && h_idx_reg == h+pad_h-1 &&
          pad_h > 0) begin
          state_next    = STATE_PAD_TAILING_ZERO_ROWS;
        end else begin
          state_next    = STATE_WAIT_FOR_INPUT;
        end
      end
    end

    STATE_DO_POOLING: begin
      // do the computation related control here
      // for pooling array only 1 level of loop unroll is applied
      tiled_in_idx_next = (tiled_in_idx_reg == TILED_IN_PARALLEL-1) ?
        {clog2(TILED_IN_PARALLEL){1'b0}} : tile_in_idx_increment;

      // state transfer
      if (tiled_in_idx_reg == TILED_IN_PARALLEL-1) begin
        state_next      = STATE_WAIT_FOR_OUT_READY;
        // clear the out ready counter
        out_ready_counter_next  = {clog2(OUT_READY_CNT){1'b0}};
      end
    end

    STATE_WAIT_FOR_OUT_READY: begin
      if (out_ready_counter == OUT_READY_CNT-1) begin
        next_layer_valid  = 1'b1;
        if (next_layer_rdy) begin
          if (w_idx_reg == {clog2(PADDED_W){1'b0}} && h_idx_reg ==
              {clog2(PADDED_H){1'b0}}) begin
            // finish computation of the one entire feature map
            state_next    = STATE_IDLE;
          end else if (pad_h > 0 && h_idx_reg > h+pad_h-1) begin
            state_next    = STATE_PAD_TAILING_ZERO_ROWS;
          end else if (w_idx_reg < pad_w && pad_w > 0) begin
            // go to pad leading 0 state
            state_next    = STATE_PAD_LEADING_ZEROS;
          end else if (w_idx_reg > pad_w+w-1 && pad_w > 0) begin
            // go to pad tailing 0 state
            state_next    = STATE_PAD_TAILING_ZEROS;
          end else begin
            state_next    = STATE_WAIT_FOR_INPUT;
          end
        end
      end else begin
        out_ready_counter_next  = out_ready_counter + 1;
      end
    end

    STATE_PAD_TAILING_ZERO_ROWS: begin
      // pad pad_h tailing rows
      line_buffer_valid   = 1'b1;
      line_buffer_zero    = 1'b1;
      // increment the track index
      w_idx_next          = (w_idx_reg == PADDED_W-1) ? {clog2(PADDED_W){1'b0}} :
        w_idx_increment;
      if (w_idx_reg == PADDED_W-1 && h_idx_reg == PADDED_H-1) begin
        h_idx_next        = {clog2(PADDED_H){1'b0}};
      end else begin
        h_idx_next        = (w_idx_reg == PADDED_W-1) ? h_idx_increment : h_idx_reg;
      end

      // increment the stride index
      if (stride_w == 1) begin
        // never increments the unit stride counter
        stride_w_next   = 0;
      end else if (w_idx_reg == PADDED_W-1) begin
        // initialize the stride_w_next after each row
        stride_w_next   = 0;
      end else if (w_idx_reg >= Kw-1) begin
        // only enable counting after the 1st sliding window
        stride_w_next   = (stride_w_reg == stride_w-1) ? 0 : stride_w_increment;
      end
      if (stride_h == 1) begin
        // never increments the unit stride counter
        stride_h_next   = 0;
      end else if (w_idx_reg == PADDED_W-1 && h_idx_reg == PADDED_H-1) begin
        stride_h_next   = 0;
      end else if (h_idx_reg >= Kh-1) begin
        // only enable counting after 1st sliding window
        stride_h_next   = (w_idx_reg != PADDED_W-1) ? stride_h_reg :
          ((stride_h_reg == stride_h-1) ? 0 : stride_h_increment);
      end

      // state transfer
      if (w_idx_reg >= Kw-1 && h_idx_reg >= Kh-1 &&
        stride_h_reg == {(clog2(stride_h)+1){1'b0}} &&
        stride_w_reg == {(clog2(stride_w)+1){1'b0}}) begin
        // do the computation in the next state
        state_next    = STATE_DO_POOLING;
        // reset the computation registers
        tiled_in_idx_next   = {clog2(TILED_IN_PARALLEL){1'b0}};
      end else if (w_idx_reg == PADDED_W-1 && h_idx_reg == PADDED_H-1) begin
        state_next    = STATE_IDLE;
      end else begin
        // stay @ padding last pad_h rows of 0
        state_next    = STATE_PAD_TAILING_ZERO_ROWS;
      end
    end
  endcase
end

// ------------------------
// Feature map track index
// ------------------------
always @ (posedge clk or posedge rst) begin
  if (rst) begin
    h_idx_reg   <= {clog2(PADDED_H){1'b0}};
    w_idx_reg   <= {clog2(PADDED_W){1'b0}};
  end else begin
    h_idx_reg   <= h_idx_next;
    w_idx_reg   <= w_idx_next;
  end
end

// ---------------------
// stride counter index
// ---------------------
always @ (posedge clk or posedge rst) begin
  if (rst) begin
    stride_h_reg  <= {(clog2(stride_h)+1){1'b0}};
    stride_w_reg  <= {(clog2(stride_w)+1){1'b0}};
  end else begin
    stride_h_reg  <= stride_h_next;
    stride_w_reg  <= stride_w_next;
  end
end

// --------------------------------------------------------
// Output ready status counter: finish the entire pipeline
// --------------------------------------------------------
always @(posedge clk or posedge rst) begin
  if (rst) begin
    out_ready_counter <= {clog2(OUT_READY_CNT){1'b0}};
  end else begin
    out_ready_counter <= out_ready_counter_next;
  end
end

// ---------------------------
// computing related register
// ---------------------------
always @ (posedge clk or posedge rst) begin
  if (rst) begin
    tiled_in_idx_reg  <= {clog2(TILED_IN_PARALLEL){1'b0}};
  end else begin
    tiled_in_idx_reg  <= tiled_in_idx_next;
  end
end

// ------------------------------------
// line buffer mux array control logic
// ------------------------------------
always @ (*) begin
  // disable the mux array enable by default
  mux_enable    = 1'b0;

  if (state_reg == STATE_DO_POOLING) begin
    // enable the line buffer mux enable only when do pooling operation
    mux_enable  = 1'b1;
  end
end

always @ (*) begin
  mux_select    = tiled_in_idx_reg;
end

// ----------------------------
// Pooling array control
// ----------------------------
// pool array enable: delay one cycle
always @ (posedge clk or posedge rst) begin
  if (rst) begin
    pool_array_en         <= 1'b0;
  end else begin
    pool_array_en         <= (state_reg == STATE_DO_POOLING);
  end
end

// -------------------------------------
// Output register file write path
// -------------------------------------
// Write data to the output register each cycle
always @(*) begin
  out_regfile_waddr_    = {clog2(ceil_div(Nin, Pin)){1'b0}};
  if (state_reg == STATE_DO_POOLING) begin
    out_regfile_waddr_  = tiled_in_idx_reg;
  end
end
// write data path control pipeline
generate
for (g = 0; g < OUT_REGFILE_WRITE_PIPELINE; g = g + 1) begin: out_regfile_write_g
  if (g == 0) begin
    always @(posedge clk) begin
      out_regfile_waddr_pipeline[g] <= out_regfile_waddr_;
    end
  end else begin
    always @(posedge clk) begin
      out_regfile_waddr_pipeline[g] <= out_regfile_waddr_pipeline[g-1];
    end
  end
end
endgenerate
// primary output assignment
always @(*) begin
  out_regfile_waddr = out_regfile_waddr_pipeline[OUT_REGFILE_WRITE_PIPELINE-1];
end

endmodule
