`timescale 1ns / 1ps
`include "top.v"

/* verilator lint_off DECLFILENAME */
/* verilator lint_off UNUSEDPARAM */
module IBUFG #(parameter IOSTANDARD="LVCMOS33") (
   output wire O,
   input  wire I
);
   assign O = I;
endmodule

module IBUF #(parameter IOSTANDARD="LVCMOS33") (
   output wire O,
   input  wire I
);
   assign O = I;
endmodule

module OBUF #(parameter DRIVE=12, parameter IOSTANDARD="LVCMOS33", parameter SLEW="SLOW") (
   input  wire I,
   output wire O
);
   assign O = I;
endmodule

module IOBUF #(parameter DRIVE=12, parameter IOSTANDARD="LVCMOS33", parameter SLEW="SLOW") (
   input  wire I,
   output wire O,
   inout  wire IO,
   input  wire T
);
   assign IO = T ? 1'bz : I;
   assign O  = IO;
endmodule
/* verilator lint_on UNUSEDPARAM */
/* verilator lint_on DECLFILENAME */

module testbench;

   parameter FT_LOOPBACK_TEST = 1'b0;

   localparam integer TOTAL_WORDS = 3402;
   localparam integer PAUSE_LEN   = 16;

   localparam integer GPIO_LEN    = 8;
   localparam integer DATA_LEN    = 32;
   localparam integer BE_LEN      = 4;
   localparam integer FIFO_DEPTH  = 8192;
   localparam integer MAX_WORDS   = (TOTAL_WORDS / 4) + 4;

   reg                  gpio_clk;
   reg                  ft_clk;
   reg                  gpio_strob;
   reg                  fpga_reset;
   reg                  ft_reset_n;
   reg  [GPIO_LEN-1:0]  gpio_data;
   reg                  ft_txe_n;
   reg                  ft_rxf_n;

   reg                  host_drive_en;
   reg  [DATA_LEN-1:0]  host_data_drv;
   reg  [BE_LEN-1:0]    host_be_drv;

   wire                 ft_oe_n;
   wire                 ft_wr_n;
   wire                 ft_rd_n;
   wire [DATA_LEN-1:0]  ft_data_bus;
   wire [BE_LEN-1:0]    ft_be_bus;

   reg [7:0]  byte_seq_p [0:TOTAL_WORDS-1];
   reg [31:0] exp_words  [0:MAX_WORDS-1];

   integer exp_words_n;
   integer tx_words_n;
   integer rx_active_cycles_n;
   integer oe_active_cycles_n;

   assign ft_data_bus = host_drive_en ? host_data_drv : {DATA_LEN{1'bz}};
   assign ft_be_bus   = host_drive_en ? host_be_drv   : {BE_LEN{1'bz}};

   always #10  gpio_clk = ~gpio_clk;   // 50 MHz GPIO clock
   always #7.5 ft_clk   = ~ft_clk;     // 66.67 MHz FT601 clock

   top #(
      .GPIO_LEN(GPIO_LEN),
      .DATA_LEN(DATA_LEN),
      .BE_LEN(BE_LEN),
      .FIFO_DEPTH(FIFO_DEPTH),
      .FT_LOOPBACK_TEST(FT_LOOPBACK_TEST)
   ) dut (
      .GPIO_CLK(gpio_clk),
      .GPIO_DATA(gpio_data),
      .GPIO_STROB(gpio_strob),
      .FPGA_RESET(fpga_reset),
      .CLK(ft_clk),
      .RESET_N(ft_reset_n),
      .TXE_N(ft_txe_n),
      .RXF_N(ft_rxf_n),
      .OE_N(ft_oe_n),
      .WR_N(ft_wr_n),
      .RD_N(ft_rd_n),
      .BE(ft_be_bus),
      .DATA(ft_data_bus)
   );

   initial begin
      gpio_clk      = 1'b0;
      ft_clk        = 1'b0;
      gpio_strob    = 1'b0;
      fpga_reset    = 1'b0;
      ft_reset_n    = 1'b0;
      gpio_data     = {GPIO_LEN{1'b0}};
      ft_txe_n      = 1'b1;
      ft_rxf_n      = 1'b1;
      host_drive_en = 1'b0;
      host_data_drv = {DATA_LEN{1'b0}};
      host_be_drv   = {BE_LEN{1'b0}};
      exp_words_n   = 0;
      tx_words_n    = 0;
      rx_active_cycles_n = 0;
      oe_active_cycles_n = 0;
   end

   task fail(input [1023:0] msg);
      begin
         $display("ERROR: %0s", msg);
         $finish;
      end
   endtask

   task tb_reset;
      integer n;
      integer gpio_release_cycles;
      integer ft_release_cycles;
      begin
         $display("INFO: Applying reset requests");
         @(negedge gpio_clk);
         gpio_strob    = 1'b0;
         gpio_data     = {GPIO_LEN{1'b0}};
         ft_txe_n      = 1'b1;
         ft_rxf_n      = 1'b1;
         host_drive_en = 1'b0;
         host_data_drv = {DATA_LEN{1'b0}};
         host_be_drv   = {BE_LEN{1'b0}};
         fpga_reset    = 1'b1;
         ft_reset_n    = 1'b0;

         #1;
         if (dut.gpio_rst_n_i !== 1'b0)
            fail("gpio_rst_n_i must assert low immediately after FPGA_RESET");
         if (dut.ft_rst_n_i !== 1'b0)
            fail("ft_rst_n_i must assert low immediately after RESET_N goes low");

         for (n = 0; n < 4; n = n + 1)
            @(posedge gpio_clk);
         for (n = 0; n < 4; n = n + 1)
            @(posedge ft_clk);

         if (ft_wr_n !== 1'b1)
            fail("WR_N must stay inactive during reset");
         if (ft_rd_n !== 1'b1)
            fail("RD_N must stay inactive during reset");
         if (ft_oe_n !== 1'b1)
            fail("OE_N must stay inactive during reset");
         if (dut.drive_tx !== 1'b0)
            fail("drive_tx must stay low during reset");

         $display("INFO: Releasing reset requests");
         fpga_reset = 1'b0;
         ft_reset_n = 1'b1;

         #1;
         if (dut.gpio_rst_n_i !== 1'b0)
            fail("gpio_rst_n_i must remain low until synchronized release");
         if (dut.ft_rst_n_i !== 1'b0)
            fail("ft_rst_n_i must remain low until synchronized release");

         gpio_release_cycles = 0;
         while ((dut.gpio_rst_n_i !== 1'b1) && (gpio_release_cycles < 4)) begin
            @(posedge gpio_clk);
            #1;
            gpio_release_cycles = gpio_release_cycles + 1;
         end
         if (dut.gpio_rst_n_i !== 1'b1)
            fail("gpio_rst_n_i did not release within four gpio clocks");

         ft_release_cycles = 0;
         while ((dut.ft_rst_n_i !== 1'b1) && (ft_release_cycles < 4)) begin
            @(posedge ft_clk);
            #1;
            ft_release_cycles = ft_release_cycles + 1;
         end
         if (dut.ft_rst_n_i !== 1'b1)
            fail("ft_rst_n_i did not release within four FT clocks");

         for (n = 0; n < 2; n = n + 1)
            @(posedge gpio_clk);
         for (n = 0; n < 2; n = n + 1)
            @(posedge ft_clk);

         #1;
         if (ft_wr_n !== 1'b1)
            fail("WR_N must stay inactive after reset release");
         if (ft_rd_n !== 1'b1)
            fail("RD_N must stay inactive after reset release");
         if (ft_oe_n !== 1'b1)
            fail("OE_N must stay inactive after reset release");

         $display("INFO: Reset release observed after %0d gpio clocks and %0d FT clocks", gpio_release_cycles, ft_release_cycles);
         $display("INFO: Reset sequence passed");
      end
   endtask

   task load_vectors;
      integer fd_p;
      integer i;
      begin
         fd_p = $fopen("data_p", "r");
         if (fd_p == 0)
            fd_p = $fopen("fpga/data_p", "r");
         if (fd_p == 0)
            fail("cannot open data_p");
         $display("INFO: Loading stimulus bytes from data_p");

         for (i = 0; i < TOTAL_WORDS; i = i + 1) begin
            if ($fscanf(fd_p, "%h\n", byte_seq_p[i]) != 1)
               fail("cannot read byte from data_p");
         end

         $fclose(fd_p);
         $display("INFO: Stimulus file loaded");
      end
   endtask

   task is_pause_template_at(
      input  integer idx,
      output reg     is_pause
   );
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
            $display("WARNING: valid byte count is not a multiple of 4");
         $display("INFO: Built %0d expected 32-bit words", exp_words_n);
      end
   endtask

   task send_one_gpio_byte(
      input [GPIO_LEN-1:0] data_i,
      input                strobe_i
   );
      begin
         @(negedge gpio_clk);
         gpio_data  = data_i;
         gpio_strob = strobe_i;
      end
   endtask

   task send_gpio_stream;
      integer idx;
      integer t;
      reg pause_here;
      begin
         idx = 0;
         while (idx < TOTAL_WORDS) begin
            is_pause_template_at(idx, pause_here);

            if (pause_here) begin
               for (t = 0; t < PAUSE_LEN; t = t + 1) begin
                  send_one_gpio_byte(byte_seq_p[idx], 1'b0);
                  idx = idx + 1;
               end
            end
            else begin
               send_one_gpio_byte(byte_seq_p[idx], 1'b1);
               idx = idx + 1;
            end
         end

         @(negedge gpio_clk);
         gpio_strob = 1'b0;
      end
   endtask

   task wait_ft_cycles(input integer cycles);
      integer i;
      begin
         for (i = 0; i < cycles; i = i + 1)
            @(negedge ft_clk);
      end
   endtask

   task wait_gpio_cycles(input integer cycles);
      integer i;
      begin
         for (i = 0; i < cycles; i = i + 1)
            @(posedge gpio_clk);
      end
   endtask

   task expect_tx_word(
      input integer        wi,
      input [DATA_LEN-1:0] got_data,
      input [BE_LEN-1:0]   got_be
   );
      begin
         if (wi >= exp_words_n)
            fail("unexpected FT601 TX word");
         if (got_data !== exp_words[wi]) begin
            $display("ERROR: TX word [%0d] got=%h expected=%h", wi, got_data, exp_words[wi]);
            $finish;
         end
         if (got_be !== {BE_LEN{1'b1}}) begin
            $display("ERROR: TX BE [%0d] got=%h expected=%h", wi, got_be, {BE_LEN{1'b1}});
            $finish;
         end
      end
   endtask

   task wait_for_tx_words(
      input integer expected_words,
      input integer timeout_cycles
   );
      integer i;
      begin
         for (i = 0; i < timeout_cycles; i = i + 1) begin
            if (tx_words_n == expected_words)
               i = timeout_cycles;
            else
               @(negedge ft_clk);
         end

         if (tx_words_n !== expected_words) begin
            $display("ERROR: TX timeout, got=%0d expected=%0d", tx_words_n, expected_words);
            $finish;
         end
      end
   endtask

   task expect_no_tx_for_cycles(input integer cycles);
      integer start_words;
      begin
         start_words = tx_words_n;
         wait_ft_cycles(cycles);
         if (tx_words_n !== start_words)
            fail("FT601 transmitted data while TXE_N was inactive");
      end
   endtask

   task drive_ft_loopback_stream;
      integer i;
      integer timeout;
      begin
         @(negedge ft_clk);
         host_drive_en = 1'b1;
         host_be_drv   = {BE_LEN{1'b1}};
         host_data_drv = exp_words[0];
         ft_rxf_n      = 1'b0;

         timeout = 0;
         while ((ft_rd_n !== 1'b0) || (ft_oe_n !== 1'b0)) begin
            @(negedge ft_clk);
            timeout = timeout + 1;
            if (timeout > 64)
               fail("FT601 RX burst did not start");
         end

         for (i = 0; i < exp_words_n; i = i + 1) begin
            @(negedge ft_clk);
            if ((ft_rd_n !== 1'b0) || (ft_oe_n !== 1'b0))
               fail("RX burst has an unexpected gap");
            if (i + 1 < exp_words_n)
               host_data_drv = exp_words[i + 1];
            else
               ft_rxf_n = 1'b1;
         end

         @(negedge ft_clk);
         host_drive_en = 1'b0;
         host_data_drv = {DATA_LEN{1'b0}};
         host_be_drv   = {BE_LEN{1'b0}};
      end
   endtask

   task test_gpio_mode;
      begin
         if (FT_LOOPBACK_TEST !== 1'b0)
            fail("GPIO-mode test started while FT_LOOPBACK_TEST is not 0");
         $display("INFO: Starting GPIO to FT601 mode test");

         send_gpio_stream();
         $display("INFO: GPIO stimulus sent into packer/FIFO path");
         wait_gpio_cycles(8);

         expect_no_tx_for_cycles(8);
         $display("INFO: Confirmed TX path stays idle while TXE_N is inactive");

         @(negedge ft_clk);
         ft_txe_n = 1'b0;
         $display("INFO: TXE_N asserted low, waiting for FT601 transmission");

         wait_for_tx_words(exp_words_n, 12000);

         if (rx_active_cycles_n != 0)
            fail("RD_N became active in GPIO TX-only mode");
         if (oe_active_cycles_n != 0)
            fail("OE_N became active in GPIO TX-only mode");
         if (dut.empty_fifo !== 1'b1)
            fail("TX FIFO is not empty after GPIO-mode transmission");
         $display("INFO: GPIO mode test passed, transmitted %0d words", tx_words_n);
      end
   endtask

   task test_loopback_mode;
      begin
         if (FT_LOOPBACK_TEST !== 1'b1)
            fail("Loopback-mode test started while FT_LOOPBACK_TEST is not 1");
         $display("INFO: Starting FT601 loopback mode test");

         @(negedge ft_clk);
         ft_txe_n = 1'b0;
         $display("INFO: TXE_N asserted low, FT601 may accept loopback data");

         drive_ft_loopback_stream();
         $display("INFO: FT601 RX stimulus burst driven into DUT");
         wait_for_tx_words(exp_words_n, 16000);

         if (rx_active_cycles_n == 0)
            fail("RD_N never became active in loopback mode");
         if (oe_active_cycles_n == 0)
            fail("OE_N never became active in loopback mode");
         if (dut.empty_fifo_rx !== 1'b1)
            fail("RX FIFO is not empty after loopback transmission");
         $display("INFO: Loopback mode test passed, looped back %0d words", tx_words_n);
      end
   endtask

   // Capture FT601 TX words after DUT outputs settle on the previous posedge.
   always @(negedge ft_clk or negedge ft_reset_n) begin
      if (!ft_reset_n) begin
         tx_words_n <= 0;
         rx_active_cycles_n <= 0;
         oe_active_cycles_n <= 0;
      end
      else begin
         if (!ft_wr_n) begin
            if (host_drive_en)
               fail("DATA/BE bus contention during FT601 TX");
            expect_tx_word(tx_words_n, ft_data_bus, ft_be_bus);
            tx_words_n <= tx_words_n + 1;
         end

         if (!ft_rd_n)
            rx_active_cycles_n <= rx_active_cycles_n + 1;
         if (!ft_oe_n)
            oe_active_cycles_n <= oe_active_cycles_n + 1;
      end
   end

   initial begin
      $display("INFO: Testbench start. FT_LOOPBACK_TEST=%0d", FT_LOOPBACK_TEST);
      load_vectors();
      build_expected_words();

      if (dut.FT_LOOPBACK_TEST !== FT_LOOPBACK_TEST)
         fail("top parameter FT_LOOPBACK_TEST mismatch");
      $display("INFO: Top parameter check passed");

      tb_reset();

      if (FT_LOOPBACK_TEST == 1'b0)
         test_gpio_mode();
      else
         test_loopback_mode();

      $display("TEST PASSED. FT_LOOPBACK_TEST=%0d words=%0d", FT_LOOPBACK_TEST, exp_words_n);
      $finish;
   end

   initial begin
      $dumpfile("testbench.vcd");
      $dumpvars(0, testbench);
   end

endmodule
