// -----------------------------------------------------------------------------
// This file exports the module `add_array`, which is the main stage for doing
// the accumulation of the CONV.
// -----------------------------------------------------------------------------

module add_array #(
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
  input wire                      add_array_en,   // adder array enable (active high)
  input wire  [Pout*Pin*Kh*Kw*BIT_WIDTH-1:0]
                                  mult_array_data,// data from the multiplier array

  output reg                      add_array_valid,// add array valid
  output reg  [Pout*BIT_WIDTH-1:0]add_array_data  // add array output data
);

// generation iterator
genvar i;
// merge results
wire [Pout*BIT_WIDTH-1:0] merge_result;
// intermediate pipeline stages for the add array
reg [PIPELINE_STAGE-1:0] add_array_valid_pipeline;
reg [Pout*BIT_WIDTH-1:0] add_array_data_pipeline [PIPELINE_STAGE-1:0];

// ------------------------------------
// Merge network no.: Pout
// ------------------------------------
generate
  for (i = 0; i < Pout; i = i + 1) begin: merge_network_inst_i
    merge_network #(
      .NUM_INPUTS                 (Pin*Kh*Kw),
      .BIT_WIDTH                  (BIT_WIDTH)
    ) merge_network_inst (
      .in_data                    (mult_array_data[i*Pin*Kh*Kw*BIT_WIDTH+:Pin*Kh*Kw*BIT_WIDTH]),
      .merge_result               (merge_result[i*BIT_WIDTH+:BIT_WIDTH])
    );
  end
endgenerate

// ------------------------------------
// Pipeline stage
// ------------------------------------
// adder array enable
generate
  for (i = 0; i < PIPELINE_STAGE; i = i + 1) begin: add_array_valid_i
    if (i == 0) begin
      // first stage: direct connect to the primary input
      always @(posedge clk or posedge rst) begin
        if (rst) begin
          add_array_valid_pipeline[i] <= 1'b0;
        end else begin
          add_array_valid_pipeline[i] <= add_array_en;
        end
      end
    end else begin
      // remaining stages: connected to the proceeding one
      always @ (posedge clk) begin
        if (rst) begin
          add_array_valid_pipeline[i] <= 1'b0;
        end else begin
          add_array_valid_pipeline[i] <= add_array_valid_pipeline[i-1];
        end
      end
    end
  end
endgenerate
// add array data
generate
  for (i = 0; i < PIPELINE_STAGE; i = i + 1) begin: add_array_data_i
    if (i == 0) begin
      // first stage: direct connect to the merge results
      always @ (posedge clk) begin
        if (add_array_en) begin
          add_array_data_pipeline[i]  <= merge_result;
        end
      end
    end else begin
      // remaining stages: connect to the proceeding one
      always @ (posedge clk) begin
        if (add_array_valid_pipeline[i-1]) begin
          add_array_data_pipeline[i]  <= add_array_data_pipeline[i-1];
        end
      end
    end
  end
endgenerate

// ---------------------------
// Assign the primary outputs
// ---------------------------
always @(*) begin
  add_array_valid = add_array_valid_pipeline[PIPELINE_STAGE-1];
  add_array_data = add_array_data_pipeline[PIPELINE_STAGE-1];
end

endmodule
