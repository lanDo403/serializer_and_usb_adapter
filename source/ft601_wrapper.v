`timescale 1ns / 1ps

module ft601_wrapper #(
	parameter DATA_LEN = 32,
	parameter BE_LEN   = 4
)(
	// FT601 physical pins
	input                     CLK,
	output                    RESET_N, // Active-low reset driven to FT601
	input                     TXE_N,
	input                     RXF_N,
	output                    OE_N,
	output                    WR_N,
	output                    RD_N,
	inout  [BE_LEN-1:0]       BE,
	inout  [DATA_LEN-1:0]     DATA,

	// Internal core-side signals
	output                    clk_o,
	input                     ft601_reset_n_i,
	input                     core_rst_n_i,
	output                    txe_n_o,
	output                    rxf_n_o,
	output [BE_LEN-1:0]       rx_be_o,
	output [DATA_LEN-1:0]     rx_data_o,

	input                     oe_n_i,
	input                     wr_n_i,
	input                     rd_n_i,
	input                     drive_tx_i,
	input  [BE_LEN-1:0]       tx_be_i,
	input  [DATA_LEN-1:0]     tx_data_i
);

	wire [BE_LEN-1:0] 	rx_be_o_buf;
	wire [DATA_LEN-1:0] 	rx_data_o_buf;
	wire boundary_rst_n;
	(* IOB = "TRUE" *) reg [BE_LEN-1:0]		rx_be_o_ff;
	(* IOB = "TRUE" *) reg [DATA_LEN-1:0]	rx_data_o_ff;

	wire txe_n_ibuf;
	wire rxf_n_ibuf;
	(* IOB = "TRUE" *) reg txe_n_ff;
	(* IOB = "TRUE" *) reg rxf_n_ff;

	// FT601 output boundary. Launch signals from IO registers so board-level
	// timing is not limited by routes from the core FSM to the physical pins.
	(* IOB = "TRUE" *) reg [DATA_LEN-1:0]	tx_data_o_ff;
	(* IOB = "TRUE" *) reg [BE_LEN-1:0]		tx_be_o_ff;
	(* IOB = "TRUE" *) reg [DATA_LEN-1:0]	data_t_o_ff;
	(* IOB = "TRUE" *) reg [BE_LEN-1:0]		be_t_o_ff;
	(* IOB = "TRUE" *) reg					oe_n_o_ff;
	(* IOB = "TRUE" *) reg					wr_n_o_ff;
	(* IOB = "TRUE" *) reg					rd_n_o_ff;

	// Buffered inputs from FT601
	IBUFG #(
		.IOSTANDARD("LVCMOS33")
	) ibufg_clk (
		.I(CLK),
		.O(clk_o)
	);

	assign boundary_rst_n = ft601_reset_n_i && core_rst_n_i;

	OBUF #(
		.IOSTANDARD("LVCMOS33"),
		.DRIVE(8),
		.SLEW("FAST")
	) obuf_reset_n (
		.I(ft601_reset_n_i),
		.O(RESET_N)
	);

	IBUF #(
		.IOSTANDARD("LVCMOS33")
	) ibuf_txe_n (
		.I(TXE_N),
		.O(txe_n_ibuf)
	);

	IBUF #(
		.IOSTANDARD("LVCMOS33")
	) ibuf_rxf_n (
		.I(RXF_N),
		.O(rxf_n_ibuf)
	);

	always @(posedge clk_o or negedge boundary_rst_n) begin
		if (!boundary_rst_n) begin
			txe_n_ff <= 1'b1;
			rxf_n_ff <= 1'b1;
		end
		else begin
			txe_n_ff <= txe_n_ibuf;
			rxf_n_ff <= rxf_n_ibuf;
		end
	end

	assign txe_n_o = txe_n_ff;
	assign rxf_n_o = rxf_n_ff;

	always @(posedge clk_o or negedge boundary_rst_n) begin
		if (!boundary_rst_n) begin
			oe_n_o_ff <= 1'b1;
			wr_n_o_ff <= 1'b1;
			rd_n_o_ff <= 1'b1;
		end
		else begin
			// Use registered FT601 flags at the output boundary; raw pad flags must
			// not feed core strobes or FIFO read paths.
			oe_n_o_ff <= oe_n_i | rxf_n_ff;
			wr_n_o_ff <= wr_n_i | txe_n_ff;
			rd_n_o_ff <= rd_n_i | rxf_n_ff;
		end
	end

	// Buffered FT601 control outputs
	OBUF #(
		.IOSTANDARD("LVCMOS33"),
		.DRIVE(8),
		.SLEW("FAST")
	) obuf_oe_n (
		.I(oe_n_o_ff),
		.O(OE_N)
	);

	OBUF #(
		.IOSTANDARD("LVCMOS33"),
		.DRIVE(8),
		.SLEW("FAST")
	) obuf_wr_n (
		.I(wr_n_o_ff),
		.O(WR_N)
	);

	OBUF #(
		.IOSTANDARD("LVCMOS33"),
		.DRIVE(8),
		.SLEW("FAST")
	) obuf_rd_n (
		.I(rd_n_o_ff),
		.O(RD_N)
	);

	always @(posedge clk_o or negedge boundary_rst_n) begin
		if (!boundary_rst_n) begin
			tx_data_o_ff <= {DATA_LEN{1'b0}};
			tx_be_o_ff <= {BE_LEN{1'b0}};
			data_t_o_ff <= {DATA_LEN{1'b1}};
			be_t_o_ff <= {BE_LEN{1'b1}};
		end
		else begin
			tx_data_o_ff <= tx_data_i;
			tx_be_o_ff <= tx_be_i;
			data_t_o_ff <= {DATA_LEN{~drive_tx_i}};
			be_t_o_ff <= {BE_LEN{~drive_tx_i}};
		end
	end
	
	// Bidirectional FT601 buses
	genvar i;
	generate
		for (i = 0; i < DATA_LEN; i = i + 1) begin : gen_data_iobuf
			IOBUF #(
				.IOSTANDARD("LVCMOS33"),
				.DRIVE(8),
				.SLEW("FAST")
			) iobuf_data (
				.I(tx_data_o_ff[i]),
				.O(rx_data_o_buf[i]),
				.IO(DATA[i]),
				.T(data_t_o_ff[i])
			);
		end
	endgenerate

	genvar j;
	generate
		for (j = 0; j < BE_LEN; j = j + 1) begin : gen_be_iobuf
			IOBUF #(
				.IOSTANDARD("LVCMOS33"),
				.DRIVE(8),
				.SLEW("FAST")
			) iobuf_be (
				.I(tx_be_o_ff[j]),
				.O(rx_be_o_buf[j]),
				.IO(BE[j]),
				.T(be_t_o_ff[j])
			);
		end
	endgenerate

	always @(posedge clk_o or negedge boundary_rst_n) begin
		if (!boundary_rst_n) begin
			rx_data_o_ff <= {DATA_LEN{1'b0}};
			rx_be_o_ff <= {BE_LEN{1'b0}};
		end
		else begin
			rx_data_o_ff <= rx_data_o_buf;
			rx_be_o_ff <= rx_be_o_buf;
		end
	end

	assign rx_data_o = rx_data_o_ff;
	assign rx_be_o = rx_be_o_ff;
endmodule
