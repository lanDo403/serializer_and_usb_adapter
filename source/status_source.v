`timescale 1ns / 1ps

module status_source #(
	parameter DATA_LEN = 32
)(
	input                    clk,
	input                    rst_n,
	input                    clear_i,
	input                    hold_i,
	input                    issue_ready_i,
	input                    req_i,
	input                    m_ready_i,
	input                    send_fire_i,
	input                    loopback_mode_i,
	input                    tx_error_i,
	input                    rx_error_i,
	input                    tx_fifo_empty_i,
	input                    tx_fifo_full_i,
	input                    loopback_fifo_empty_i,
	input                    loopback_fifo_full_i,
	output                   m_active_o,
	output                   m_empty_o,
	output [DATA_LEN-1:0]    m_data_o
);
	localparam STATUS_BITS_LEN = 7;
	localparam [DATA_LEN-1:0] STATUS_MAGIC = 32'h5AA55AA5;

	reg queued_ff;
	reg active_ff;
	reg tx_started_ff;
	reg word_is_header_ff;
	reg advance_word_ff;
	reg [1:0] pending_words_ff;
	reg [STATUS_BITS_LEN-1:0] status_bits_ff;

	wire source_sel;
	wire [DATA_LEN-1:0] status_word_mux;
	wire issue_now_w;
	wire activate_now_w;

	assign source_sel = active_ff;
	assign status_word_mux = word_is_header_ff ? STATUS_MAGIC :
	                         {{(DATA_LEN-STATUS_BITS_LEN){1'b0}}, status_bits_ff};
	assign issue_now_w = req_i && !queued_ff && !active_ff && issue_ready_i && !hold_i;
	assign activate_now_w = issue_now_w || (queued_ff && issue_ready_i && !hold_i);

	// Capture a snapshot of status bits and queue a response request.
	always @(posedge clk) begin
		if (!rst_n) begin
			queued_ff <= 1'b0;
			status_bits_ff <= {STATUS_BITS_LEN{1'b0}};
		end
		else if (clear_i) begin
			queued_ff <= 1'b0;
			status_bits_ff <= {STATUS_BITS_LEN{1'b0}};
		end
		else begin
			if (req_i && !queued_ff && !active_ff) begin
				queued_ff <= !issue_now_w;
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
			else if (activate_now_w) begin
				queued_ff <= 1'b0;
			end
		end
	end

	// Drive the two-word status frame once the TX side reaches a safe issue window.
	always @(posedge clk) begin
		if (!rst_n) begin
			active_ff <= 1'b0;
			tx_started_ff <= 1'b0;
			word_is_header_ff <= 1'b0;
			advance_word_ff <= 1'b0;
			pending_words_ff <= 2'b00;
		end
		else if (clear_i) begin
			active_ff <= 1'b0;
			tx_started_ff <= 1'b0;
			word_is_header_ff <= 1'b0;
			advance_word_ff <= 1'b0;
			pending_words_ff <= 2'b00;
		end
		else begin
			if (advance_word_ff && word_is_header_ff)
				word_is_header_ff <= 1'b0;
			advance_word_ff <= 1'b0;

			if (activate_now_w) begin
				active_ff <= 1'b1;
				word_is_header_ff <= 1'b1;
				advance_word_ff <= 1'b0;
				pending_words_ff <= 2'd2;
				tx_started_ff <= 1'b0;
			end

			if (active_ff && m_ready_i && (pending_words_ff != 2'd0)) begin
				pending_words_ff <= pending_words_ff - 1'b1;
				advance_word_ff <= 1'b1;
			end

			if (active_ff && (send_fire_i || !issue_ready_i))
				tx_started_ff <= 1'b1;

			if (active_ff && tx_started_ff && issue_ready_i && (pending_words_ff == 2'd0)) begin
				active_ff <= 1'b0;
				tx_started_ff <= 1'b0;
				word_is_header_ff <= 1'b0;
				advance_word_ff <= 1'b0;
				pending_words_ff <= 2'b00;
			end
		end
	end

	assign m_active_o = source_sel;
	assign m_empty_o = (pending_words_ff == 2'd0);
	assign m_data_o = status_word_mux;

endmodule
