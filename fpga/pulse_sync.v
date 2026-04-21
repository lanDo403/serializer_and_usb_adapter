`timescale 1ns / 1ps

module pulse_sync(
	input  src_clk,
	input  src_rst_n,
	input  dst_clk,
	input  dst_rst_n,
	input  pulse_i,
	output pulse_o
);

	reg src_toggle_ff;
	reg dst_sync1_ff;
	reg dst_sync2_ff;

	always @(posedge src_clk) begin
		if (!src_rst_n)
			src_toggle_ff <= 1'b0;
		else if (pulse_i)
			src_toggle_ff <= ~src_toggle_ff;
	end

	always @(posedge dst_clk) begin
		if (!dst_rst_n) begin
			dst_sync1_ff <= 1'b0;
			dst_sync2_ff <= 1'b0;
		end
		else begin
			dst_sync1_ff <= src_toggle_ff;
			dst_sync2_ff <= dst_sync1_ff;
		end
	end

	assign pulse_o = dst_sync1_ff ^ dst_sync2_ff;

endmodule
