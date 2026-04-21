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
	input  [DATA_LEN-1:0]   	data_i,	// Data from FIFO
	input  [BE_LEN-1:0]      tx_be_i,   // Byte enable from selected TX source
	input  [DATA_LEN-1:0]   	rx_data,		// Data from FT601
	input  [BE_LEN-1:0] 		be_i,			// Byte enable from FT601
	input 						full_fifo,
	input 						empty_fifo,
	input                  tx_clear_i,
	input                  soft_clear_i,
	input                  service_hold_i,
	input                  tx_prefetch_en_i,
	
	output [DATA_LEN-1:0] 		data_o,		// DATA
	output [DATA_LEN-1:0]  		tx_data,		// Data to FT601
	output [BE_LEN-1:0]   		be_o,			// Byte enable to FT601
	output [BE_LEN-1:0]			fifo_be,
	output  					wr_n,    
	output   					rd_n,
	output   					oe_n,
	output 						drive_tx,
	output                  idle_o,
	output                  tx_path_idle_o,
	output 						fifo_pop,
	output						fifo_append
     );
	 
	localparam ARB         = 5'b00001; // Select Recieve or Trancieve mode
	localparam TX_PREFETCH = 5'b00010; // Request the first TX word from FIFO, prefetch
	localparam TX_BURST    = 5'b00100; // Continuous write burst to FT601
	localparam RX_START    = 5'b01000; // Assert OE# one cycle before RD#
	localparam RX_BURST    = 5'b10000; // Continuous read burst from FT601

	reg [4:0] next_state;
	reg [4:0] state;
	
	reg [DATA_LEN-1:0] tx_stage_ff;
	reg [DATA_LEN-1:0] tx_prefetch_ff;
	reg [DATA_LEN-1:0] tx_data_ff;
	reg [BE_LEN-1:0]   tx_stage_be_ff;
	reg [BE_LEN-1:0]   tx_prefetch_be_ff;
	reg [DATA_LEN-1:0] rx_data_ff;
	reg [BE_LEN-1:0] be_i_ff;
	reg [BE_LEN-1:0] be_o_ff;
	
	reg drive_tx_ff;
	reg fifo_append_ff;
	
	reg tx_data_valid_ff;
	reg tx_prefetch_valid_ff;
	reg tx_prefetch_pending_ff;
	
	reg rxf_n_p1;

	wire rx_cond_w;
	wire rx_burst_keep_w;
	wire tx_req_w;
	wire tx_burst_w;
	wire tx_send_w;
	wire fifo_pop_w;
	wire wr_w;
	wire rd_w;
	wire oe_w;
	
	assign rx_cond_w  		= !rxf_n_p1 && !full_fifo;
	assign rx_burst_keep_w  = !rxf_n && !full_fifo;
	assign tx_req_w   		= !txe_n && tx_data_valid_ff;
	assign tx_burst_w		= !txe_n && tx_data_valid_ff;
	assign tx_send_w        = (state == TX_BURST) && !txe_n && tx_data_valid_ff;
	assign fifo_pop_w       = !service_hold_i && tx_prefetch_en_i && !empty_fifo && (
	                          (!tx_data_valid_ff && !tx_prefetch_pending_ff) ||
	                          ( tx_data_valid_ff && !tx_prefetch_valid_ff && !tx_prefetch_pending_ff) ||
	                          ((state == TX_BURST) && tx_send_w)
	                         );
	assign wr_w            = !((state == TX_BURST) && tx_data_valid_ff && !txe_n);
	assign rd_w            = !((state == RX_BURST) && !full_fifo && !rxf_n);
	assign oe_w            = !(((state == RX_START) && !full_fifo) ||
	                          ((state == RX_BURST) && !full_fifo && !rxf_n));
	
	
	//-------------------------------------------------------------
	// state and next_state logic
	//-------------------------------------------------------------	
	always @(posedge clk) begin
		if (!rst_n)
			state <= ARB;
		else if (soft_clear_i)
			state <= ARB;
		else
			state <= next_state;
	end
	
	always @(*) begin
		next_state = state;
		case(state)
			ARB:			next_state = (!service_hold_i && rx_cond_w) ? RX_START :
			                         (!service_hold_i && tx_req_w) ? TX_PREFETCH : ARB;
			TX_PREFETCH: 	next_state = (tx_burst_w) ? TX_BURST : TX_PREFETCH;
			TX_BURST:		next_state = (tx_burst_w) ? TX_BURST : ARB;
			RX_START:		next_state = RX_BURST;
			RX_BURST:		next_state = (rx_burst_keep_w) ? RX_BURST : ARB;
			default:		next_state = ARB;
		endcase
	end
	
	//-------------------------------------------------------------
	// Flags 
	//-------------------------------------------------------------
	always @(posedge clk) begin
		if (!rst_n) begin
			rxf_n_p1 <= 1'b1;
		end
		else if (soft_clear_i) begin
			rxf_n_p1 <= 1'b1;
		end
		else begin
			rxf_n_p1 <= rxf_n;
		end
	end
	
	//-------------------------------------------------------------
	// Prefetch
	//-------------------------------------------------------------
	always @(posedge clk) begin
		if (!rst_n) begin
			tx_stage_ff            <= {DATA_LEN{1'b0}};
			tx_prefetch_ff         <= {DATA_LEN{1'b0}};
			tx_stage_be_ff         <= {BE_LEN{1'b0}};
			tx_prefetch_be_ff      <= {BE_LEN{1'b0}};
			tx_prefetch_pending_ff <= 1'b0;
			tx_prefetch_valid_ff   <= 1'b0;
			tx_data_valid_ff       <= 1'b0;
		end
		else if (tx_clear_i || soft_clear_i) begin
			tx_stage_ff            <= {DATA_LEN{1'b0}};
			tx_prefetch_ff         <= {DATA_LEN{1'b0}};
			tx_stage_be_ff         <= {BE_LEN{1'b0}};
			tx_prefetch_be_ff      <= {BE_LEN{1'b0}};
			tx_prefetch_pending_ff <= 1'b0;
			tx_prefetch_valid_ff   <= 1'b0;
			tx_data_valid_ff       <= 1'b0;
		end
		else begin
			if (tx_send_w) begin
				if (tx_prefetch_valid_ff) begin
					tx_stage_ff       <= tx_prefetch_ff;
					tx_stage_be_ff    <= tx_prefetch_be_ff;
					tx_data_valid_ff  <= 1'b1;
					if (tx_prefetch_pending_ff) begin
						tx_prefetch_ff       <= data_i;
						tx_prefetch_be_ff    <= tx_be_i;
						tx_prefetch_valid_ff <= 1'b1;
					end
					else
						tx_prefetch_valid_ff <= 1'b0;
				end
				else if (tx_prefetch_pending_ff) begin
					tx_stage_ff      <= data_i;
					tx_stage_be_ff   <= tx_be_i;
					tx_data_valid_ff <= 1'b1;
				end
				else begin
					tx_data_valid_ff    <= 1'b0;
					tx_prefetch_valid_ff<= 1'b0;
				end
			end
			else if (tx_prefetch_pending_ff) begin
				if (!tx_data_valid_ff) begin
					tx_stage_ff      <= data_i;
					tx_stage_be_ff   <= tx_be_i;
					tx_data_valid_ff <= 1'b1;
				end
				else begin
					tx_prefetch_ff       <= data_i;
					tx_prefetch_be_ff    <= tx_be_i;
					tx_prefetch_valid_ff <= 1'b1;
				end
			end

			if (tx_prefetch_pending_ff)
				tx_prefetch_pending_ff <= fifo_pop_w;
			else if (fifo_pop_w)
				tx_prefetch_pending_ff <= 1'b1;
		end
	end
	
	//-------------------------------------------------------------
	// TX output register
	//-------------------------------------------------------------
	always @(posedge clk) begin
		if (!rst_n) begin
			tx_data_ff <= {DATA_LEN{1'b0}};
			be_o_ff    <= {BE_LEN{1'b0}};
		end
		else if (tx_clear_i || soft_clear_i) begin
			tx_data_ff <= {DATA_LEN{1'b0}};
			be_o_ff    <= {BE_LEN{1'b0}};
		end
		else if ((state == TX_PREFETCH) && tx_burst_w) begin
			tx_data_ff <= tx_stage_ff;
			be_o_ff    <= tx_stage_be_ff;
		end
		else if (tx_send_w) begin
			if (tx_prefetch_valid_ff) begin
				tx_data_ff <= tx_prefetch_ff;
				be_o_ff    <= tx_prefetch_be_ff;
			end
			else if (tx_prefetch_pending_ff) begin
				tx_data_ff <= data_i;
				be_o_ff    <= tx_be_i;
			end
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
		else if (soft_clear_i) begin
			rx_data_ff <= {DATA_LEN{1'b0}};
			be_i_ff    <= {BE_LEN{1'b0}};
		end
		else if ((state == RX_START) && !rxf_n_p1 && !full_fifo) begin
			rx_data_ff <= rx_data;
			be_i_ff    <= be_i;
		end
		else if ((state == RX_BURST) && !rxf_n && !full_fifo) begin
			rx_data_ff <= rx_data;
			be_i_ff    <= be_i;
		end
	end
	//-------------------------------------------------------------
	// Control-output block
	//-------------------------------------------------------------
	always @(posedge clk) begin
		if (!rst_n) begin
			drive_tx_ff   <= 1'b0;
			fifo_append_ff<= 1'b0;
		end
		else if (soft_clear_i) begin
			drive_tx_ff    <= 1'b0;
			fifo_append_ff <= 1'b0;
		end
		else begin
			drive_tx_ff    <= 1'b0;
			fifo_append_ff <= 1'b0;

			case (state)
				TX_PREFETCH: begin
					if (tx_data_valid_ff)
						drive_tx_ff <= 1'b1;
				end

				TX_BURST: begin
					if (tx_data_valid_ff)
						drive_tx_ff <= 1'b1;
				end

				RX_START: begin
					if (!rxf_n_p1 && !full_fifo)
						fifo_append_ff <= 1'b1;
				end

				RX_BURST: begin
					if (!rxf_n && !full_fifo)
						fifo_append_ff <= 1'b1;
				end

				default: begin
				end
			endcase
		end
	end
	
	
	assign tx_data     = tx_data_ff;
	assign data_o      = rx_data_ff;
	assign be_o        = be_o_ff;
	assign fifo_be     = be_i_ff;
	assign wr_n        = wr_w;
	assign rd_n        = rd_w;
	assign oe_n        = oe_w;
	assign drive_tx    = drive_tx_ff;
	assign idle_o      = (state == ARB) && wr_w && rd_w && oe_w && !fifo_append_ff && !drive_tx_ff;
	assign tx_path_idle_o = idle_o && !tx_data_valid_ff && !tx_prefetch_valid_ff && !tx_prefetch_pending_ff;
	assign fifo_pop    = fifo_pop_w;
	assign fifo_append = fifo_append_ff;
		
endmodule
