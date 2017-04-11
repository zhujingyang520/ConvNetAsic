// -----------------------------------------------------------------------------
// This file exports the module `pool_array`, which is the stage for doing the
// pooling operation (max or avg).
// -----------------------------------------------------------------------------

module pool_array #(
  parameter                         Kh = 3,           // pooling kernel dimension
  parameter                         Kw = 3,
  parameter                         Pin = 2,          // input feature map parallelism
  parameter                         BIT_WIDTH = 8,    // bit width of the data path
  // pooling method: support for {MAX, AVG}
  parameter                         POOL_METHOD = "MAX",
  // advanced setting: pipeline stage (retiming for a better performance)
  // Note: should be at least 1 pipeline stage
  parameter                         PIPELINE_STAGE = 1
) (
  input wire                        clk,              // system clock
  input wire                        rst,              // system reset
  input wire                        pool_array_en,    // pool array enable
  input wire  [Pin*Kh*Kw*BIT_WIDTH-1:0]
                                    pool_data,        // pool data (from line buffer)

  output reg                        pool_array_valid, // pool array valid signal
  output reg  [Pin*BIT_WIDTH-1:0]   pool_array_data
);

// -------------------
// interconnections
// -------------------
// pool array calculated results
reg [BIT_WIDTH-1:0] pool_result [Pin-1:0];
wire [Pin*BIT_WIDTH-1:0] pool_result_flatten;
// pool array pipeline stage
reg pool_array_valid_pipeline [PIPELINE_STAGE-1:0];
reg [Pin*BIT_WIDTH-1:0] pool_array_data_pipeline [PIPELINE_STAGE-1:0];

integer i;
genvar g;

generate
  if (POOL_METHOD == "MAX") begin
    for (g = 0; g < Pin; g = g + 1) begin: max_array_g
      always @ (*) begin
        pool_result[g]      = pool_data[g*Kh*Kw*BIT_WIDTH +: BIT_WIDTH];
        for (i = 1; i < Kh*Kw; i = i + 1) begin
          if (pool_result[g] < pool_data[(g*Kh*Kw+i)*BIT_WIDTH +: BIT_WIDTH]) begin
            pool_result[g]  = pool_data[(g*Kh*Kw+i)*BIT_WIDTH +: BIT_WIDTH];
          end
        end
      end
    end
  end else if (POOL_METHOD == "AVG") begin
    // internal summing network
    wire [BIT_WIDTH-1:0] sum_result [Pin-1:0];
    // calculate the sum
    for (g = 0; g < Pin; g = g + 1) begin: merge_network_g
      merge_network # (
        .NUM_INPUTS                 (Kh*Kw),
        .BIT_WIDTH                  (BIT_WIDTH)
      ) merge_network_inst (
        .in_data                    (pool_data[g*Kh*Kw*BIT_WIDTH+:Kh*Kw*BIT_WIDTH]),
        .merge_result               (sum_result[g])
      );
    end
    // do the avg
    for (g = 0; g < Pin; g = g + 1) begin: avg_array_g
      always @ (*) begin
        // TODO: divided by a constant
        pool_result[g]              = sum_result[g] / (Kh*Kw);
      end
    end
  end else begin
    //$display("undefined pooling method: %s", POOL_METHOD);
  end
endgenerate

// ----------------------------
// Flatten the pooling result
// ----------------------------
generate
  for (g = 0; g < Pin; g = g + 1) begin: pool_result_flatten_g
    assign pool_result_flatten[g*BIT_WIDTH+:BIT_WIDTH] = pool_result[g];
  end
endgenerate

// ----------------------------
// Pipeline stage
// ----------------------------
// pooling array enable
generate
  for (g = 0; g < PIPELINE_STAGE; g = g + 1) begin: pool_array_valid_g
    if (g == 0) begin
      // first stage: direct connect to the primiary input
      always @ (posedge clk or posedge rst) begin
        if (rst) begin
          pool_array_valid_pipeline[g]  <= 1'b0;
        end else begin
          pool_array_valid_pipeline[g]  <= pool_array_en;
        end
      end
    end else begin
      // remaining stages: connect to the previous one
      always @ (posedge clk or posedge rst) begin
        if (rst) begin
          pool_array_valid_pipeline[g]  <= 1'b0;
        end else begin
          pool_array_valid_pipeline[g]  <= pool_array_valid_pipeline[g-1];
        end
      end
    end
  end
endgenerate
// pooling array data
generate
  for (g = 0; g < PIPELINE_STAGE; g = g + 1) begin: pool_array_data_g
    if (g == 0) begin
      // first stage: direct connect to the pooling results
      always @ (posedge clk) begin
        if (pool_array_en) begin
          pool_array_data_pipeline[g]   <= pool_result_flatten;
        end
      end
    end else begin
      // remaining stages: connect to the previous one
      always @ (posedge clk) begin
        if (pool_array_valid_pipeline[g-1]) begin
          pool_array_data_pipeline[g]   <= pool_array_data_pipeline[g-1];
        end
      end
    end
  end
endgenerate

// ---------------------------
// Primary output assignment
// ---------------------------
always @ (*) begin
  pool_array_valid = pool_array_valid_pipeline[PIPELINE_STAGE-1];
  pool_array_data = pool_array_data_pipeline[PIPELINE_STAGE-1];
end

endmodule
