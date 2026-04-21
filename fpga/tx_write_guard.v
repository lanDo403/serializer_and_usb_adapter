`timescale 1ns / 1ps

module tx_write_guard(
	input 						clk,
	input 						rst_n,
	input                   soft_clear_i,
	input 						packer_valid_i,
	input 						full_fifo_i,
	input 						tx_fifo_overflow_i,
	input 						tx_fifo_error_i,
	input 						rx_fifo_error_i,
	output 						packer_wen_o
    );

	reg tx_fifo_error_wr_ff;
	// Two-flop synchronizers bring status/control from CLK domain into gpio_clk domain.
	reg tx_fifo_error_sync1_ff, tx_fifo_error_sync2_ff;
	reg rx_fifo_error_sync1_ff, rx_fifo_error_sync2_ff;

	// Stop accepting packed get_gpio words after any sticky TX/RX error until host clears it.
	assign packer_wen_o = packer_valid_i & ~(tx_fifo_error_wr_ff || tx_fifo_error_sync2_ff || rx_fifo_error_sync2_ff);

	always @(posedge clk) begin
		if (!rst_n) begin
			tx_fifo_error_wr_ff <= 1'b0;
			tx_fifo_error_sync1_ff <= 1'b0;
			tx_fifo_error_sync2_ff <= 1'b0;
			rx_fifo_error_sync1_ff <= 1'b0;
			rx_fifo_error_sync2_ff <= 1'b0;
		end
		else if (soft_clear_i) begin
			tx_fifo_error_wr_ff <= 1'b0;
			tx_fifo_error_sync1_ff <= 1'b0;
			tx_fifo_error_sync2_ff <= 1'b0;
			rx_fifo_error_sync1_ff <= 1'b0;
			rx_fifo_error_sync2_ff <= 1'b0;
		end
		else begin
			tx_fifo_error_sync1_ff <= tx_fifo_error_i;
			tx_fifo_error_sync2_ff <= tx_fifo_error_sync1_ff;
			rx_fifo_error_sync1_ff <= rx_fifo_error_i;
			rx_fifo_error_sync2_ff <= rx_fifo_error_sync1_ff;
			if (tx_fifo_overflow_i || (packer_valid_i && full_fifo_i))
				tx_fifo_error_wr_ff <= 1'b1;
		end
	end

endmodule
