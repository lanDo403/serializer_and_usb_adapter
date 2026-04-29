`timescale 1ns / 1ps

module service_cmd_decoder #(
	parameter DATA_LEN = 32,
	parameter BE_LEN = 4,
	parameter FIFO_RX_LEN = DATA_LEN + BE_LEN
)(
	input 						clk,
	input 						rst_n,
	input 						s_valid_i,
	input [FIFO_RX_LEN-1:0] s_word_i,
	input                   idle_i,
	input 						normal_rd_underflow_i,
	input                   normal_wr_error_i,
	input 						loopback_wr_overflow_i,
	input 						loopback_rd_underflow_i,
	output 						tx_error_o,
	output 						rx_error_o,
	output 						tx_fifo_clear_o,
	output 						loopback_fifo_clear_o,
	output 						state_clear_o,
	output                  tx_flush_o,
	output                  status_req_o,
	output                  hold_o,
	output 						loopback_mode_o
    );

	localparam [DATA_LEN-1:0] CMD_CLR_TX_ERROR = 32'h00000001;
	localparam [DATA_LEN-1:0] CMD_CLR_RX_ERROR = 32'h00000002;
	localparam [DATA_LEN-1:0] CMD_CLR_ALL_ERROR = 32'h00000003;
	localparam [DATA_LEN-1:0] CMD_SET_LOOPBACK = 32'hA5A50004;
	localparam [DATA_LEN-1:0] CMD_SET_NORMAL = 32'hA5A50005;
	localparam [DATA_LEN-1:0] CMD_GET_STATUS = 32'hA5A50006;
	localparam [DATA_LEN-1:0] CMD_RESET_FT_STATE = 32'hA5A50007;
	localparam [DATA_LEN-1:0] CMD_MAGIC = 32'hA55A5AA5;
	localparam [1:0] MODE_IDLE = 2'b00;
	localparam [1:0] MODE_WAIT_IDLE = 2'b01;
	localparam [1:0] MODE_CLEAR = 2'b10;
	localparam [1:0] MODE_COMMIT = 2'b11;

	reg tx_fifo_error_ff;
	reg rx_fifo_error_ff;
	reg loopback_mode_ff;
	reg s_valid_ff;
	reg [FIFO_RX_LEN-1:0] s_word_ff;
	reg wait_opcode_ff;
	reg parser_settle_ff;
	reg opcode_valid_ff;
	reg [DATA_LEN-1:0] opcode_ff;
	reg tx_error_arm_ff;
	reg [1:0] tx_clear_pipe_ff;
	reg [1:0] rx_clear_pipe_ff;
	reg [1:0] ft_state_clear_pipe_ff;
	reg tx_flush_ff;
	reg [1:0] mode_state_ff;
	reg mode_target_ff;

	wire full_word;
	wire cmd_magic;
	(* KEEP = "TRUE" *) wire cmd_valid;
	wire set_loopback_cmd;
	wire set_normal_cmd;
	wire get_status_cmd;
	wire reset_ft_state_cmd;
	wire opcode_valid_w;
	wire clear_all_cmd;
	wire mode_switch_cmd;
	wire clear_tx_cmd;
	wire clear_rx_cmd;
	wire tx_recover_active;
	wire rx_recover_active;
	wire mode_switch_active;
	wire tx_error_clear;
	wire rx_error_clear;
	wire mode_clear_start;

	assign full_word = (s_word_ff[FIFO_RX_LEN-1:DATA_LEN] == {BE_LEN{1'b1}});
	assign cmd_magic = s_valid_ff && full_word && (s_word_ff[DATA_LEN-1:0] == CMD_MAGIC);
	assign opcode_valid_w = opcode_valid_ff;
	assign mode_switch_active = (mode_state_ff != MODE_IDLE);
	assign cmd_valid = opcode_valid_w && !mode_switch_active && (
	                  (opcode_ff == CMD_CLR_TX_ERROR) ||
	                  (opcode_ff == CMD_CLR_RX_ERROR) ||
	                  (opcode_ff == CMD_CLR_ALL_ERROR) ||
	                  (opcode_ff == CMD_SET_LOOPBACK) ||
	                  (opcode_ff == CMD_SET_NORMAL) ||
	                  (opcode_ff == CMD_GET_STATUS) ||
	                  (opcode_ff == CMD_RESET_FT_STATE)
	                 );
	assign clear_all_cmd = cmd_valid && (opcode_ff == CMD_CLR_ALL_ERROR);
	assign set_loopback_cmd = cmd_valid && (opcode_ff == CMD_SET_LOOPBACK);
	assign set_normal_cmd = cmd_valid && (opcode_ff == CMD_SET_NORMAL);
	assign get_status_cmd = cmd_valid && (opcode_ff == CMD_GET_STATUS);
	assign reset_ft_state_cmd = cmd_valid && (opcode_ff == CMD_RESET_FT_STATE);
	assign mode_switch_cmd = (set_loopback_cmd && !loopback_mode_ff) ||
	                         (set_normal_cmd && loopback_mode_ff);
	assign clear_tx_cmd = (cmd_valid && (opcode_ff == CMD_CLR_TX_ERROR)) ||
	                      clear_all_cmd;
	assign clear_rx_cmd = (cmd_valid && (opcode_ff == CMD_CLR_RX_ERROR)) ||
	                      clear_all_cmd;
	assign tx_recover_active = |tx_clear_pipe_ff;
	assign rx_recover_active = |rx_clear_pipe_ff;
	assign tx_error_clear = tx_recover_active || clear_tx_cmd || mode_switch_active ||
	                        mode_switch_cmd || reset_ft_state_cmd || |ft_state_clear_pipe_ff;
	assign rx_error_clear = rx_recover_active || clear_rx_cmd || mode_switch_active ||
	                        mode_switch_cmd || reset_ft_state_cmd || |ft_state_clear_pipe_ff;
	assign mode_clear_start = (mode_state_ff == MODE_WAIT_IDLE) && idle_i;
	assign tx_error_o = tx_fifo_error_ff;
	assign rx_error_o = rx_fifo_error_ff;
	assign tx_fifo_clear_o = tx_clear_pipe_ff[1];
	assign loopback_fifo_clear_o = rx_clear_pipe_ff[1];
	assign state_clear_o = tx_clear_pipe_ff[1] || rx_clear_pipe_ff[1] ||
	                               ft_state_clear_pipe_ff[1];
	assign tx_flush_o = tx_flush_ff;
	assign status_req_o = get_status_cmd;
	assign hold_o = mode_switch_active;
	assign loopback_mode_o = loopback_mode_ff;

	// Decouple parser timing from the registered RX word producer.
	always @(posedge clk) begin
		if (!rst_n) begin
			s_valid_ff <= 1'b0;
			s_word_ff <= {FIFO_RX_LEN{1'b0}};
		end
		else begin
			s_valid_ff <= s_valid_i;
			s_word_ff <= s_word_i;
		end
	end

	// Parse FT601 service frames as two full words: CMD_MAGIC followed by opcode.
	always @(posedge clk) begin
		if (!rst_n) begin
			wait_opcode_ff <= 1'b0;
			parser_settle_ff <= 1'b0;
			opcode_valid_ff <= 1'b0;
			opcode_ff <= {DATA_LEN{1'b0}};
		end
		else begin
			opcode_valid_ff <= 1'b0;

			if (wait_opcode_ff) begin
				if (parser_settle_ff)
					parser_settle_ff <= 1'b0;
				else if (s_valid_ff && full_word) begin
					wait_opcode_ff <= 1'b0;
					opcode_valid_ff <= 1'b1;
					opcode_ff <= s_word_ff[DATA_LEN-1:0];
				end
			end
			else if (cmd_magic) begin
				wait_opcode_ff <= 1'b1;
				parser_settle_ff <= 1'b1;
			end
		end
	end

	// Generate two-cycle recovery pulses for TX and RX/local FT-domain state.
	always @(posedge clk) begin
		if (!rst_n) begin
			tx_clear_pipe_ff <= 2'b00;
			rx_clear_pipe_ff <= 2'b00;
			ft_state_clear_pipe_ff <= 2'b00;
			tx_flush_ff <= 1'b0;
		end
		else begin
			tx_flush_ff <= mode_switch_cmd;

			if (mode_clear_start || clear_tx_cmd)
				tx_clear_pipe_ff <= 2'b11;
			else
				tx_clear_pipe_ff <= {1'b0, tx_clear_pipe_ff[1]};

			if (mode_clear_start || clear_rx_cmd)
				rx_clear_pipe_ff <= 2'b11;
			else
				rx_clear_pipe_ff <= {1'b0, rx_clear_pipe_ff[1]};

			if (reset_ft_state_cmd)
				ft_state_clear_pipe_ff <= 2'b11;
			else
				ft_state_clear_pipe_ff <= {1'b0, ft_state_clear_pipe_ff[1]};
		end
	end

	// TX sticky error combines normal TX read-underflow events with the synchronized GPIO write-side fault.
	always @(posedge clk) begin
		if (!rst_n) begin
			tx_fifo_error_ff <= 1'b0;
			tx_error_arm_ff <= 1'b1;
		end
		else begin
			if (tx_error_clear) begin
				tx_fifo_error_ff <= 1'b0;
				tx_error_arm_ff <= 1'b0;
			end
			else begin
				if (!normal_wr_error_i)
					tx_error_arm_ff <= 1'b1;
				if (tx_error_arm_ff && (normal_rd_underflow_i || normal_wr_error_i))
					tx_fifo_error_ff <= 1'b1;
			end
		end
	end

	// RX sticky error tracks loopback FIFO request/full/empty faults until explicit recovery.
	always @(posedge clk) begin
		if (!rst_n)
			rx_fifo_error_ff <= 1'b0;
		else begin
			if (rx_error_clear)
				rx_fifo_error_ff <= 1'b0;
			else if (loopback_wr_overflow_i || loopback_rd_underflow_i)
				rx_fifo_error_ff <= 1'b1;
		end
	end

	// Mode switch waits for FT idle, launches local recovery, then commits the new runtime mode.
	always @(posedge clk) begin
		if (!rst_n) begin
			loopback_mode_ff <= 1'b0;
			mode_state_ff <= MODE_IDLE;
			mode_target_ff <= 1'b0;
		end
		else begin
			if (reset_ft_state_cmd) begin
				loopback_mode_ff <= 1'b0;
				mode_state_ff <= MODE_IDLE;
				mode_target_ff <= 1'b0;
			end
			else begin
				case (mode_state_ff)
					MODE_IDLE: begin
						if (mode_switch_cmd) begin
							mode_target_ff <= set_loopback_cmd;
							mode_state_ff <= MODE_WAIT_IDLE;
						end
					end

					MODE_WAIT_IDLE: begin
						if (idle_i)
							mode_state_ff <= MODE_CLEAR;
					end

					MODE_CLEAR: begin
						if (!tx_recover_active && !rx_recover_active) begin
							loopback_mode_ff <= mode_target_ff;
							mode_state_ff <= MODE_COMMIT;
						end
					end

					MODE_COMMIT:
						mode_state_ff <= MODE_IDLE;
				endcase
			end
		end
	end

endmodule
