`timescale 1ns / 1ps

module rst_sync(
	input clk,
	input arst_i,
	output rst_n_o
    );

	reg rst_ff1;
	reg rst_ff2;

	// Asynchronous assertion keeps startup/reset behavior reliable.
	// Internal logic uses rst_n_o as an active-low synchronous reset in its own domain.
	always @(posedge clk or posedge arst_i) begin
		if (arst_i) begin
			rst_ff1 <= 1'b0;
			rst_ff2 <= 1'b0;
		end
		else begin
			rst_ff1 <= 1'b1;
			rst_ff2 <= rst_ff1;
		end
	end

	assign rst_n_o = rst_ff2;

endmodule
