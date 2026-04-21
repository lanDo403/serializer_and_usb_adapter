`timescale 1ns / 1ps

module status_ft_ctrl #(
	parameter DATA_LEN = 32
)(
	input                    clk,
	input                    rst_n,
	input                    soft_clear_i,
	input                    service_hold_i,
	input                    ft_idle_i,
	input                    status_req_i,
	input                    status_pop_i,
	input                    status_send_i,
	input                    loopback_mode_i,
	input                    tx_error_i,
	input                    rx_error_i,
	input                    tx_fifo_empty_i,
	input                    tx_fifo_full_i,
	input                    loopback_fifo_empty_i,
	input                    loopback_fifo_full_i,
	output                   source_sel_o,
	output                   source_empty_o,
	output [DATA_LEN-1:0]    status_word_o,
	output                   source_change_o
);
	localparam STATUS_BITS_LEN = 7;

	reg queued_ff;
	reg pending_ff;
	reg inflight_ff;
	reg tx_started_ff;
	reg source_sel_p1_ff;
	reg [STATUS_BITS_LEN-1:0] status_bits_ff;

	wire source_sel;

	assign source_sel = pending_ff || inflight_ff;

	always @(posedge clk) begin
		if (!rst_n) begin
			queued_ff <= 1'b0;
			pending_ff <= 1'b0;
			inflight_ff <= 1'b0;
			tx_started_ff <= 1'b0;
			source_sel_p1_ff <= 1'b0;
			status_bits_ff <= {STATUS_BITS_LEN{1'b0}};
		end
		else if (soft_clear_i) begin
			queued_ff <= 1'b0;
			pending_ff <= 1'b0;
			inflight_ff <= 1'b0;
			tx_started_ff <= 1'b0;
			source_sel_p1_ff <= 1'b0;
			status_bits_ff <= {STATUS_BITS_LEN{1'b0}};
		end
		else begin
			source_sel_p1_ff <= source_sel;

			if (status_req_i && !queued_ff && !pending_ff && !inflight_ff) begin
				queued_ff <= 1'b1;
				status_bits_ff <= {
				                   loopback_fifo_full_i,
				                   loopback_fifo_empty_i,
				                   tx_fifo_full_i,
				                   tx_fifo_empty_i,
				                   rx_error_i,
				                   tx_error_i,
				                   loopback_mode_i
				                  };
			end

			if (queued_ff && ft_idle_i && !service_hold_i) begin
				queued_ff <= 1'b0;
				pending_ff <= 1'b1;
			end

			if (pending_ff && status_pop_i) begin
				pending_ff <= 1'b0;
				inflight_ff <= 1'b1;
				tx_started_ff <= 1'b0;
			end

			if (inflight_ff && (status_send_i || !ft_idle_i))
				tx_started_ff <= 1'b1;

			if (inflight_ff && tx_started_ff && ft_idle_i) begin
				inflight_ff <= 1'b0;
				tx_started_ff <= 1'b0;
			end
		end
	end

	assign source_sel_o = source_sel;
	assign source_empty_o = !pending_ff;
	// Protocol defines bits [31:7] as zero.
	assign status_word_o = {{(DATA_LEN-STATUS_BITS_LEN){1'b0}}, status_bits_ff};
	assign source_change_o = source_sel ^ source_sel_p1_ff;

endmodule
