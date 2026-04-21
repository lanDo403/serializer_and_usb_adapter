`timescale 1ns / 1ps

module host_cmd_ctrl #(
	parameter DATA_LEN = 32,
	parameter BE_LEN = 4,
	parameter FIFO_RX_LEN = DATA_LEN + BE_LEN,
	parameter [BE_LEN-1:0] CMD_BE = 4'hE
)(
	input 						clk,
	input 						rst_n,
	input 						rx_word_valid_i,
	input [FIFO_RX_LEN-1:0] rx_word_i,
	input                   ft_idle_i,
	input 						tx_fifo_underflow_i,
	input 						rx_fifo_overflow_i,
	input 						rx_fifo_underflow_i,
	output 						tx_fifo_error_o,
	output 						rx_fifo_error_o,
	output 						soft_clear_tx_o,
	output 						soft_clear_rx_o,
	output 						soft_clear_ft_state_o,
	output                  status_req_o,
	output                  service_hold_o,
	output 						loopback_mode_o
    );

	localparam [DATA_LEN-1:0] CMD_CLR_TX_ERROR = 32'h00000001;
	localparam [DATA_LEN-1:0] CMD_CLR_RX_ERROR = 32'h00000002;
	localparam [DATA_LEN-1:0] CMD_CLR_ALL_ERROR = 32'h00000003;
	localparam [DATA_LEN-1:0] CMD_SET_LOOPBACK = 32'hA5A50004;
	localparam [DATA_LEN-1:0] CMD_SET_NORMAL = 32'hA5A50005;
	localparam [DATA_LEN-1:0] CMD_GET_STATUS = 32'hA5A50006;
	localparam [1:0] MODE_IDLE = 2'b00;
	localparam [1:0] MODE_WAIT_IDLE = 2'b01;
	localparam [1:0] MODE_CLEAR = 2'b10;
	localparam [1:0] MODE_COMMIT = 2'b11;

	reg tx_fifo_error_ff;
	reg rx_fifo_error_ff;
	reg loopback_mode_ff;
	reg cmd_burst_ff;
	reg [1:0] tx_recover_pipe_ff;
	reg [1:0] rx_recover_pipe_ff;
	reg [1:0] mode_state_ff;
	reg mode_target_ff;

	wire control_beat;
	wire cmd_valid;
	wire set_loopback_cmd;
	wire set_normal_cmd;
	wire get_status_cmd;
	wire mode_switch_cmd;
	wire clear_tx_cmd;
	wire clear_rx_cmd;
	wire tx_recover_active;
	wire rx_recover_active;
	wire mode_switch_active;

	assign control_beat = (rx_word_i[FIFO_RX_LEN-1:DATA_LEN] == CMD_BE);
	assign mode_switch_active = (mode_state_ff != MODE_IDLE);
	assign cmd_valid = rx_word_valid_i && control_beat && !cmd_burst_ff && !mode_switch_active;
	assign set_loopback_cmd = cmd_valid && (rx_word_i[DATA_LEN-1:0] == CMD_SET_LOOPBACK);
	assign set_normal_cmd = cmd_valid && (rx_word_i[DATA_LEN-1:0] == CMD_SET_NORMAL);
	assign get_status_cmd = cmd_valid && (rx_word_i[DATA_LEN-1:0] == CMD_GET_STATUS);
	assign mode_switch_cmd = (set_loopback_cmd && !loopback_mode_ff) ||
	                         (set_normal_cmd && loopback_mode_ff);
	assign clear_tx_cmd = cmd_valid && ((rx_word_i[DATA_LEN-1:0] == CMD_CLR_TX_ERROR) ||
	                                   (rx_word_i[DATA_LEN-1:0] == CMD_CLR_ALL_ERROR));
	assign clear_rx_cmd = cmd_valid && ((rx_word_i[DATA_LEN-1:0] == CMD_CLR_RX_ERROR) ||
	                                   (rx_word_i[DATA_LEN-1:0] == CMD_CLR_ALL_ERROR));
	assign tx_recover_active = |tx_recover_pipe_ff;
	assign rx_recover_active = |rx_recover_pipe_ff;
	assign tx_fifo_error_o = tx_fifo_error_ff;
	assign rx_fifo_error_o = rx_fifo_error_ff;
	assign soft_clear_tx_o = tx_recover_pipe_ff[1];
	assign soft_clear_rx_o = rx_recover_pipe_ff[1];
	assign soft_clear_ft_state_o = tx_recover_pipe_ff[1] || rx_recover_pipe_ff[1];
	assign status_req_o = get_status_cmd;
	assign service_hold_o = mode_switch_active;
	assign loopback_mode_o = loopback_mode_ff;

	always @(posedge clk) begin
		if (!rst_n) begin
			tx_fifo_error_ff <= 1'b0;
			rx_fifo_error_ff <= 1'b0;
			loopback_mode_ff <= 1'b0;
			cmd_burst_ff <= 1'b0;
			tx_recover_pipe_ff <= 2'b00;
			rx_recover_pipe_ff <= 2'b00;
			mode_state_ff <= MODE_IDLE;
			mode_target_ff <= 1'b0;
		end
		else begin
			cmd_burst_ff <= rx_word_valid_i && control_beat;
			tx_recover_pipe_ff <= {1'b0, tx_recover_pipe_ff[1]};
			rx_recover_pipe_ff <= {1'b0, rx_recover_pipe_ff[1]};

			if (tx_recover_active || clear_tx_cmd || mode_switch_active || mode_switch_cmd)
				tx_fifo_error_ff <= 1'b0;
			else if (tx_fifo_underflow_i)
				tx_fifo_error_ff <= 1'b1;
			if (rx_recover_active || clear_rx_cmd || mode_switch_active || mode_switch_cmd)
				rx_fifo_error_ff <= 1'b0;
			else if (rx_fifo_overflow_i || rx_fifo_underflow_i)
				rx_fifo_error_ff <= 1'b1;
			if (clear_tx_cmd)
				tx_recover_pipe_ff <= 2'b11;
			if (clear_rx_cmd)
				rx_recover_pipe_ff <= 2'b11;

			case (mode_state_ff)
				MODE_IDLE: begin
					if (mode_switch_cmd) begin
						mode_target_ff <= set_loopback_cmd;
						mode_state_ff <= MODE_WAIT_IDLE;
					end
				end

				MODE_WAIT_IDLE: begin
					if (ft_idle_i) begin
						tx_recover_pipe_ff <= 2'b11;
						rx_recover_pipe_ff <= 2'b11;
						mode_state_ff <= MODE_CLEAR;
					end
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

			if (cmd_valid) begin
				case (rx_word_i[DATA_LEN-1:0])
					CMD_CLR_TX_ERROR: ;
					CMD_CLR_RX_ERROR: ;
					CMD_CLR_ALL_ERROR: ;
					CMD_SET_NORMAL: ;
					CMD_GET_STATUS: ;
					CMD_SET_LOOPBACK: ;
				endcase
			end
		end
	end

endmodule
