`timescale 1ns / 1ps

module fifo_tx_ctrl(
	input 						clk,
	input 						rst_n,
	input 						packer_valid_i,
	input 						full_fifo_i,
	input 						tx_fifo_overflow_i,
	input 						clr_tx_error_tgl_i,
	input 						tx_fifo_error_i,
	input 						rx_fifo_error_i,
	output 						packer_wen_o
    );

	reg tx_fifo_error_wr_ff;
	// Two-flop synchronizers bring status/control from CLK domain into gpio_clk domain.
	reg clr_tx_error_sync1_ff, clr_tx_error_sync2_ff;
	reg tx_fifo_error_sync1_ff, tx_fifo_error_sync2_ff;
	reg rx_fifo_error_sync1_ff, rx_fifo_error_sync2_ff;
	wire clr_tx_error_pulse_w;

	assign clr_tx_error_pulse_w = clr_tx_error_sync1_ff ^ clr_tx_error_sync2_ff;
	// Stop accepting packed get_gpio words after any sticky TX/RX error until host clears it.
	assign packer_wen_o = packer_valid_i & ~(tx_fifo_error_wr_ff || tx_fifo_error_sync2_ff || rx_fifo_error_sync2_ff);

	always @(posedge clk) begin
		if (!rst_n) begin
			tx_fifo_error_wr_ff <= 1'b0;
			clr_tx_error_sync1_ff <= 1'b0;
			clr_tx_error_sync2_ff <= 1'b0;
			tx_fifo_error_sync1_ff <= 1'b0;
			tx_fifo_error_sync2_ff <= 1'b0;
			rx_fifo_error_sync1_ff <= 1'b0;
			rx_fifo_error_sync2_ff <= 1'b0;
		end
		else begin
			clr_tx_error_sync1_ff <= clr_tx_error_tgl_i;
			clr_tx_error_sync2_ff <= clr_tx_error_sync1_ff;
			tx_fifo_error_sync1_ff <= tx_fifo_error_i;
			tx_fifo_error_sync2_ff <= tx_fifo_error_sync1_ff;
			rx_fifo_error_sync1_ff <= rx_fifo_error_i;
			rx_fifo_error_sync2_ff <= rx_fifo_error_sync1_ff;
			if (clr_tx_error_pulse_w)
				tx_fifo_error_wr_ff <= 1'b0;
			else if (tx_fifo_overflow_i || (packer_valid_i && full_fifo_i))
				tx_fifo_error_wr_ff <= 1'b1;
		end
	end

endmodule
