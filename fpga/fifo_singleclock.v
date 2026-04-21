`timescale 1ns / 1ps

module fifo_singleclock #(
	parameter DATA_LEN = 32,
	parameter DEPTH = 8192,
	parameter ADDR_LEN = $clog2(DEPTH)
)(
	input                    clk,
	input                    rst_n,
	input                    soft_clear_i,
	input                    wen_i,
	input                    ren_i,
	input  [DATA_LEN-1:0]    data_i,
	output [DATA_LEN-1:0]    data_o,
	output                   full,
	output                   empty,
	output                   overflow,
	output                   underflow
);

	reg [DATA_LEN-1:0] mem [0:DEPTH-1];
	reg [DATA_LEN-1:0] data_ff;
	reg [ADDR_LEN:0] wr_ptr_ff;
	reg [ADDR_LEN:0] rd_ptr_ff;
	reg full_ff;
	reg empty_ff;
	reg overflow_ff;
	reg underflow_ff;

	wire wen_do;
	wire ren_do;
	wire [ADDR_LEN:0] wr_ptr_next;
	wire [ADDR_LEN:0] rd_ptr_next;
	wire full_next;
	wire empty_next;

	assign wen_do = wen_i && !full_ff;
	assign ren_do = ren_i && !empty_ff;
	assign wr_ptr_next = wr_ptr_ff + {{ADDR_LEN{1'b0}}, wen_do};
	assign rd_ptr_next = rd_ptr_ff + {{ADDR_LEN{1'b0}}, ren_do};
	assign empty_next = (wr_ptr_next == rd_ptr_next);
	assign full_next = (wr_ptr_next[ADDR_LEN] != rd_ptr_next[ADDR_LEN]) &&
	                   (wr_ptr_next[ADDR_LEN-1:0] == rd_ptr_next[ADDR_LEN-1:0]);

	always @(posedge clk) begin
		if (!rst_n) begin
			wr_ptr_ff <= {ADDR_LEN+1{1'b0}};
			rd_ptr_ff <= {ADDR_LEN+1{1'b0}};
			data_ff <= {DATA_LEN{1'b0}};
			full_ff <= 1'b0;
			empty_ff <= 1'b1;
			overflow_ff <= 1'b0;
			underflow_ff <= 1'b0;
		end
		else if (soft_clear_i) begin
			wr_ptr_ff <= {ADDR_LEN+1{1'b0}};
			rd_ptr_ff <= {ADDR_LEN+1{1'b0}};
			data_ff <= {DATA_LEN{1'b0}};
			full_ff <= 1'b0;
			empty_ff <= 1'b1;
			overflow_ff <= 1'b0;
			underflow_ff <= 1'b0;
		end
		else begin
			if (wen_do)
				mem[wr_ptr_ff[ADDR_LEN-1:0]] <= data_i;
			else if (wen_i && full_ff)
				overflow_ff <= 1'b1;

			if (ren_do)
				data_ff <= mem[rd_ptr_ff[ADDR_LEN-1:0]];
			else if (ren_i && empty_ff)
				underflow_ff <= 1'b1;

			wr_ptr_ff <= wr_ptr_next;
			rd_ptr_ff <= rd_ptr_next;
			full_ff <= full_next;
			empty_ff <= empty_next;
		end
	end

	assign data_o = data_ff;
	assign full = full_ff;
	assign empty = empty_ff;
	assign overflow = overflow_ff;
	assign underflow = underflow_ff;

endmodule
