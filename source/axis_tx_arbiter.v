`timescale 1ns / 1ps

module axis_tx_arbiter #(
	parameter DATA_LEN = 32,
	parameter BE_LEN   = 4
)(
	input                    status_lock_i,
	input                    loopback_sel_i,

	input                    s_normal_valid_i,
	output                   s_normal_ready_o,
	input  [DATA_LEN-1:0]    s_normal_data_i,
	input  [BE_LEN-1:0]      s_normal_keep_i,

	input                    s_loopback_valid_i,
	output                   s_loopback_ready_o,
	input  [DATA_LEN-1:0]    s_loopback_data_i,
	input  [BE_LEN-1:0]      s_loopback_keep_i,

	input                    s_status_valid_i,
	output                   s_status_ready_o,
	input  [DATA_LEN-1:0]    s_status_data_i,
	input  [BE_LEN-1:0]      s_status_keep_i,

	output                   m_valid_o,
	input                    m_ready_i,
	output [DATA_LEN-1:0]    m_data_o,
	output [BE_LEN-1:0]      m_keep_o
);

	wire status_sel_w;
	wire loopback_sel_w;
	wire normal_sel_w;

	assign status_sel_w = status_lock_i;
	assign loopback_sel_w = !status_sel_w && loopback_sel_i;
	assign normal_sel_w = !status_sel_w && !loopback_sel_i;

	assign s_status_ready_o = m_ready_i && status_sel_w;
	assign s_loopback_ready_o = m_ready_i && loopback_sel_w;
	assign s_normal_ready_o = m_ready_i && normal_sel_w;

	assign m_valid_o = status_sel_w ? s_status_valid_i :
	                   loopback_sel_w ? s_loopback_valid_i :
	                   s_normal_valid_i;
	assign m_data_o = status_sel_w ? s_status_data_i :
	                  loopback_sel_w ? s_loopback_data_i :
	                  s_normal_data_i;
	assign m_keep_o = status_sel_w ? s_status_keep_i :
	                  loopback_sel_w ? s_loopback_keep_i :
	                  s_normal_keep_i;

endmodule
