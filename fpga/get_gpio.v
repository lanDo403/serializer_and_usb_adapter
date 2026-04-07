`timescale 1ns / 1ps

module get_gpio #(
	parameter GPIO_LEN = 8
)(
	input clk_i,
	input strob_i,	
	input [GPIO_LEN-1:0] data_i,
	
	output [GPIO_LEN-1:0] data_o,
   output strob_o,
	output clk_o
    );
	 
	// input buffers signals
	wire [GPIO_LEN-1:0] data_buf;
	wire strob_buf;	
	wire clk_buf;
	// output regs
	reg [GPIO_LEN-1:0] data_rx;
	reg strob_rx;
	
	//---------------------------------------------------------------------
	// input buffers
	//---------------------------------------------------------------------
	IBUFG #(
		.IOSTANDARD("LVCMOS33") // Specify the output I/O standard
	) IBUFG_clk (
		.O(clk_buf),     // Buffer output
		.I(clk_i)      // Diff_p buffer input (connect directly to top-level port) 
	);
	
	/*
	BUFG global_clk_buffer(
		.O(clk_buf),
		.I(clk_buf_i)
	);
	*/
	
	genvar i;
	generate
		for (i = 0; i < GPIO_LEN; i = i + 1) begin : data_bufs
			IBUF #(
				.IOSTANDARD("LVCMOS33") // Specify the output I/O standard
			) IBUF_data (
				.O(data_buf[i]),     // Buffer output
				.I(data_i[i])      // Diff_p buffer input (connect directly to top-level port) 
			);
		end
	endgenerate

	IBUF #(
		.IOSTANDARD("LVCMOS33") // Specify the output I/O standard
	) IBUF_strob (
		.O(strob_buf),     // Buffer output
		.I(strob_i)      // Diff_p buffer input (connect directly to top-level port) 
	);
	
	always @(posedge clk_buf) begin 
		data_rx <= data_buf;
		strob_rx <= strob_buf;
	end
	
	assign data_o 	= data_rx;
	assign strob_o = strob_rx;
	assign clk_o = clk_buf;

endmodule
