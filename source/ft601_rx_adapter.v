`timescale 1ns / 1ps

module ft601_rx_adapter #(
	parameter DATA_LEN = 32,
	parameter BE_LEN   = 4
)(
	input                    clk_i,
	input                    rst_n_i,
	input                    clear_i,
	input                    start_i,
	input                    burst_i,
	input                    rxf_n_i,
	input                    m_ready_i,
	input  [DATA_LEN-1:0]    bus_data_i,
	input  [BE_LEN-1:0]      bus_keep_i,
	output [DATA_LEN-1:0]    m_data_o,
	output [BE_LEN-1:0]      m_keep_o,
	output                   m_valid_o,
	output                   rd_n_o,
	output                   oe_n_o
);

	reg [DATA_LEN-1:0] m_data_ff;
	reg [BE_LEN-1:0]   m_keep_ff;
	reg                m_valid_ff;
	reg                read_fire_ff;

	wire read_fire_w;

	assign read_fire_w = burst_i && !rxf_n_i && m_ready_i;
	assign rd_n_o = !read_fire_w;
	assign oe_n_o = !((start_i && !rxf_n_i && m_ready_i) || read_fire_w);

	always @(posedge clk_i) begin
		if (!rst_n_i) begin
			m_data_ff <= {DATA_LEN{1'b0}};
			m_keep_ff <= {BE_LEN{1'b0}};
			m_valid_ff <= 1'b0;
			read_fire_ff <= 1'b0;
		end
		else if (clear_i) begin
			m_data_ff <= {DATA_LEN{1'b0}};
			m_keep_ff <= {BE_LEN{1'b0}};
			m_valid_ff <= 1'b0;
			read_fire_ff <= 1'b0;
		end
		else begin
			m_valid_ff <= read_fire_ff;
			if (read_fire_ff) begin
				m_data_ff <= bus_data_i;
				m_keep_ff <= bus_keep_i;
			end
			read_fire_ff <= read_fire_w;
		end
	end

	assign m_data_o = m_data_ff;
	assign m_keep_o = m_keep_ff;
	assign m_valid_o = m_valid_ff;

endmodule
