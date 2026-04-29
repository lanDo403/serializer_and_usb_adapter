`timescale 1ns / 1ps

module ft601_fsm #(
	parameter DATA_LEN = 32,
	parameter BE_LEN   = 4
)(
	input                    rst_n,
	input                    clk,
	input                    txe_n,
	input                    rxf_n,

	input                    tx_axis_tvalid_i,
	output                   tx_axis_tready_o,
	input  [DATA_LEN-1:0]    tx_axis_tdata_i,
	input  [BE_LEN-1:0]      tx_axis_tkeep_i,

	output                   rx_axis_tvalid_o,
	input                    rx_axis_tready_i,
	output [DATA_LEN-1:0]    rx_axis_tdata_o,
	output [BE_LEN-1:0]      rx_axis_tkeep_o,
	input  [DATA_LEN-1:0]    rx_data_i,
	input  [BE_LEN-1:0]      rx_keep_i,

	input                    tx_flush_i,
	input                    state_clear_i,
	input                    service_hold_i,
	input                    tx_prefetch_en_i,
	input                    tx_source_status_i,

	output [DATA_LEN-1:0]    tx_data_o,
	output [BE_LEN-1:0]      tx_keep_o,
	output                   wr_n,
	output                   rd_n,
	output                   oe_n,
	output                   drive_tx,
	output                   idle_o
);

	localparam ARB         = 5'b00001;
	localparam TX_PREFETCH = 5'b00010;
	localparam TX_BURST    = 5'b00100;
	localparam RX_START    = 5'b01000;
	localparam RX_BURST    = 5'b10000;

	reg [4:0] next_state;
	reg [4:0] state;

	wire rx_cond_w;
	wire rx_burst_keep_w;
	wire tx_data_valid_w;
	wire tx_idle_w;
	wire tx_req_w;
	wire tx_burst_w;
	wire tx_write_active_w;

	assign rx_cond_w = !rxf_n && rx_axis_tready_i;
	assign rx_burst_keep_w = !rxf_n && rx_axis_tready_i;
	assign tx_req_w = !txe_n && tx_data_valid_w;
	assign tx_burst_w = tx_req_w;
	assign tx_write_active_w = ((state == TX_PREFETCH) || (state == TX_BURST)) &&
	                           tx_req_w;

	always @(posedge clk) begin
		if (!rst_n)
			state <= ARB;
		else if (state_clear_i)
			state <= ARB;
		else
			state <= next_state;
	end

	always @(*) begin
		next_state = state;
		case (state)
			ARB:         next_state = (!service_hold_i && rx_cond_w) ? RX_START :
			                         tx_req_w ? TX_PREFETCH : ARB;
			TX_PREFETCH: next_state = tx_burst_w ? TX_BURST : TX_PREFETCH;
			TX_BURST:    next_state = tx_burst_w ? TX_BURST : ARB;
			RX_START:    next_state = RX_BURST;
			RX_BURST:    next_state = rx_burst_keep_w ? RX_BURST : ARB;
			default:     next_state = ARB;
		endcase
	end

	ft601_rx_adapter #(
		.DATA_LEN(DATA_LEN),
		.BE_LEN(BE_LEN)
	) rx_adapter (
		.clk_i(clk),
		.rst_n_i(rst_n),
		.clear_i(state_clear_i),
		.start_i(state == RX_START),
		.burst_i(state == RX_BURST),
		.rxf_n_i(rxf_n),
		.m_ready_i(rx_axis_tready_i),
		.bus_data_i(rx_data_i),
		.bus_keep_i(rx_keep_i),
		.m_data_o(rx_axis_tdata_o),
		.m_keep_o(rx_axis_tkeep_o),
		.m_valid_o(rx_axis_tvalid_o),
		.rd_n_o(rd_n),
		.oe_n_o(oe_n)
	);

	ft601_tx_adapter #(
		.DATA_LEN(DATA_LEN),
		.BE_LEN(BE_LEN)
	) tx_adapter (
		.clk_i(clk),
		.rst_n_i(rst_n),
		.txe_n_i(txe_n),
		.arb_phase_i(state == ARB),
		.prefetch_phase_i(state == TX_PREFETCH),
		.burst_phase_i(state == TX_BURST),
		.write_fire_i(tx_write_active_w),
		.flush_i(tx_flush_i),
		.clear_i(state_clear_i),
		.hold_i(service_hold_i),
		.prefetch_en_i(tx_prefetch_en_i),
		.status_source_i(tx_source_status_i),
		.s_valid_i(tx_axis_tvalid_i),
		.s_data_i(tx_axis_tdata_i),
		.s_keep_i(tx_axis_tkeep_i),
		.s_ready_o(tx_axis_tready_o),
		.word_valid_o(tx_data_valid_w),
		.idle_o(tx_idle_w),
		.bus_data_o(tx_data_o),
		.bus_keep_o(tx_keep_o),
		.wr_n_o(wr_n),
		.bus_drive_o(drive_tx)
	);

	assign idle_o = (state == ARB) && tx_idle_w && wr_n && rd_n && oe_n && !rx_axis_tvalid_o && !drive_tx;

endmodule
