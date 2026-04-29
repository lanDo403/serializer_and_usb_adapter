`timescale 1ns / 1ps

module ft601_tx_adapter #(
	parameter DATA_LEN = 32,
	parameter BE_LEN   = 4
)(
	input                    clk_i,
	input                    rst_n_i,
	input                    txe_n_i,
	input                    arb_phase_i,
	input                    prefetch_phase_i,
	input                    burst_phase_i,
	input                    write_fire_i,
	input                    flush_i,
	input                    clear_i,
	input                    hold_i,
	input                    prefetch_en_i,
	input                    status_source_i,
	input                    s_valid_i,
	input  [DATA_LEN-1:0]    s_data_i,
	input  [BE_LEN-1:0]      s_keep_i,
	output                   s_ready_o,
	output                   word_valid_o,
	output                   idle_o,
	output [DATA_LEN-1:0]    bus_data_o,
	output [BE_LEN-1:0]      bus_keep_o,
	output                   wr_n_o,
	output                   bus_drive_o
);

	reg [DATA_LEN-1:0] data_ff;
	reg [DATA_LEN-1:0] lookahead_data_ff;
	reg [BE_LEN-1:0]   keep_ff;
	reg [BE_LEN-1:0]   lookahead_keep_ff;
	reg word_valid_ff;
	reg lookahead_valid_ff;
	reg s_fire_pending_ff;
	reg prefetch_en_ff;
	reg source_ready_ff;
	reg bus_drive_ff;

	reg [DATA_LEN-1:0] data_next;
	reg [DATA_LEN-1:0] lookahead_data_next;
	reg [BE_LEN-1:0]   keep_next;
	reg [BE_LEN-1:0]   lookahead_keep_next;
	reg word_valid_next;
	reg lookahead_valid_next;

	wire write_fire_w;
	wire write_req_w;
	wire source_ready_w;
	wire buffer_ready_w;
	wire s_fire_w;
	wire [1:0] buffer_used_w;

	assign write_fire_w = write_fire_i && word_valid_ff;
	assign write_req_w = !txe_n_i && word_valid_ff;
	assign source_ready_w = status_source_i || !txe_n_i;
	assign buffer_used_w = {1'b0, word_valid_ff} +
	                       {1'b0, lookahead_valid_ff} +
	                       {1'b0, s_fire_pending_ff};
	assign buffer_ready_w = (buffer_used_w < 2'd2) || write_fire_w;
	assign s_fire_w = !hold_i && prefetch_en_ff &&
	                  source_ready_ff && s_valid_i &&
	                  buffer_ready_w;

	// One output word plus one lookahead word are enough for continuous FT601 TX bursts.
	// flush_i drops only this local prefetch state; hold_i only blocks new source pops.
	always @(*) begin
		data_next = data_ff;
		keep_next = keep_ff;
		lookahead_data_next = lookahead_data_ff;
		lookahead_keep_next = lookahead_keep_ff;
		word_valid_next = word_valid_ff;
		lookahead_valid_next = lookahead_valid_ff;

		if (write_fire_w) begin
			if (lookahead_valid_ff) begin
				data_next = lookahead_data_ff;
				keep_next = lookahead_keep_ff;
				word_valid_next = 1'b1;
				lookahead_valid_next = 1'b0;
			end
			else begin
				word_valid_next = 1'b0;
			end
		end

		if (s_fire_pending_ff) begin
			if (!word_valid_next) begin
				data_next = s_data_i;
				keep_next = s_keep_i;
				word_valid_next = 1'b1;
			end
			else if (!lookahead_valid_next) begin
				lookahead_data_next = s_data_i;
				lookahead_keep_next = s_keep_i;
				lookahead_valid_next = 1'b1;
			end
		end
	end

	always @(posedge clk_i) begin
		if (!rst_n_i) begin
			data_ff <= {DATA_LEN{1'b0}};
			lookahead_data_ff <= {DATA_LEN{1'b0}};
			keep_ff <= {BE_LEN{1'b0}};
			lookahead_keep_ff <= {BE_LEN{1'b0}};
			word_valid_ff <= 1'b0;
			lookahead_valid_ff <= 1'b0;
			s_fire_pending_ff <= 1'b0;
			prefetch_en_ff <= 1'b0;
			source_ready_ff <= 1'b0;
		end
		else if (flush_i || clear_i) begin
			data_ff <= {DATA_LEN{1'b0}};
			lookahead_data_ff <= {DATA_LEN{1'b0}};
			keep_ff <= {BE_LEN{1'b0}};
			lookahead_keep_ff <= {BE_LEN{1'b0}};
			word_valid_ff <= 1'b0;
			lookahead_valid_ff <= 1'b0;
			s_fire_pending_ff <= 1'b0;
			prefetch_en_ff <= 1'b0;
			source_ready_ff <= 1'b0;
		end
		else begin
			data_ff <= data_next;
			lookahead_data_ff <= lookahead_data_next;
			keep_ff <= keep_next;
			lookahead_keep_ff <= lookahead_keep_next;
			word_valid_ff <= word_valid_next;
			lookahead_valid_ff <= lookahead_valid_next;
			s_fire_pending_ff <= s_fire_w;
			prefetch_en_ff <= prefetch_en_i;
			source_ready_ff <= source_ready_w;
		end
	end

	always @(posedge clk_i) begin
		if (!rst_n_i)
			bus_drive_ff <= 1'b0;
		else if (clear_i)
			bus_drive_ff <= 1'b0;
		else begin
			bus_drive_ff <= 1'b0;
			if (arb_phase_i) begin
				if (write_req_w)
					bus_drive_ff <= 1'b1;
			end
			else if (prefetch_phase_i || burst_phase_i) begin
				if (word_valid_ff)
					bus_drive_ff <= 1'b1;
			end
		end
	end

	assign s_ready_o = s_fire_w;
	assign word_valid_o = word_valid_ff;
	assign idle_o = !word_valid_ff && !lookahead_valid_ff && !s_fire_pending_ff;
	assign bus_data_o = data_ff;
	assign bus_keep_o = keep_ff;
	assign wr_n_o = !write_fire_i;
	assign bus_drive_o = bus_drive_ff;

endmodule
