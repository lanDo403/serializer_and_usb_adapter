`timescale 1ns / 1ps

module rx_stream_router #(
	parameter DATA_LEN = 32,
	parameter BE_LEN   = 4,
	parameter FIFO_RX_LEN = DATA_LEN + BE_LEN
)(
	input                     clk,
	input                     rst_n,
	input                     clear_i,
	input                     hold_i,
	input                     loopback_mode_i,
	input                     rxf_n_i,

	input                     s_valid_i,
	output                    s_ready_o,
	input  [DATA_LEN-1:0]     s_data_i,
	input  [BE_LEN-1:0]       s_keep_i,

	output                    cmd_valid_o,
	output [DATA_LEN-1:0]     cmd_opcode_o,

	output                    m_payload_valid_o,
	input                     m_payload_ready_i,
	output [DATA_LEN-1:0]     m_payload_data_o,
	output [BE_LEN-1:0]       m_payload_keep_o,

	output                    prefetch_en_o,
	output                    source_flush_o
);

	localparam [DATA_LEN-1:0] CMD_MAGIC = 32'hA55A5AA5;

	reg loopback_mode_p1_ff;
	reg loopback_payload_en_ff;
	reg loopback_rx_idle_seen_ff;
	reg service_wait_opcode_ff;
	reg service_settle_ff;
	reg cmd_valid_ff;
	reg [DATA_LEN-1:0] cmd_opcode_ff;
	reg payload_valid_ff;
	reg [DATA_LEN-1:0] payload_data_ff;
	reg [BE_LEN-1:0] payload_keep_ff;

	wire s_full_word_w;
	wire s_has_keep_w;
	wire cmd_magic_w;
	wire payload_ready_w;
	wire payload_gate_w;
	wire payload_word_w;
	wire s_fire_w;
	wire loopback_rx_busy_w;

	assign s_full_word_w = (s_keep_i == {BE_LEN{1'b1}});
	assign s_has_keep_w = |s_keep_i;
	assign cmd_magic_w = s_full_word_w && (s_data_i == CMD_MAGIC);
	assign payload_ready_w = !payload_valid_ff || m_payload_ready_i;
	assign payload_gate_w = loopback_payload_en_ff ||
	                        (loopback_mode_i && loopback_rx_idle_seen_ff && !rxf_n_i);
	assign payload_word_w = !service_wait_opcode_ff && loopback_mode_i &&
	                        payload_gate_w && !cmd_magic_w;

	assign s_ready_o = !hold_i && (!loopback_mode_i || payload_ready_w || service_wait_opcode_ff);
	assign s_fire_w = s_valid_i && s_ready_o && s_has_keep_w;

	always @(posedge clk) begin
		if (!rst_n) begin
			loopback_mode_p1_ff <= 1'b0;
			loopback_payload_en_ff <= 1'b0;
			loopback_rx_idle_seen_ff <= 1'b0;
		end
		else if (clear_i) begin
			loopback_mode_p1_ff <= loopback_mode_i;
			loopback_payload_en_ff <= 1'b0;
			loopback_rx_idle_seen_ff <= 1'b0;
		end
		else begin
			loopback_mode_p1_ff <= loopback_mode_i;
			if (!loopback_mode_i) begin
				loopback_payload_en_ff <= 1'b0;
				loopback_rx_idle_seen_ff <= 1'b0;
			end
			else if (s_fire_w && cmd_magic_w) begin
				loopback_payload_en_ff <= 1'b0;
				loopback_rx_idle_seen_ff <= 1'b0;
			end
			else begin
				if (rxf_n_i)
					loopback_rx_idle_seen_ff <= 1'b1;
				if (s_fire_w && payload_word_w)
					loopback_payload_en_ff <= 1'b1;
			end
		end
	end

	// Service parser consumes CMD_MAGIC and the next full word as opcode.
	always @(posedge clk) begin
		if (!rst_n) begin
			service_wait_opcode_ff <= 1'b0;
			service_settle_ff <= 1'b0;
			cmd_valid_ff <= 1'b0;
			cmd_opcode_ff <= {DATA_LEN{1'b0}};
		end
		else if (clear_i) begin
			service_wait_opcode_ff <= 1'b0;
			service_settle_ff <= 1'b0;
			cmd_valid_ff <= 1'b0;
			cmd_opcode_ff <= {DATA_LEN{1'b0}};
		end
		else begin
			cmd_valid_ff <= 1'b0;

			if (service_wait_opcode_ff && service_settle_ff) begin
				service_settle_ff <= 1'b0;
			end
			else if (s_fire_w && service_wait_opcode_ff) begin
				service_wait_opcode_ff <= 1'b0;
				cmd_valid_ff <= 1'b1;
				cmd_opcode_ff <= s_data_i;
			end
			else if (s_fire_w && cmd_magic_w) begin
				service_wait_opcode_ff <= 1'b1;
				service_settle_ff <= 1'b1;
			end
		end
	end

	// One-word payload buffer decouples RX sampling from loopback FIFO backpressure.
	always @(posedge clk) begin
		if (!rst_n) begin
			payload_valid_ff <= 1'b0;
			payload_data_ff <= {DATA_LEN{1'b0}};
			payload_keep_ff <= {BE_LEN{1'b0}};
		end
		else if (clear_i) begin
			payload_valid_ff <= 1'b0;
			payload_data_ff <= {DATA_LEN{1'b0}};
			payload_keep_ff <= {BE_LEN{1'b0}};
		end
		else begin
			if (payload_valid_ff && m_payload_ready_i)
				payload_valid_ff <= 1'b0;

			if (s_fire_w && payload_word_w) begin
				payload_valid_ff <= 1'b1;
				payload_data_ff <= s_data_i;
				payload_keep_ff <= s_keep_i;
			end
		end
	end

	assign cmd_valid_o = cmd_valid_ff;
	assign cmd_opcode_o = cmd_opcode_ff;
	assign m_payload_valid_o = payload_valid_ff;
	assign m_payload_data_o = payload_data_ff;
	assign m_payload_keep_o = payload_keep_ff;
	assign loopback_rx_busy_w = !rxf_n_i || s_valid_i || service_wait_opcode_ff ||
	                            cmd_valid_ff || payload_valid_ff;
	assign prefetch_en_o = !loopback_mode_i || !loopback_rx_busy_w;
	assign source_flush_o = !hold_i && (loopback_mode_i ^ loopback_mode_p1_ff);

endmodule
