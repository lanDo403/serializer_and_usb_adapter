`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date:    11:30:00 09/10/2025 
// Design Name: 
// Module Name:    sram_dp 
// Project Name: 
// Target Devices: 
// Tool versions: 
// Description: 
//
// Dependencies: 
//
// Revision: 
// Revision 0.01 - File Created
// Additional Comments: 
//
//////////////////////////////////////////////////////////////////////////////////
module sram_dp
#(
	parameter DATA_LEN = 32,
	parameter DEPTH = 8192,
	parameter ADDR_LEN = $clog2(DEPTH)
)
(
	input							wr_clk,
	input 						rd_clk,
	input 						wen,
	input 						ren,
	input [ADDR_LEN-1:0] 	wr_addr,
	input [ADDR_LEN-1:0] 	rd_addr,
	input [DATA_LEN-1:0]  	data_i,
	output [DATA_LEN-1:0] 	data_o
    );
	 
	reg [DATA_LEN-1:0] sram [0:DEPTH-1];
	reg [DATA_LEN-1:0] ram_data_ff;
	
	always @(posedge wr_clk) begin
		if (wen)
			sram[wr_addr] <= data_i;
	end
	
	always @(posedge rd_clk) begin
		if (ren)
			ram_data_ff <= sram[rd_addr];
	end
	
	assign data_o = ram_data_ff;

endmodule
