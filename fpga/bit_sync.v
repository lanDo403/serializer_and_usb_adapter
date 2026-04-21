`timescale 1ns / 1ps

module bit_sync #(
	parameter RESET_VALUE = 1'b0
)(
	input  clk,
	input  rst_n,
	input  din,
	output dout
);

	reg sync1_ff;
	reg sync2_ff;

	always @(posedge clk) begin
		if (!rst_n) begin
			sync1_ff <= RESET_VALUE;
			sync2_ff <= RESET_VALUE;
		end
		else begin
			sync1_ff <= din;
			sync2_ff <= sync1_ff;
		end
	end

	assign dout = sync2_ff;

endmodule
