`timescale 1ns / 1ps

module sync_fifo_axis_source #(
	parameter DATA_LEN = 32
)(
	input                     clk,
	input                     rst_n,
	input                     clear_i,
	input                     enable_i,

	input  [DATA_LEN-1:0]     fifo_data_i,
	input                     fifo_empty_i,
	output                    fifo_ren_o,

	output                    m_valid_o,
	input                     m_ready_i,
	output [DATA_LEN-1:0]     m_data_o
);

	reg [DATA_LEN-1:0] data_ff;
	reg [DATA_LEN-1:0] lookahead_data_ff;
	reg [1:0]          count_ff;
	reg                read_pending_ff;

	reg [DATA_LEN-1:0] data_next;
	reg [DATA_LEN-1:0] lookahead_data_next;
	reg [1:0]          count_next;

	wire m_fire_w;
	wire [1:0] used_slots_w;

	assign m_valid_o = (count_ff != 2'd0);
	assign m_data_o = data_ff;
	assign m_fire_w = m_valid_o && m_ready_i;
	assign used_slots_w = count_ff + {1'b0, read_pending_ff};
	assign fifo_ren_o = enable_i && ((used_slots_w < 2'd2) || m_fire_w) && !fifo_empty_i;

	always @(*) begin
		data_next = data_ff;
		lookahead_data_next = lookahead_data_ff;
		count_next = count_ff;

		if (m_fire_w) begin
			if (count_ff == 2'd2) begin
				data_next = lookahead_data_ff;
				count_next = 2'd1;
			end
			else begin
				count_next = 2'd0;
			end
		end

		if (read_pending_ff) begin
			if (count_next == 2'd0) begin
				data_next = fifo_data_i;
				count_next = 2'd1;
			end
			else begin
				lookahead_data_next = fifo_data_i;
				count_next = 2'd2;
			end
		end
	end

	always @(posedge clk) begin
		if (!rst_n) begin
			data_ff <= {DATA_LEN{1'b0}};
			lookahead_data_ff <= {DATA_LEN{1'b0}};
			count_ff <= 2'd0;
			read_pending_ff <= 1'b0;
		end
		else if (clear_i) begin
			data_ff <= {DATA_LEN{1'b0}};
			lookahead_data_ff <= {DATA_LEN{1'b0}};
			count_ff <= 2'd0;
			read_pending_ff <= 1'b0;
		end
		else begin
			data_ff <= data_next;
			lookahead_data_ff <= lookahead_data_next;
			count_ff <= count_next;
			read_pending_ff <= fifo_ren_o;
		end
	end

endmodule
