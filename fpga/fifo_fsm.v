`timescale 1ns / 1ps

module fifo_fsm
#(
	parameter DATA_LEN = 32,
	parameter BE_LEN   = 4
)
(
	input  						rst_n,
	input  						clk,
	
	input  						txe_n,		// Trancieve empty from FT601 
	input  						rxf_n,		// Recieve full from FT601
	input  [DATA_LEN-1:0]   data_i,	// Data from FIFO
	input  [DATA_LEN-1:0]   rx_data,		// Data from FT601
	input  [BE_LEN-1:0] 		be_i,			// Byte enable from FT601
	input 						full_fifo,
	input 						empty_fifo,
	
	output [DATA_LEN-1:0] 	data_o,		// DATA
	output [DATA_LEN-1:0]  	tx_data,		// Data to FT601
	output [BE_LEN-1:0]   	be_o,			// Byte enable to FT601
	output [BE_LEN-1:0]		fifo_be,
	output  						wr_n,    
	output   					rd_n,
	output   					oe_n,
	output 						drive_tx,
	output 						fifo_pop,
	output						fifo_append
    );
	 
	localparam ARB         = 5'b00001; // Select Recieve or Trancieve mode
	localparam TX_PREFETCH = 5'b00010; // Request the first TX word from FIFO, prefetch
	localparam TX_BURST    = 5'b00100; // Continuous write burst to FT601
	localparam RX_START    = 5'b01000; // Assert OE#/RD# before the first RX word
	localparam RX_BURST    = 5'b10000; // Continuous read burst from FT601

	reg [4:0] next_state;
	reg [4:0] state;
	
	reg [DATA_LEN-1:0] tx_prefetch_ff;
	reg [DATA_LEN-1:0] tx_data_ff;
	reg [DATA_LEN-1:0] rx_data_ff;
	reg [BE_LEN-1:0] be_i_ff;
	reg [BE_LEN-1:0] be_o_ff;
	
	reg wr_ff;
	reg rd_ff;
	reg oe_ff;
	reg drive_tx_ff;
	reg fifo_pop_ff;
	reg fifo_append_ff;
	
	reg tx_prefetch_valid_ff;
	reg tx_prefetch_pending_ff;
	
	reg txe_n_p1;
	reg rxf_n_p1;

	wire rx_cond_w;
	wire tx_req_w;
	wire tx_burst_w;
	
	assign rx_cond_w  = !rxf_n_p1 && !full_fifo;
	assign tx_req_w   = !txe_n_p1 && (!empty_fifo || tx_prefetch_valid_ff || tx_prefetch_pending_ff);
	assign tx_burst_w = !txe_n_p1 && tx_prefetch_valid_ff;
	
	
	//-------------------------------------------------------------
	// state and next_state logic
	//-------------------------------------------------------------	
	always @(posedge clk) begin
		if (!rst_n)
			state <= ARB;
		else
			state <= next_state;
	end
	
	always @(*) begin
		next_state = state;
		case(state)
			ARB:				next_state = (rx_cond_w) ? RX_START : (tx_req_w) ? TX_PREFETCH : ARB;
			TX_PREFETCH: 	next_state = (tx_prefetch_valid_ff) ? TX_BURST : TX_PREFETCH;
			TX_BURST:		next_state = (tx_burst_w) ? TX_BURST : ARB;
			RX_START:		next_state = RX_BURST;
			RX_BURST:		next_state = (rx_cond_w) ? RX_BURST : ARB;
			default:			next_state = ARB;
		endcase
	end
	
	//-------------------------------------------------------------
	// Flags 
	//-------------------------------------------------------------
	always @(posedge clk) begin
		if (!rst_n) begin
			txe_n_p1 <= 1'b1;
			rxf_n_p1 <= 1'b1;
		end
		else begin
			txe_n_p1 <= txe_n;
			rxf_n_p1 <= rxf_n;
		end
	end
	
	//-------------------------------------------------------------
	// Prefetch
	//-------------------------------------------------------------
	always @(posedge clk) begin
		if (!rst_n) begin
			tx_prefetch_ff         <= {DATA_LEN{1'b0}};
			tx_prefetch_valid_ff   <= 1'b0;
			tx_prefetch_pending_ff <= 1'b0;
		end
		else begin
			if (fifo_pop_ff)
				tx_prefetch_pending_ff <= 1'b1;
			if (tx_prefetch_pending_ff) begin
				tx_prefetch_ff         <= data_i;
				tx_prefetch_valid_ff   <= 1'b1;
				tx_prefetch_pending_ff <= 1'b0;
			end
			if ((state == TX_BURST) && !txe_n_p1 && tx_prefetch_valid_ff)
				tx_prefetch_valid_ff <= 1'b0;
		end
	end
	
	//-------------------------------------------------------------
	// TX data register
	//-------------------------------------------------------------
	always @(posedge clk) begin
		if (!rst_n) begin
			tx_data_ff <= {DATA_LEN{1'b0}};
			be_o_ff    <= {BE_LEN{1'b0}};
		end
		else if ((state == TX_BURST) && !txe_n_p1 && tx_prefetch_valid_ff) begin
			tx_data_ff <= tx_prefetch_ff;
			be_o_ff    <= {BE_LEN{1'b1}};
		end
	end
	
	//-------------------------------------------------------------
	// RX data register
	//-------------------------------------------------------------
	always @(posedge clk) begin
		if (!rst_n) begin
			rx_data_ff <= {DATA_LEN{1'b0}};
			be_i_ff    <= {BE_LEN{1'b0}};
		end
		else if ((state == RX_BURST) && !rxf_n_p1 && !full_fifo) begin
			rx_data_ff <= rx_data;
			be_i_ff    <= be_i;
		end
	end
	//-------------------------------------------------------------
	// Control-output block
	//-------------------------------------------------------------
	always @(posedge clk) begin
		if (!rst_n) begin
			wr_ff         <= 1'b1;
			rd_ff         <= 1'b1;
			oe_ff         <= 1'b1;
			drive_tx_ff   <= 1'b0;
			fifo_pop_ff   <= 1'b0;
			fifo_append_ff<= 1'b0;
		end
		else begin
			wr_ff          <= 1'b1;
			rd_ff          <= 1'b1;
			oe_ff          <= 1'b1;
			drive_tx_ff    <= 1'b0;
			fifo_pop_ff    <= 1'b0;
			fifo_append_ff <= 1'b0;

			case (state)
				ARB: begin
					if (!txe_n_p1 && !empty_fifo && !tx_prefetch_valid_ff && !tx_prefetch_pending_ff && !fifo_pop_ff)
						fifo_pop_ff <= 1'b1;
				end

				TX_PREFETCH: begin
					if (!tx_prefetch_valid_ff && !tx_prefetch_pending_ff && !fifo_pop_ff && !empty_fifo)
						fifo_pop_ff <= 1'b1;
				end

				TX_BURST: begin
					if (!txe_n_p1 && tx_prefetch_valid_ff) begin
						drive_tx_ff <= 1'b1;
						wr_ff       <= 1'b0;
						if (!empty_fifo && !tx_prefetch_pending_ff && !fifo_pop_ff)
							fifo_pop_ff <= 1'b1;
					end
				end

				RX_START: begin
					oe_ff <= 1'b0;
					rd_ff <= 1'b0;
				end

				RX_BURST: begin
					oe_ff <= 1'b0;
					rd_ff <= 1'b0;
					if (!rxf_n_p1 && !full_fifo)
						fifo_append_ff <= 1'b1;
				end
			endcase
		end
	end
	
	
	assign tx_data     = tx_data_ff;
	assign data_o      = rx_data_ff;
	assign be_o        = be_o_ff;
	assign fifo_be     = be_i_ff;
	assign wr_n        = wr_ff;
	assign rd_n        = rd_ff;
	assign oe_n        = oe_ff;
	assign drive_tx    = drive_tx_ff;
	assign fifo_pop    = fifo_pop_ff;
	assign fifo_append = fifo_append_ff;
		
endmodule
