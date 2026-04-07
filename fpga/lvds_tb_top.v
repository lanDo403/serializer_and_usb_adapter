`timescale 1ns / 1ps
`include "fifo_top"

module lvds_tb;
   localparam integer TOTAL_WORDS = 3402;
   localparam integer PAUSE_LEN   = 16;

   localparam integer FPGA_DATA_GPIO_LEN = 8;
   localparam integer CHIP_BE_LEN = 4;
   localparam integer CHIP_DATA_LEN = 32;
   localparam integer FIFO_DEPTH = 4096;
	localparam integer ADDR_LEN = 12; // $clog2(FIFO_DEPTH)
   localparam integer MAX_WORDS = (TOTAL_WORDS / 4) + 4;

   // Data from data_p file
   reg [7:0] byte_seq [0:TOTAL_WORDS-1]

   // FPGA input data from gpio
   reg gpio_clk = 1'b0;
   reg fpga_reset = 1'b0;
   reg gpio_strob = 1'b0;
   reg [7:0] gpio_data = 8'hff;

   // FPGA data to/from FT601
   reg chip_clk = 1'b0;
   reg chip_reset_n =1'b0;
   reg chip_txe_n = 1'b1;
   reg chip_rxf_n = 1'b1;
   reg chip_oe_n = 1'b1;
   reg chip_wr_n = 1'b1;
   reg chip_rd_n = 1'b1;
   reg [CHIP_BE_LEN-1:0] chip_be = 4'hf;
   reg [CHIP_DATA_LEN-1:0] chip_data = 32'hffffffff;

   // Input bytes loaded from file
   reg [7:0] byte_seq_p [0:TOTAL_WORDS-1];

   // Expected TX words built only from bytes sent with strobe = 1
   reg [31:0] exp_words [0:MAX_WORDS-1];
   integer    exp_words_n;

   // Expected RX words for fifo_fsm receive path checks
   reg [DATA_LEN-1:0] fsm_rx_exp_data [0:FSM_RX_MAX_WORDS-1];
   reg [BE_LEN-1:0]   fsm_rx_exp_be   [0:FSM_RX_MAX_WORDS-1];
   integer            fsm_rx_exp_n;

   always #10 lvds_clk  = ~lvds_clk;
   always #5  chip_clk  = ~chip_clk;

   fifo_top #() u_top (
      .LVDS_CLK(gpio_clk),
      .LVDS_DATA(gpio_data),
      .LVDS_STROB(gpio_strob),
      .FPGA_RESET(fpga_reset),
      .CLK(chip_clk),
      .RESET_N(chip_reset_n),
      .TXE_N(chip_txe_n),
      .RXF_N(chip_rxf_n),
      .OE_N(chip_oe_n),
      .WR_N(chip_wr_n),
      .RD_N(chip_rd_n),
      .BE(chip_be),
      .DATA(chip_data)
   );

   initial begin 
      fpga_reset = 1;
      chip_reset_n = 0;
      #20;
      fpga_reset = 0;
      chip_reset_n = 1;
      #100;
   end

   // =========================================================
   // Tasks
   // =========================================================
   task tb_reset;
      begin
         
      end   
   endtask

   task load_vectors;
      integer fd_p;
      integer i;
      begin
         fd_p = $fopen("data_p", "r");
         if (fd_p == 0)
            fd_p = $fopen("fpga/data_p", "r");
         if (fd_p == 0) begin
            $display("ERROR: cannot open data_p");
            #1000;
            $finish;
         end

         for (i = 0; i < TOTAL_WORDS; i = i + 1) begin
            if ($fscanf(fd_p, "%h\n", byte_seq_p[i]) != 1) begin
               $display("ERROR: cannot read byte [%0d] from data_p", i);
               $stop;
            end
         end

         $fclose(fd_p);
      end
   endtask

   task is_pause_template_at(input integer idx, output reg is_pause);
      integer t;
      reg [7:0] expected;
      begin
         is_pause = 1'b1;

         if (idx + PAUSE_LEN > TOTAL_WORDS)
            is_pause = 1'b0;
         else begin
            for (t = 0; t < PAUSE_LEN; t = t + 1) begin
               expected = 8'h00;
               if ((t % 4) == 0)
                  expected = 8'hFF;
               if (byte_seq_p[idx + t] !== expected)
                  is_pause = 1'b0;
            end
         end
      end
   endtask

   task build_expected_words;
      integer i;
      reg [31:0] w;
      reg [1:0]  cnt;
      reg        pause_here;
      begin
         w = 32'd0;
         cnt = 2'd0;
         exp_words_n = 0;

         i = 0;
         while (i < TOTAL_WORDS) begin
            is_pause_template_at(i, pause_here);

            if (pause_here)
               i = i + PAUSE_LEN;
            else begin
               case (cnt)
                  2'd0: w[7:0]   = byte_seq_p[i];
                  2'd1: w[15:8]  = byte_seq_p[i];
                  2'd2: w[23:16] = byte_seq_p[i];
                  2'd3: begin
                     w[31:24] = byte_seq_p[i];
                     exp_words[exp_words_n] = w;
                     exp_words_n = exp_words_n + 1;
                  end
               endcase
               cnt = cnt + 1'b1;
               i = i + 1;
            end
         end

         if (cnt != 2'd0)
            $display("WARNING: valid bytes count is not multiple of 4");
      end
   endtask

   task send_one_byte(input [7:0] bp, input strobe);
      begin
         @(negedge tb_clock);
         data_p        = bp;
         tb_strob      = strobe;
         lvds_exp_data = bp;
         lvds_exp_strob = strobe;
      end
   endtask
   
   initial begin
      $dumpfile("lvds_tb_top.vcd");
      $dumpvars(0, lvds_tb_top);
   end
endmodule;