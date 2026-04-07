`timescale 1ns / 1ps

module fifo_rx_ctrl #(
	parameter DATA_LEN = 32,
	parameter BE_LEN = 4,
	parameter FIFO_RX_LEN = DATA_LEN + BE_LEN
)(
	input 						clk,
	input 						rst_n,
	input 						fifo_rx_empty_i,
	input 						fifo_rx_ren_i,
	input [FIFO_RX_LEN-1:0] fifo_rx_data_i,
	input 						tx_fifo_underflow_i,
	input 						rx_fifo_overflow_i,
	input 						rx_fifo_underflow_i,
	output 						fifo_rx_pop_o,
	output 						tx_fifo_error_o,
	output 						rx_fifo_error_o,
	output 						clr_tx_error_tgl_o
    );

	localparam [DATA_LEN-1:0] CMD_CLR_TX_ERROR = 32'h00000001;
	localparam [DATA_LEN-1:0] CMD_CLR_RX_ERROR = 32'h00000002;
	localparam [DATA_LEN-1:0] CMD_CLR_ALL_ERROR = 32'h00000003;

	reg fifo_rx_ren_d;
	reg tx_fifo_error_ff;
	reg rx_fifo_error_ff;
	reg clr_tx_error_tgl_ff;

	// RX FIFO is drained immediately because received words are treated as commands/status.
	assign fifo_rx_pop_o = ~fifo_rx_empty_i;
	assign tx_fifo_error_o = tx_fifo_error_ff;
	assign rx_fifo_error_o = rx_fifo_error_ff;
	// Toggle is synchronized into gpio_clk domain and converted there to a clear pulse.
	assign clr_tx_error_tgl_o = clr_tx_error_tgl_ff;

	always @(posedge clk) begin
		if (!rst_n) begin
			fifo_rx_ren_d <= 1'b0;
			tx_fifo_error_ff <= 1'b0;
			rx_fifo_error_ff <= 1'b0;
			clr_tx_error_tgl_ff <= 1'b0;
		end
		else begin
			// SRAM read data is valid one CLK later than ren_o.
			fifo_rx_ren_d <= fifo_rx_ren_i;
			if (tx_fifo_underflow_i)
				tx_fifo_error_ff <= 1'b1;
			if (rx_fifo_overflow_i || rx_fifo_underflow_i)
				rx_fifo_error_ff <= 1'b1;
			if (fifo_rx_ren_d && (fifo_rx_data_i[FIFO_RX_LEN-1:DATA_LEN] == {BE_LEN{1'b1}})) begin
				case (fifo_rx_data_i[DATA_LEN-1:0])
					CMD_CLR_TX_ERROR: begin
						tx_fifo_error_ff <= 1'b0;
						clr_tx_error_tgl_ff <= ~clr_tx_error_tgl_ff;
					end
					CMD_CLR_RX_ERROR: rx_fifo_error_ff <= 1'b0;
					CMD_CLR_ALL_ERROR: begin
						tx_fifo_error_ff <= 1'b0;
						rx_fifo_error_ff <= 1'b0;
						clr_tx_error_tgl_ff <= ~clr_tx_error_tgl_ff;
					end
				endcase
			end
		end
	end

endmodule
