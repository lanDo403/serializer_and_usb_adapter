`timescale 1ns / 1ps

module tx_write_guard(
	input 						clk,
	input 						rst_n,
	input                   clear_i,
	input                   write_enable_i,
	input 						packer_valid_i,
	input 						full_fifo_i,
	input 						tx_fifo_error_i,
	input 						rx_fifo_error_i,
	output 						packer_wen_o,
	output                  tx_fifo_error_wr_o
    );

	reg tx_fifo_error_wr_ff;
	// Two-flop synchronizers bring status/control from CLK domain into gpio_clk domain.
	reg tx_fifo_error_sync1_ff, tx_fifo_error_sync2_ff;
	reg rx_fifo_error_sync1_ff, rx_fifo_error_sync2_ff;
	wire write_blocked;
	wire write_request;

	// Stop accepting packed GPIO words after any sticky TX/RX error until host clears it.
	assign write_blocked = tx_fifo_error_wr_ff || tx_fifo_error_sync2_ff || rx_fifo_error_sync2_ff;
	assign write_request = packer_valid_i && write_enable_i && !write_blocked;
	assign packer_wen_o = write_request;
	assign tx_fifo_error_wr_o = tx_fifo_error_wr_ff;

	// TX-side logic: synchronize host TX error back into gpio_clk and latch local write-side faults.
	always @(posedge clk) begin
		if (!rst_n) begin
			tx_fifo_error_wr_ff <= 1'b0;
			tx_fifo_error_sync1_ff <= 1'b0;
			tx_fifo_error_sync2_ff <= 1'b0;
		end
		else if (clear_i) begin
			tx_fifo_error_wr_ff <= 1'b0;
			tx_fifo_error_sync1_ff <= 1'b0;
			tx_fifo_error_sync2_ff <= 1'b0;
		end
		else begin
			tx_fifo_error_sync1_ff <= tx_fifo_error_i;
			tx_fifo_error_sync2_ff <= tx_fifo_error_sync1_ff;
			if (write_request && full_fifo_i)
				tx_fifo_error_wr_ff <= 1'b1;
		end
	end

	// RX-side logic: synchronize FT-domain RX error into gpio_clk for packer backpressure.
	always @(posedge clk) begin
		if (!rst_n) begin
			rx_fifo_error_sync1_ff <= 1'b0;
			rx_fifo_error_sync2_ff <= 1'b0;
		end
		else if (clear_i) begin
			rx_fifo_error_sync1_ff <= 1'b0;
			rx_fifo_error_sync2_ff <= 1'b0;
		end
		else begin
			rx_fifo_error_sync1_ff <= rx_fifo_error_i;
			rx_fifo_error_sync2_ff <= rx_fifo_error_sync1_ff;
		end
	end

endmodule
