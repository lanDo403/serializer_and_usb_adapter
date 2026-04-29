`timescale 1ns / 1ps

module rx_stream_router #(
	parameter FIFO_RX_LEN = 36,
	parameter BE_LEN = 4
)(
	input                     clk,
	input                     rst_n,
	input                     clear_i,
	input                     hold_i,
	input                     loopback_mode_i,
	input                     rxf_n_i,
	input                     s_valid_i,
	input  [FIFO_RX_LEN-1:0]  s_word_i,
	output                    m_cmd_valid_o,
	output [FIFO_RX_LEN-1:0]  m_cmd_word_o,
	output                    m_loopback_valid_o,
	output [FIFO_RX_LEN-1:0]  m_loopback_word_o,
	output                    prefetch_en_o,
	output                    source_flush_o
);

	reg loopback_mode_p1_ff;
	reg loopback_payload_en_ff;
	reg loopback_rx_idle_seen_ff;
	reg service_wait_opcode_ff;
	reg service_settle_ff;
	reg cmd_valid_ff;
	reg [FIFO_RX_LEN-1:0] cmd_word_ff;
	reg loopback_valid_ff;

	wire s_full_word_w;
	wire s_has_keep_w;
	wire cmd_magic_w;
	wire s_fire_w;
	wire service_word_w;
	wire loopback_rx_busy;

	localparam [FIFO_RX_LEN-BE_LEN-1:0] CMD_MAGIC = 32'hA55A5AA5;

	assign s_full_word_w = (s_word_i[FIFO_RX_LEN-1:FIFO_RX_LEN-BE_LEN] == {BE_LEN{1'b1}});
	assign s_has_keep_w = |s_word_i[FIFO_RX_LEN-1:FIFO_RX_LEN-BE_LEN];
	assign cmd_magic_w = s_full_word_w && (s_word_i[FIFO_RX_LEN-BE_LEN-1:0] == CMD_MAGIC);
	assign s_fire_w = s_valid_i && s_has_keep_w;
	assign service_word_w = s_fire_w && (
	                        (!service_wait_opcode_ff && cmd_magic_w) ||
	                        ( service_wait_opcode_ff && s_full_word_w)
	                       );

	// Keep the previous loopback mode to detect the first RX phase after entering loopback.
	always @(posedge clk) begin
		if (!rst_n) begin
			loopback_mode_p1_ff <= 1'b0;
		end
		else if (clear_i) begin
			loopback_mode_p1_ff <= loopback_mode_i;
		end
		else
			loopback_mode_p1_ff <= loopback_mode_i;
	end

	// Capture RX words after FIFO append so command decode and loopback buffering use the same registered sample.
	always @(posedge clk) begin
		if (!rst_n) begin
			cmd_valid_ff <= 1'b0;
			cmd_word_ff <= {FIFO_RX_LEN{1'b0}};
			loopback_valid_ff <= 1'b0;
		end
		else if (clear_i) begin
			cmd_valid_ff <= 1'b0;
			cmd_word_ff <= {FIFO_RX_LEN{1'b0}};
			loopback_valid_ff <= 1'b0;
		end
		else begin
			cmd_valid_ff <= s_fire_w && !hold_i;
			loopback_valid_ff <= s_fire_w && loopback_payload_en_ff &&
			                     !service_word_w && !hold_i;

			if (!hold_i && s_fire_w) begin
				cmd_word_ff <= s_word_i;
			end

		end
	end

	// Enable loopback payload storage only after the initial command frame has been consumed.
	always @(posedge clk) begin
		if (!rst_n) begin
			loopback_payload_en_ff <= 1'b0;
			loopback_rx_idle_seen_ff <= 1'b0;
		end
		else if (clear_i) begin
			loopback_payload_en_ff <= 1'b0;
			loopback_rx_idle_seen_ff <= 1'b0;
		end
		else begin
			if (!loopback_mode_i) begin
				loopback_payload_en_ff <= 1'b0;
				loopback_rx_idle_seen_ff <= 1'b0;
			end
			else if (!hold_i) begin
				if (rxf_n_i)
					loopback_rx_idle_seen_ff <= 1'b1;

				if (s_fire_w && cmd_magic_w) begin
					loopback_payload_en_ff <= 1'b0;
					loopback_rx_idle_seen_ff <= 1'b0;
				end
				else if (loopback_mode_p1_ff && loopback_rx_idle_seen_ff && !rxf_n_i)
					loopback_payload_en_ff <= 1'b1;
			end
		end
	end

	// Service-frame parser consumes CMD_MAGIC first and the opcode on the next full RX word.
	always @(posedge clk) begin
		if (!rst_n) begin
			service_wait_opcode_ff <= 1'b0;
			service_settle_ff <= 1'b0;
		end
		else if (clear_i) begin
			service_wait_opcode_ff <= 1'b0;
			service_settle_ff <= 1'b0;
		end
		else begin
			if (!loopback_mode_i) begin
				service_wait_opcode_ff <= 1'b0;
				service_settle_ff <= 1'b0;
			end
			else if (!hold_i && service_wait_opcode_ff) begin
				if (service_settle_ff)
					service_settle_ff <= 1'b0;
				else if (s_fire_w && s_full_word_w)
					service_wait_opcode_ff <= 1'b0;
			end
			else if (!hold_i && s_fire_w) begin
				if (cmd_magic_w) begin
					service_wait_opcode_ff <= 1'b1;
					service_settle_ff <= 1'b1;
				end
			end
		end
	end

	assign m_cmd_valid_o      = cmd_valid_ff;
	assign m_cmd_word_o       = cmd_word_ff;
	assign m_loopback_valid_o = loopback_valid_ff;
	assign m_loopback_word_o  = s_word_i;
	assign loopback_rx_busy   = !rxf_n_i || s_valid_i || cmd_valid_ff || loopback_valid_ff;
	// ft601_fsm already blocks fifo_pop during hold_i; do not duplicate that gate here.
	assign prefetch_en_o      = !loopback_mode_i || !loopback_rx_busy;
	assign source_flush_o     = !hold_i && (loopback_mode_i ^ loopback_mode_p1_ff);

endmodule
