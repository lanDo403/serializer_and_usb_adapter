`timescale 1ns / 1ps

module async_fifo#(
	parameter DATA_LEN = 32,
	parameter DEPTH = 8192,
	parameter ADDR_LEN = $clog2(DEPTH)
)(
	input 						clk_wr,
	input							clk_rd,
	input 						rst_wr_n,
	input 						rst_rd_n,
	input                   clear_wr_i,
	input                   clear_rd_i,
	input 						wen_i, // write enable from packer8to32	
	input 						ren_i,  // read enable from FT601
	input [DATA_LEN-1:0] 	sram_data_r,
	input [DATA_LEN-1:0] 	data_i,
	output [DATA_LEN-1:0]  	data_o,
	output [DATA_LEN-1:0]	sram_data_w, 
	output 						wen_o, // write enable to memory
	output [ADDR_LEN-1:0]	wr_addr_o,
	output [ADDR_LEN-1:0]	rd_addr_o,
	output 						full,			
	output 						empty
    ); 
	 
	reg full_ff;
	reg empty_ff;
	wire full_next_w, empty_next_r;
	
	reg [ADDR_LEN:0] wr_ptr_bin, wr_ptr_bin_next;
	reg [ADDR_LEN:0] rd_ptr_bin, rd_ptr_bin_next;
	reg [ADDR_LEN:0] wr_ptr_gray, wr_ptr_gray_next;
	reg [ADDR_LEN:0] rd_ptr_gray, rd_ptr_gray_next;
	
	reg [ADDR_LEN:0] wr_ptr_gray_sync1, wr_ptr_gray_sync2;
	reg [ADDR_LEN:0] rd_ptr_gray_sync1, rd_ptr_gray_sync2;
	
	reg wen_do, ren_do;
	
	// Write domain pointers (gray pointers and address)
	always @(posedge clk_wr) begin
		if (!rst_wr_n) begin
			wr_ptr_bin <= 0;
			wr_ptr_gray <= 0;
			full_ff <= 0;
		end
		else if (clear_wr_i) begin
			wr_ptr_bin <= 0;
			wr_ptr_gray <= 0;
			full_ff <= 0;
		end
		else begin
			wr_ptr_bin <= wr_ptr_bin_next;
			wr_ptr_gray <= wr_ptr_gray_next;
			full_ff <= full_next_w;
		end
	end
	
	always @(*) begin
		wen_do = wen_i & ~full;
		wr_ptr_bin_next = wr_ptr_bin + {{ADDR_LEN{1'b0}}, wen_do};
		wr_ptr_gray_next = (wr_ptr_bin_next >> 1) ^ wr_ptr_bin_next;
	end
	
	// Read domain pointers (gray pointers and address)
	always @(posedge clk_rd) begin
		if (!rst_rd_n) begin
			rd_ptr_bin <= 0;
			rd_ptr_gray <= 0;
			empty_ff <= 1;
		end
		else if (clear_rd_i) begin
			rd_ptr_bin <= 0;
			rd_ptr_gray <= 0;
			empty_ff <= 1;
		end
		else begin
			rd_ptr_bin <= rd_ptr_bin_next;
			rd_ptr_gray <= rd_ptr_gray_next;
			empty_ff <= empty_next_r;
		end
	end

	always @(*) begin
		ren_do = ren_i & ~empty;
		rd_ptr_bin_next = rd_ptr_bin + {{ADDR_LEN{1'b0}}, ren_do};
		rd_ptr_gray_next = (rd_ptr_bin_next >> 1) ^ rd_ptr_bin_next;
	end
	
	// Synchronize Gray pointers between write and read domains.
	always @(posedge clk_wr) begin
		if (!rst_wr_n) begin
			rd_ptr_gray_sync1 <= 0;
			rd_ptr_gray_sync2 <= 0;
		end
		else if (clear_wr_i) begin
			rd_ptr_gray_sync1 <= 0;
			rd_ptr_gray_sync2 <= 0;
		end
		else begin
			rd_ptr_gray_sync1 <= rd_ptr_gray;
			rd_ptr_gray_sync2 <= rd_ptr_gray_sync1;
		end
	end
	
	always @(posedge clk_rd) begin
		if (!rst_rd_n) begin
			wr_ptr_gray_sync1 <= 0;
			wr_ptr_gray_sync2 <= 0;
		end
		else if (clear_rd_i) begin
			wr_ptr_gray_sync1 <= 0;
			wr_ptr_gray_sync2 <= 0;
		end
		else begin
			wr_ptr_gray_sync1 <= wr_ptr_gray;
			wr_ptr_gray_sync2 <= wr_ptr_gray_sync1;
		end
	end
	
	// Output signals
	assign empty_next_r = (rd_ptr_gray_next == wr_ptr_gray_sync2);
	assign full_next_w  = (wr_ptr_gray_next == {~rd_ptr_gray_sync2[ADDR_LEN:ADDR_LEN-1], rd_ptr_gray_sync2[ADDR_LEN-2:0]});
	
	assign full = full_ff;
	assign empty = empty_ff;
	assign wen_o = wen_do;
	assign wr_addr_o = wr_ptr_bin[ADDR_LEN-1:0];
	assign rd_addr_o = rd_ptr_bin[ADDR_LEN-1:0];
	assign sram_data_w = data_i;
	assign data_o = sram_data_r;

endmodule
