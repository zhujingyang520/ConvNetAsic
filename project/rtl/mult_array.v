// -----------------------------------------------------------------------------
// This file exports the module `mult_array`, which is the main stage for doing
// the multiplication of the CONV.
// -----------------------------------------------------------------------------

module mult_array #(
  parameter                       Kh = 3,         // convolutional kernel dimension
  parameter                       Kw = 3,
  parameter                       Pin = 2,        // input feature map parallelism
  parameter                       Pout = 1,       // output feature map parallelism
  parameter                       BIT_WIDTH = 8,  // bit width of the data path
  // advanced setting: pipeline stage (retiming for a better performance)
  // Note: should be at least 1 pipeline stage
  parameter                       PIPELINE_STAGE = 1
) (
  input wire                      clk,            // system clock
  input wire                      rst,            // system reset
  input wire                      mult_array_en,  // multiplier array enable (active high)
  input wire  [Pin*Kh*Kw*BIT_WIDTH-1:0]
                                  feat_map_in,    // feature map input (from line buffer)
  input wire  [Pout*Pin*Kh*Kw*BIT_WIDTH-1:0]
                                  kernel_in,      // kernel input (from weight mem)

  output reg                      mult_array_valid, // multiplier array valid
  output reg  [Pout*Pin*Kh*Kw*BIT_WIDTH-1:0]
                                  mult_array_data // multiplier array data output
);

// generation iterator
genvar i, j, k, l;
// intermediate multiplier results
wire [2*BIT_WIDTH-1:0] multiplier_result [Pout-1:0][Pin-1:0][Kh-1:0][Kw-1:0];
// truncated multiplier results
wire [Pout*Pin*Kh*Kw*BIT_WIDTH-1:0] multiplier_trunc;
// intermediate pipeline stages of the multiplier array
reg [PIPELINE_STAGE-1:0] mult_array_valid_pipeline;
reg [Pout*Pin*Kh*Kw*BIT_WIDTH-1:0] mult_array_data_pipeline [PIPELINE_STAGE-1:0];

// -------------------------------------
// Multiplier array no.: Pout*Pin*Kh*Kw
// -------------------------------------
generate
  for (i = 0; i < Pout; i = i + 1) begin: multiplier_inst_i
    for (j = 0; j < Pin; j = j + 1) begin: multiplier_inst_j
      for (k = 0; k < Kh; k = k + 1) begin: multiplier_inst_k
        for (l = 0; l < Kw; l = l + 1) begin: multiplier_inst_l
          multiplier #(.BIT_WIDTH(BIT_WIDTH)) multiplier_inst (
            // 2 input operands
            .a      (feat_map_in[(j*Kh*Kw+k*Kw+l)*BIT_WIDTH +: BIT_WIDTH]),
            .b      (kernel_in[(i*Pin*Kh*Kw+j*Kh*Kw+k*Kw+l)*BIT_WIDTH +: BIT_WIDTH]),
            .result (multiplier_result[i][j][k][l])  // multiplication result
          );
        end
      end
    end
  end
endgenerate

// --------------------------------------------------------------------------
// TODO: truncation network: from 2*BIT_WIDTH to BIT_WIDTH
// Right now, we truncate the LSB (should be determined from the fixed point
// simulation of the ConvNet.
// --------------------------------------------------------------------------
generate
  for (i = 0; i < Pout; i = i + 1) begin: multiplier_trunc_i
    for (j = 0; j < Pin; j = j + 1) begin: multiplier_trunc_j
      for (k = 0; k < Kh; k = k + 1) begin: multiplier_trunc_k
        for (l = 0; l < Kw; l = l + 1) begin: multiplier_trunc_l
          assign multiplier_trunc[(i*Pin*Kh*Kw+j*Kh*Kw+k*Kw+l)*BIT_WIDTH+:BIT_WIDTH] =
            multiplier_result[i][j][k][l][BIT_WIDTH-1:0];
        end
      end
    end
  end
endgenerate

// -------------------------------------
// Pipeline stage
// -------------------------------------
// multiplier array enable
generate
  for (i = 0; i < PIPELINE_STAGE; i = i + 1) begin: mult_array_valid_i
    if (i == 0) begin
      // first stage: direct connect to the primary input
      always @(posedge clk or posedge rst) begin
        if (rst) begin
          mult_array_valid_pipeline[i]  <= 1'b0;
        end else begin
          mult_array_valid_pipeline[i]  <= mult_array_en;
        end
      end
    end else begin
      // remaining stages: connect to the proceeding one
      always @(posedge clk or posedge rst) begin
        if (rst) begin
          mult_array_valid_pipeline[i]  <= 1'b0;
        end else begin
          mult_array_valid_pipeline[i]  <= mult_array_valid_pipeline[i-1];
        end
      end
    end
  end
endgenerate
// multiplier array data
generate
  for (i = 0; i < PIPELINE_STAGE; i = i + 1) begin: mult_array_data_i
    if (i == 0) begin
      // first stage: direct connect to the multiplication results
      always @ (posedge clk) begin
        if (mult_array_en) begin
          mult_array_data_pipeline[i]   <= multiplier_trunc;
        end
      end
    end else begin
      // remaining stages: connect to the proceeding one
      always @ (posedge clk) begin
        if (mult_array_valid_pipeline[i-1]) begin
          mult_array_data_pipeline[i]   <= mult_array_data_pipeline[i-1];
        end
      end
    end
  end
endgenerate

// ---------------------------
// Assign the primary outputs
// ---------------------------
always @(*) begin
  mult_array_valid = mult_array_valid_pipeline[PIPELINE_STAGE-1];
  mult_array_data = mult_array_data_pipeline[PIPELINE_STAGE-1];
end

endmodule
