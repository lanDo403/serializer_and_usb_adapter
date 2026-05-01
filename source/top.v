`timescale 1ns / 1ps

// This project is divided by two frequency domains. Write domain works on GPIO_CLK from input gpio pin (frequency is changeable). 
// Write domain includes modules such as gpio_wrapper, packer8to32, async_fifo(write side) and sram_dp(write side).
// Read domain works on CLK from FT601 (100MHZ). Read domain includes modules such as async_fifo(read side), sram_dp(read side), loopback and ft601_fsm.

module top #(
	parameter GPIO_LEN = 8,
	parameter DATA_LEN = 32,
	parameter BE_LEN = 4,
	parameter FIFO_DEPTH = 8192,
	parameter ADDR_LEN = $clog2(FIFO_DEPTH),
	parameter FIFO_RX_LEN = DATA_LEN + BE_LEN
)(
	// GPIO signals from FPGA logic
	input   						GPIO_CLK,
	input  [GPIO_LEN-1:0]	GPIO_DATA,
	input   						GPIO_STROB,	
	input 						FPGA_RESET,
   input 						CLK,		// Clock signal from FT601
	output 						RESET_N,	// Active-low reset signal driven to FT601
   input 						TXE_N,		// Trancieve empty signal from FT601
   input 						RXF_N,		// Receive full signal from FT601
   output 						OE_N,		// Output enable signal to FT601
   output 						WR_N,		// Write enable signal to FT601
   output 						RD_N,		// Read enable signal to FT601
	inout [BE_LEN-1:0] 		BE,			// In and out byte enable bus connected to FT601
	inout [DATA_LEN-1:0] 	DATA		// In and out data bus connected to FT601
	 );
	localparam [BE_LEN-1:0] FULL_BE = {BE_LEN{1'b1}};
	
	// Reset and clocks
	wire gpio_clk;
	wire ft_clk_i;
	wire fpga_reset_i;
	wire ft601_reset_n_i;
	wire gpio_rst_req;
	wire ft_rst_req;
	wire gpio_rst_n_i;
	wire ft_rst_n_i;

	// FT601 wrapper / FSM pins
	wire ft_txe_n_i;
	wire ft_rxf_n_i;
	wire [DATA_LEN-1:0] rx_data;
	wire [DATA_LEN-1:0] tx_data;
	wire [BE_LEN-1:0] rx_be;
	wire [BE_LEN-1:0] tx_be;
	wire fsm_oe_o;
	wire fsm_wr_o;
	wire fsm_rd_o;
	wire fsm_idle_o;
	wire drive_tx;

	// GPIO wrapper and packer
	wire [GPIO_LEN-1:0] gpio_data;
	wire gpio_strob;
	wire packer_valid_o;
	wire [DATA_LEN-1:0] packer_data_o;
	wire packer_wen_i;

	// Normal TX async FIFO and SRAM
	wire [DATA_LEN-1:0] normal_fifo_wdata;
	wire [DATA_LEN-1:0] normal_fifo_rdata;
	wire [ADDR_LEN-1:0] normal_fifo_waddr;
	wire [ADDR_LEN-1:0] normal_fifo_raddr;
	wire [DATA_LEN-1:0] sram_in;
	wire [DATA_LEN-1:0] sram_out;
	wire normal_fifo_full;
	wire normal_fifo_empty;
	wire normal_fifo_wen;
	wire normal_fifo_ren;
	wire tx_fifo_full_ft;
	wire tx_fifo_clear_ft;
	wire tx_fifo_clear_gpio;

	// Loopback FIFO
	wire [FIFO_RX_LEN-1:0] loop_fifo_wdata;
	wire [FIFO_RX_LEN-1:0] loop_fifo_rdata;
	wire loop_fifo_full;
	wire loop_fifo_empty;
	wire loop_fifo_wen;
	wire loop_fifo_ren;

	// RX stream router
	wire ft_rx_axis_tvalid;
	wire ft_rx_axis_tready;
	wire [DATA_LEN-1:0] ft_rx_axis_tdata;
	wire [BE_LEN-1:0] ft_rx_axis_tkeep;
	wire rx_cmd_valid;
	wire [DATA_LEN-1:0] rx_cmd_opcode;
	wire loopback_payload_tvalid;
	wire loopback_payload_tready;
	wire [DATA_LEN-1:0] loopback_payload_tdata;
	wire [BE_LEN-1:0] loopback_payload_tkeep;
	wire tx_prefetch_en;
	wire tx_source_change;

	// Service command decoder and diagnostics
	wire tx_error;
	wire rx_error;
	wire tx_fifo_error_wr_gpio;
	wire tx_fifo_error_wr_ft;
	wire tx_fifo_rd_underflow_event;
	wire loopback_wr_overflow_event;
	wire loopback_rd_underflow_event;
	wire loopback_mode_ft;
	wire loopback_mode_gpio;
	wire service_hold_ft;
	wire service_hold_gpio;
	wire tx_recovery_clear_req_ft;
	wire rx_recovery_clear_req_ft;
	wire ft_state_clear_req_ft;
	wire tx_prefetch_flush_req_ft;
	wire status_req;

	// Status source
	wire status_source_sel;
	wire status_issue_window;

	// Local stream contracts between payload/status sources, the TX arbiter and
	// FT601 adapters. This is a small valid/ready layer, not a full AXI fabric.
	wire normal_axis_tvalid;
	wire normal_axis_tready;
	wire [DATA_LEN-1:0] normal_axis_tdata;
	wire [BE_LEN-1:0] normal_axis_tkeep;
	wire loopback_axis_tvalid;
	wire loopback_axis_tready;
	wire [DATA_LEN-1:0] loopback_axis_tdata;
	wire [BE_LEN-1:0] loopback_axis_tkeep;
	wire [FIFO_RX_LEN-1:0] loopback_axis_word;
	wire status_axis_tvalid;
	wire status_axis_tready;
	wire [DATA_LEN-1:0] status_axis_tdata;
	wire [BE_LEN-1:0] status_axis_tkeep;
	wire tx_axis_tvalid;
	wire tx_axis_tready;
	wire [DATA_LEN-1:0] tx_axis_tdata;
	wire [BE_LEN-1:0] tx_axis_tkeep;

	// Assignings
	assign normal_fifo_wdata = packer_data_o;

	assign normal_axis_tkeep  = FULL_BE;

	assign loopback_axis_tdata  = loopback_axis_word[DATA_LEN-1:0];
	assign loopback_axis_tkeep  = loopback_axis_word[FIFO_RX_LEN-1:DATA_LEN];

	assign tx_fifo_rd_underflow_event = normal_fifo_ren && normal_fifo_empty;
	assign loopback_payload_tready = !loop_fifo_full;
	assign loop_fifo_wen = loopback_payload_tvalid && loopback_payload_tready;
	assign loop_fifo_wdata = {loopback_payload_tkeep, loopback_payload_tdata};
	assign loopback_wr_overflow_event = loopback_payload_tvalid && !loopback_payload_tready;
	assign loopback_rd_underflow_event = loopback_axis_tvalid && loopback_axis_tready && loop_fifo_empty;
	assign status_issue_window = fsm_idle_o;
	assign ft601_reset_n_i = ~fpga_reset_i;
	assign gpio_rst_req = fpga_reset_i;
	assign ft_rst_req = fpga_reset_i;
	assign tx_fifo_clear_ft = tx_recovery_clear_req_ft;

	IBUF #(
		.IOSTANDARD("LVCMOS33")
	) ibuf_fpga_reset (
		.I(FPGA_RESET),
		.O(fpga_reset_i)
	);
	
	// FT601 physical I/O wrapper
	ft601_wrapper #(
		.DATA_LEN(DATA_LEN),
		.BE_LEN(BE_LEN)
	) ft601_wrapper (
		.CLK(CLK),
		.RESET_N(RESET_N),
		.TXE_N(TXE_N),
		.RXF_N(RXF_N),
		.OE_N(OE_N),
		.WR_N(WR_N),
		.RD_N(RD_N),
		.BE(BE),
		.DATA(DATA),
		.clk_o(ft_clk_i),
		.ft601_reset_n_i(ft601_reset_n_i),
		.core_rst_n_i(ft_rst_n_i),
		.txe_n_o(ft_txe_n_i),
		.rxf_n_o(ft_rxf_n_i),
		.rx_be_o(rx_be),
		.rx_data_o(rx_data),
		.oe_n_i(fsm_oe_o),
		.wr_n_i(fsm_wr_o),
		.rd_n_i(fsm_rd_o),
		.drive_tx_i(drive_tx),
		.tx_be_i(tx_be),
		.tx_data_i(tx_data)
	);

	// Synchronizers per clock domain
	rst_sync gpio_rst_sync (
		.clk(gpio_clk),
		.arst_i(gpio_rst_req),
		.rst_n_o(gpio_rst_n_i)
	);

	rst_sync ft_rst_sync (
		.clk(ft_clk_i),
		.arst_i(ft_rst_req),
		.rst_n_o(ft_rst_n_i)
	);

	bit_sync #(
		.RESET_VALUE(1'b0)
	) loopback_mode_gpio_sync (
		.clk(gpio_clk),
		.rst_n(gpio_rst_n_i),
		.din(loopback_mode_ft),
		.dout(loopback_mode_gpio)
	);

	bit_sync #(
		.RESET_VALUE(1'b0)
	) service_hold_gpio_sync (
		.clk(gpio_clk),
		.rst_n(gpio_rst_n_i),
		.din(service_hold_ft),
		.dout(service_hold_gpio)
	);

	bit_sync #(
		.RESET_VALUE(1'b0)
	) tx_fifo_full_ft_sync (
		.clk(ft_clk_i),
		.rst_n(ft_rst_n_i),
		.din(normal_fifo_full),
		.dout(tx_fifo_full_ft)
	);

	bit_sync #(
		.RESET_VALUE(1'b0)
	) tx_fifo_error_wr_ft_sync (
		.clk(ft_clk_i),
		.rst_n(ft_rst_n_i),
		.din(tx_fifo_error_wr_gpio),
		.dout(tx_fifo_error_wr_ft)
	);

	pulse_sync tx_fifo_clear_gpio_sync (
		.src_clk(ft_clk_i),
		.src_rst_n(ft_rst_n_i),
		.dst_clk(gpio_clk),
		.dst_rst_n(gpio_rst_n_i),
		.pulse_i(tx_recovery_clear_req_ft),
		.pulse_o(tx_fifo_clear_gpio)
	);

	sync_fifo_axis_source #(
		.DATA_LEN(DATA_LEN)
	) normal_tx_source (
		.clk(ft_clk_i),
		.rst_n(ft_rst_n_i),
		.clear_i(tx_recovery_clear_req_ft || tx_prefetch_flush_req_ft),
		.enable_i(!loopback_mode_ft && tx_prefetch_en),
		.fifo_data_i(normal_fifo_rdata),
		.fifo_empty_i(normal_fifo_empty),
		.fifo_ren_o(normal_fifo_ren),
		.m_valid_o(normal_axis_tvalid),
		.m_ready_i(normal_axis_tready),
		.m_data_o(normal_axis_tdata)
	);

	sync_fifo_axis_source #(
		.DATA_LEN(FIFO_RX_LEN)
	) loopback_tx_source (
		.clk(ft_clk_i),
		.rst_n(ft_rst_n_i),
		.clear_i(rx_recovery_clear_req_ft || ft_state_clear_req_ft || tx_source_change),
		.enable_i(loopback_mode_ft && tx_prefetch_en && !ft_txe_n_i),
		.fifo_data_i(loop_fifo_rdata),
		.fifo_empty_i(loop_fifo_empty),
		.fifo_ren_o(loop_fifo_ren),
		.m_valid_o(loopback_axis_tvalid),
		.m_ready_i(loopback_axis_tready),
		.m_data_o(loopback_axis_word)
	);

	// Explicit TX stream priority: status response, then loopback, then normal payload.
	axis_tx_arbiter #(
		.DATA_LEN(DATA_LEN),
		.BE_LEN(BE_LEN)
	) axis_tx_arbiter (
		.status_lock_i(status_source_sel),
		.loopback_sel_i(loopback_mode_ft),
		.s_normal_valid_i(normal_axis_tvalid),
		.s_normal_ready_o(normal_axis_tready),
		.s_normal_data_i(normal_axis_tdata),
		.s_normal_keep_i(normal_axis_tkeep),
		.s_loopback_valid_i(loopback_axis_tvalid),
		.s_loopback_ready_o(loopback_axis_tready),
		.s_loopback_data_i(loopback_axis_tdata),
		.s_loopback_keep_i(loopback_axis_tkeep),
		.s_status_valid_i(status_axis_tvalid),
		.s_status_ready_o(status_axis_tready),
		.s_status_data_i(status_axis_tdata),
		.s_status_keep_i(status_axis_tkeep),
		.m_valid_o(tx_axis_tvalid),
		.m_ready_i(tx_axis_tready),
		.m_data_o(tx_axis_tdata),
		.m_keep_o(tx_axis_tkeep)
	);

	// Loopback control
	rx_stream_router #(
		.DATA_LEN(DATA_LEN),
		.BE_LEN(BE_LEN),
		.FIFO_RX_LEN(FIFO_RX_LEN)
	) rx_stream_router (
		.clk(ft_clk_i),
		.rst_n(ft_rst_n_i),
		.clear_i(ft_state_clear_req_ft),
		.hold_i(service_hold_ft),
		.loopback_mode_i(loopback_mode_ft),
		.rxf_n_i(ft_rxf_n_i),
		.s_valid_i(ft_rx_axis_tvalid),
		.s_ready_o(ft_rx_axis_tready),
		.s_data_i(ft_rx_axis_tdata),
		.s_keep_i(ft_rx_axis_tkeep),
		.cmd_valid_o(rx_cmd_valid),
		.cmd_opcode_o(rx_cmd_opcode),
		.m_payload_valid_o(loopback_payload_tvalid),
		.m_payload_ready_i(loopback_payload_tready),
		.m_payload_data_o(loopback_payload_tdata),
		.m_payload_keep_o(loopback_payload_tkeep),
		.prefetch_en_o(tx_prefetch_en),
		.source_flush_o(tx_source_change)
	);

	// Status response control
	status_source #(
		.DATA_LEN(DATA_LEN),
		.BE_LEN(BE_LEN)
	) status_source (
		.clk(ft_clk_i),
		.rst_n(ft_rst_n_i),
		.clear_i(ft_state_clear_req_ft),
		.hold_i(service_hold_ft),
		.issue_ready_i(status_issue_window),
		.req_i(status_req),
		.m_ready_i(status_axis_tready),
		.send_fire_i(status_source_sel && !fsm_wr_o),
		.loopback_mode_i(loopback_mode_ft),
		.tx_error_i(tx_error),
		.rx_error_i(rx_error),
		.tx_fifo_empty_i(normal_fifo_empty),
		.tx_fifo_full_i(tx_fifo_full_ft),
		.loopback_fifo_empty_i(loop_fifo_empty),
		.loopback_fifo_full_i(loop_fifo_full),
		.m_active_o(status_source_sel),
		.m_valid_o(status_axis_tvalid),
		.m_data_o(status_axis_tdata),
		.m_keep_o(status_axis_tkeep)
	);

	// GPIO I/O wrapper
	gpio_wrapper gpio_wrapper(
		.clk_i(GPIO_CLK),
		.strob_i(GPIO_STROB),
		.data_i(GPIO_DATA),
		.data_o(gpio_data),
		.strob_o(gpio_strob),
		.clk_o(gpio_clk)
	);
	
	// Packer 8-bit bus to 32-bit bus
	packer8to32 packer(
		.clk(gpio_clk),
		.rst_n(gpio_rst_n_i),
		.valid_i(gpio_strob),
		.data_i(gpio_data),
		.valid_o(packer_valid_o),
		.data_o(packer_data_o)
	);
	
	// Normal mode asynchronous FIFO
	async_fifo #(
		.DATA_LEN(DATA_LEN),
		.DEPTH(FIFO_DEPTH)
	) async_fifo(
		.clk_wr(gpio_clk),
		.clk_rd(ft_clk_i),
		.rst_wr_n(gpio_rst_n_i),
		.rst_rd_n(ft_rst_n_i),
		.clear_wr_i(tx_fifo_clear_gpio),
		.clear_rd_i(tx_fifo_clear_ft),
		.wen_i(packer_wen_i),
		.ren_i(normal_fifo_ren),
		.sram_data_r(sram_out),
		.data_i(normal_fifo_wdata),
		.data_o(normal_fifo_rdata),
		.sram_data_w(sram_in),
		.wen_o(normal_fifo_wen),
		.wr_addr_o(normal_fifo_waddr),
		.rd_addr_o(normal_fifo_raddr),
		.full(normal_fifo_full),
		.empty(normal_fifo_empty)
	);
	
	// Asynchronous FIFO SRAM
	sram_dp #(
		.DATA_LEN(DATA_LEN),
		.DEPTH(FIFO_DEPTH)
	) mem_tx(
		.wr_clk(gpio_clk),
		.rd_clk(ft_clk_i),
		.wen(normal_fifo_wen),
		.wr_addr(normal_fifo_waddr),
		.rd_addr(normal_fifo_raddr),
		.data_i(sram_in),
		.data_o(sram_out)
	);

	// Loopback mode FIFO
	loopback_fifo #(
		.DATA_LEN(FIFO_RX_LEN),
		.DEPTH(FIFO_DEPTH)
	) loopback_fifo(
		.clk(ft_clk_i),
		.rst_n(ft_rst_n_i),
		.clear_i(rx_recovery_clear_req_ft),
		.wen_i(loop_fifo_wen),
		.ren_i(loop_fifo_ren),
		.data_i(loop_fifo_wdata),
		.data_o(loop_fifo_rdata),
		.full(loop_fifo_full),
		.empty(loop_fifo_empty)
	);

	// PC command decoder
	service_cmd_decoder #(
		.DATA_LEN(DATA_LEN),
		.BE_LEN(BE_LEN),
		.FIFO_RX_LEN(FIFO_RX_LEN)
	) service_cmd_decoder(
		.clk(ft_clk_i),
		.rst_n(ft_rst_n_i),
		.cmd_valid_i(rx_cmd_valid),
		.cmd_opcode_i(rx_cmd_opcode),
		.idle_i(fsm_idle_o),
		.normal_rd_underflow_i(tx_fifo_rd_underflow_event),
		.normal_wr_error_i(tx_fifo_error_wr_ft),
		.loopback_wr_overflow_i(loopback_wr_overflow_event),
		.loopback_rd_underflow_i(loopback_rd_underflow_event),
		.tx_error_o(tx_error),
		.rx_error_o(rx_error),
		.tx_fifo_clear_o(tx_recovery_clear_req_ft),
		.loopback_fifo_clear_o(rx_recovery_clear_req_ft),
		.state_clear_o(ft_state_clear_req_ft),
		.tx_flush_o(tx_prefetch_flush_req_ft),
		.status_req_o(status_req),
		.hold_o(service_hold_ft),
		.loopback_mode_o(loopback_mode_ft)
	);

	// TX write guard
	tx_write_guard tx_guard(
		.clk(gpio_clk),
		.rst_n(gpio_rst_n_i),
		.clear_i(tx_fifo_clear_gpio),
		.write_enable_i(!loopback_mode_gpio && !service_hold_gpio),
		.packer_valid_i(packer_valid_o),
		.full_fifo_i(normal_fifo_full),
		.tx_fifo_error_i(tx_error),
		.rx_fifo_error_i(rx_error),
		.packer_wen_o(packer_wen_i),
		.tx_fifo_error_wr_o(tx_fifo_error_wr_gpio)
	);

	// FT-domain FSM 
	ft601_fsm ft601_fsm(
		.rst_n(ft_rst_n_i),
		.clk(ft_clk_i),
		.txe_n(ft_txe_n_i),
		.rxf_n(ft_rxf_n_i),
		.tx_axis_tvalid_i(tx_axis_tvalid),
		.tx_axis_tready_o(tx_axis_tready),
		.tx_axis_tdata_i(tx_axis_tdata),
		.tx_axis_tkeep_i(tx_axis_tkeep),
		.rx_axis_tvalid_o(ft_rx_axis_tvalid),
		.rx_axis_tready_i(ft_rx_axis_tready),
		.rx_axis_tdata_o(ft_rx_axis_tdata),
		.rx_axis_tkeep_o(ft_rx_axis_tkeep),
		.rx_data_i(rx_data),
		.rx_keep_i(rx_be),
		.tx_flush_i(tx_source_change || tx_prefetch_flush_req_ft),
		.state_clear_i(ft_state_clear_req_ft),
		.service_hold_i(service_hold_ft),
		.tx_prefetch_en_i(tx_prefetch_en),
		.tx_source_status_i(status_source_sel),
		.tx_data_o(tx_data),
		.tx_keep_o(tx_be),
		.wr_n(fsm_wr_o),
		.rd_n(fsm_rd_o),
		.oe_n(fsm_oe_o),
		.drive_tx(drive_tx),
		.idle_o(fsm_idle_o)
	);
	
endmodule
