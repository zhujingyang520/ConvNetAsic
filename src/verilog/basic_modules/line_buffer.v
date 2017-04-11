// -----------------------------------------------------------------------------
// This file exports the module `line_buffer`, which exports the sliding window
// for the current convolution operation.
// -----------------------------------------------------------------------------

module line_buffer #(
  parameter                     Kh = 3,           // convolutional kernel dim
  parameter                     Kw = 3,
  parameter                     h = 5,            // input feature map dimension
  parameter                     w = 5,
  parameter                     pad_h = 1,        // padding dimension
  parameter                     pad_w = 1,
  parameter                     BIT_WIDTH = 16    // bit width of each entry
)
(
  input wire                    clk,              // system clock
  input wire                    rst,              // system reset

  // control signal
  input wire                    line_buffer_valid,// line buffer valid
  input wire                    line_buffer_zero, // line buffer 0 input

  // input data path
  input wire  [BIT_WIDTH-1:0]   input_data,       // line buffer input data
  // output data (sliding window)
  output wire [Kh*Kw*BIT_WIDTH-1:0] output_data   // line buffer output data
);

// --------------------
// Inferred parameters
// --------------------
localparam    PADDED_H = h + 2*pad_h;             // padded feature map dim
localparam    PADDED_W = w + 2*pad_w;
localparam    ROW_BUF_WIDTH = BIT_WIDTH*(Kh-1);   // row buffer width
localparam    ROW_BUF_DEPTH = PADDED_W-Kw;        // row buffer depth

// ---------------------------
// Explict sliding window DFF
// ---------------------------
reg [Kw*BIT_WIDTH-1:0] sliding_dff_array [Kh-1:0];

// iterator
genvar i, j;

generate
  if (ROW_BUF_DEPTH > 0 && ROW_BUF_WIDTH > 0) begin
    // with row buffers

    // -------------------------
    // row buffer instantiation
    // -------------------------
    wire [ROW_BUF_WIDTH-1:0] row_buffer_input_data;
    wire [ROW_BUF_WIDTH-1:0] row_buffer_output_data;
    row_buffer #(
      .BIT_WIDTH  (ROW_BUF_WIDTH),                // bit width of the row buffer
      .BUF_DEPTH  (ROW_BUF_DEPTH)                 // depth of the row buffer
    ) row_buffer_inst (
      .clk        (clk),                          // system clock
      .rst        (rst),                          // system reset

      // control signal: read & write happen simultaniously for row buffer
      .enable     (line_buffer_valid),            // enable read & write path
      .read_data  (row_buffer_output_data),       // row buffer output data
      .write_data (row_buffer_input_data)         // row buffer input data
    );

    // -------------------------
    // sliding window dff array
    // -------------------------
    for (i = 0; i < Kh; i = i + 1) begin: sliding_dff_array_i
      for (j = 0; j < Kw; j = j + 1) begin: sliding_dff_array_j
        if (i == 0 && j == 0) begin
          // input dff
          always @(posedge clk) begin
            if (line_buffer_valid) begin
              if (line_buffer_zero) begin
                // infer the mux in the DFF input
                sliding_dff_array[i][j*BIT_WIDTH+:BIT_WIDTH] <= {BIT_WIDTH{1'b0}};
              end else begin
                sliding_dff_array[i][j*BIT_WIDTH+:BIT_WIDTH] <= input_data;
              end
            end
          end
        end // i == 0 && j == 0

        else if (j == 0) begin
          // input dffs except 1st row
          always @(posedge clk) begin
            if (line_buffer_valid) begin
              sliding_dff_array[i][j*BIT_WIDTH+:BIT_WIDTH] <=
                row_buffer_output_data[(i-1)*BIT_WIDTH+:BIT_WIDTH];
            end
          end
        end // i != 0 && j == 0

        else begin
          // not the 1st dff
          always @(posedge clk) begin
            if (line_buffer_valid) begin
              // cascade the current dff to the previous dff
              sliding_dff_array[i][j*BIT_WIDTH+:BIT_WIDTH] <=
                sliding_dff_array[i][(j-1)*BIT_WIDTH+:BIT_WIDTH];
            end
          end
        end // j != 0

      end // generate j loop
    end   // generate i loop

    // ----------------------------------
    // Row buffer input data connections
    // ----------------------------------
    for (i = 0; i < Kh-1; i = i + 1) begin: row_buffer_input_data_i
      // connect to the sliding window dff array
      assign row_buffer_input_data[i*BIT_WIDTH+:BIT_WIDTH] =
        sliding_dff_array[i][(Kw-1)*BIT_WIDTH+:BIT_WIDTH];
    end
  end     // row buffer exists: ROW_BUF_WIDTH & ROW_BUF_DEPTH > 0
  else begin
    // row buffer does NOT exist
    // -------------------------
    // sliding window dff array
    // -------------------------
    for (i = 0; i < Kh; i = i + 1) begin: sliding_dff_array_i
      for (j = 0; j < Kw; j = j + 1) begin: sliding_dff_array_j
        if (i == 0 && j == 0) begin
          // input dff
          always @(posedge clk) begin
            if (line_buffer_valid) begin
              if (line_buffer_zero) begin
                // infer the mux in the DFF input
                sliding_dff_array[i][j*BIT_WIDTH+:BIT_WIDTH] <= {BIT_WIDTH{1'b0}};
              end else begin
                sliding_dff_array[i][j*BIT_WIDTH+:BIT_WIDTH] <= input_data;
              end
            end
          end
        end // i == 0 && j == 0

        else if (j == 0) begin
          // input dffs except 1st row
          always @(posedge clk) begin
            if (line_buffer_valid) begin
              sliding_dff_array[i][j*BIT_WIDTH+:BIT_WIDTH] <=
                sliding_dff_array[i-1][(Kw-1)*BIT_WIDTH+:BIT_WIDTH];
            end
          end
        end // i != 0 && j == 0

        else begin
          // not the 1st dff
          always @(posedge clk) begin
            if (line_buffer_valid) begin
              // cascade the current dff to the previous dff
              sliding_dff_array[i][j*BIT_WIDTH+:BIT_WIDTH] <=
                sliding_dff_array[i][(j-1)*BIT_WIDTH+:BIT_WIDTH];
            end
          end
        end // j != 0

      end
    end
  end
endgenerate

// --------------------------
// Export the output signals
// --------------------------
generate
for (i = 0; i < Kh; i = i + 1) begin: output_data_i
  for (j = 0; j < Kw; j = j + 1) begin: output_data_j
    assign output_data[i*Kw*BIT_WIDTH+j*BIT_WIDTH+:BIT_WIDTH] =
      sliding_dff_array[i][j*BIT_WIDTH+:BIT_WIDTH];
  end
end
endgenerate

endmodule
