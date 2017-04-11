// -----------------------------------------------------------------------------
// This file exports the module `testbench`. It applys the stimulus to the
// ConvNet accelerator, and monitor the output of the ConvNet accelerator.
//
// Note: this module is not intended for synthesize!
// -----------------------------------------------------------------------------

module testbench #(
  parameter                       Nin = 3,                // input feature map number
  parameter                       Nout = 3,               // output feature map number
  parameter                       BIT_WIDTH = 8,          // bit width of the data path
  parameter                       INPUT_SPATIAL_DIM = 25, // input feature map spatial dimension
  parameter                       FRAME_SIZE = 1          // frame size for finish simulation
) (
  input wire                      clk,                    // system clock
  input wire                      rst,                    // system reset

  // input data path & its handshake protocol
  input wire                      input_layer_rdy,        // input layer ready
  output reg                      input_layer_valid,      // input layer data valid
  output reg [Nin*BIT_WIDTH-1:0]  input_layer_data,       // input layer data

  // output data path & its handshake protocol
  input wire                      output_layer_valid,     // output layer data valid
  input wire [Nout*BIT_WIDTH-1:0] output_layer_data,      // output layer data
  output reg                      output_layer_rdy,       // output layer ready
  output reg                      stop_sim
);

// ------------------------------
// TX data path
// ------------------------------
reg [BIT_WIDTH-1:0] data_tx;
integer ii;
integer fp;   // file pointer
// boolean flag for received output indicator
reg received_output;
reg [clog2(INPUT_SPATIAL_DIM*FRAME_SIZE)-1:0] pixel_counter;

initial begin
  $timeformat(-9, 2, "", 10);
  fp = $fopen("tx_time.list", "w");
end

always @ (posedge clk or posedge rst) begin
  if (rst) begin
    data_tx           <= {{(BIT_WIDTH-1){1'b0}}, 1'b1};
    input_layer_valid <= 1'b0;
    pixel_counter     <= 0;
    stop_sim          <= 1'b0;
  end else begin
    input_layer_valid <= 1'b1;

    // if the input data is ready, increments the data to send
    if (input_layer_rdy) begin
      $display("@%0t[ns] testbench sends: %0d", $time, data_tx);
      data_tx         <= data_tx + 1;
      $fdisplay(fp, "%t", $time);
      if (received_output) begin
        pixel_counter <= pixel_counter + 1;
      end
      // received all the frames already
      if (pixel_counter == INPUT_SPATIAL_DIM*FRAME_SIZE-1) begin
        $fclose(fp);
        stop_sim      <= 1'b1;
      end
    end
  end
end
always @ (*) begin
  for (ii = 0; ii < Nin; ii = ii + 1) begin
    input_layer_data[ii*BIT_WIDTH+:BIT_WIDTH] = data_tx;
  end
end

// ------------------------------
// RX data path
// ------------------------------
integer o;
always @ (posedge clk or posedge rst) begin
  if (rst) begin
    output_layer_rdy  <= 1'b0;
    received_output   <= 1'b0;
  end else begin
    // assume the rx end has infinite bandwidth
    output_layer_rdy  <= 1'b1;
    // receive some valid data from the output of the accelerator
    if (output_layer_valid) begin
      received_output <= 1'b1;
      $write("@%0t[ns] testbench receives: ", $time);
      for (o = 0; o < Nout; o = o + 1) begin
        $write("%0d ", output_layer_data[o*BIT_WIDTH+:BIT_WIDTH]);
      end
      $write("\n");
    end
  end
end

// ---------------------------
// Ceiling of log2
// ---------------------------
function integer clog2(input integer n);
  integer i;
  begin
    clog2 = 0;
    for (i = n - 1; i > 0; i = i >> 1) begin
      clog2 = clog2 + 1;
    end
  end
endfunction

endmodule
