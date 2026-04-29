`timescale 1ns / 1ps
`include "axis_tx_arbiter.v"
`include "bit_sync.v"
`include "async_fifo.v"
`include "ft601_rx_adapter.v"
`include "ft601_tx_adapter.v"
`include "ft601_fsm.v"
`include "loopback_fifo.v"
`include "ft601_wrapper.v"
`include "gpio_wrapper.v"
`include "service_cmd_decoder.v"
`include "rx_stream_router.v"
`include "packer8to32.v"
`include "pulse_sync.v"
`include "rst_sync.v"
`include "sram_dualport.v"
`include "status_source.v"
`include "tx_write_guard.v"
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

   localparam integer TOTAL_WORDS = 3402;
   localparam integer PAUSE_LEN   = 16;

   localparam integer GPIO_LEN    = 8;
   localparam integer DATA_LEN    = 32;
   localparam integer BE_LEN      = 4;
   localparam integer FIFO_RX_LEN = DATA_LEN + BE_LEN;
   localparam integer FIFO_DEPTH  = 8192;
   localparam integer MAX_WORDS   = (TOTAL_WORDS / 4) + 4;
   localparam [DATA_LEN-1:0] CMD_MAGIC = 32'hA55A5AA5;
   localparam [DATA_LEN-1:0] STATUS_MAGIC = 32'h5AA55AA5;
   localparam [DATA_LEN-1:0] CMD_CLR_TX_ERROR = 32'h00000001;
   localparam [DATA_LEN-1:0] CMD_CLR_RX_ERROR = 32'h00000002;
   localparam [DATA_LEN-1:0] CMD_CLR_ALL_ERROR = 32'h00000003;
   localparam [DATA_LEN-1:0] CMD_SET_LOOPBACK = 32'hA5A50004;
   localparam [DATA_LEN-1:0] CMD_SET_NORMAL = 32'hA5A50005;
   localparam [DATA_LEN-1:0] CMD_GET_STATUS = 32'hA5A50006;
   localparam [DATA_LEN-1:0] CMD_RESET_FT_STATE = 32'hA5A50007;
   localparam [DATA_LEN-1:0] CMD_UNKNOWN = 32'hDEADBEEF;
   localparam [BE_LEN-1:0]   FULL_BE = {BE_LEN{1'b1}};
   localparam integer        TX_BACKPRESSURE_GUARD_WORDS = 16;
   localparam integer        TX_CAPTURE_WORDS_MAX = 16;
   localparam                TB_VERBOSE_STREAM = 1'b0;
   localparam                TB_VERBOSE_LATENCY = 1'b0;
   localparam                TB_VERBOSE_COMMAND = 1'b0;
   localparam integer        TB_POSEDGE_SAMPLE_DELAY = 2;

   reg                  gpio_clk;
   reg                  ft_clk;
   reg                  gpio_strob;
   reg                  fpga_reset;
   wire                 ft_reset_n;
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
   integer tx_status_words_n;
   integer rx_words_n;
   integer rx_active_cycles_n;
   integer oe_active_cycles_n;
   reg     rx_burst_seen;
   reg     tx_burst_seen;
   reg     tx_payload_burst_seen;
   reg     prev_ft_oe_n;
   reg     prev_ft_rd_n;
   reg     prev_ft_wr_n;
   reg     prev_ft_txe_n_neg;
   reg     prev_ft_txe_n_pos;
   reg     prev_ft_rxf_n;
   reg     prev_ft_rxf_n_pos;
   reg     allow_tx_burst_split;
   reg     rx_expect_rd_after_oe;
   reg     rx_expect_release_after_rxf;
   reg     rx_oe_assert_wait;
   reg     rx_rd_assert_wait;
   reg     rx_oe_release_wait;
   reg     rx_rd_release_wait;
   integer rx_oe_assert_cycles_cur;
   integer rx_rd_assert_cycles_cur;
   integer rx_oe_release_cycles_cur;
   integer rx_rd_release_cycles_cur;
   integer rx_oe_assert_cycles_last;
   integer rx_rd_assert_cycles_last;
   integer rx_oe_release_cycles_last;
   integer rx_rd_release_cycles_last;
   integer rx_oe_assert_cycles_max;
   integer rx_rd_assert_cycles_max;
   integer rx_oe_release_cycles_max;
   integer rx_rd_release_cycles_max;
   integer rx_oe_assert_events_n;
   integer rx_rd_assert_events_n;
   integer rx_oe_release_events_n;
   integer rx_rd_release_events_n;
   reg     tx_assert_wait;
   reg     tx_release_wait;
   integer tx_assert_cycles_cur;
   integer tx_release_cycles_cur;
   integer tx_assert_cycles_last;
   integer tx_release_cycles_last;
   integer tx_assert_cycles_max;
   integer tx_release_cycles_max;
   integer tx_assert_events_n;
   integer tx_release_events_n;
   integer exp_tx_assert_cycles;
   integer exp_tx_release_cycles;
   integer exp_rx_oe_assert_cycles;
   integer exp_rx_rd_assert_cycles;
   integer exp_rx_oe_release_cycles;
   integer exp_rx_rd_release_cycles;
   integer cmd_valid_pulses_n;
   reg     rx_payload_check_en;
   reg     tx_stream_only_mode;
   integer tx_total_words_n;
   reg [DATA_LEN-1:0] last_status_header_word;
   reg [DATA_LEN-1:0] last_status_word;
   reg [DATA_LEN-1:0] tx_captured_words [0:TX_CAPTURE_WORDS_MAX-1];
   reg [BE_LEN-1:0]   tx_captured_be    [0:TX_CAPTURE_WORDS_MAX-1];

   assign ft_data_bus = host_drive_en ? host_data_drv : {DATA_LEN{1'bz}};
   assign ft_be_bus   = host_drive_en ? host_be_drv   : {BE_LEN{1'bz}};

   always #10  gpio_clk = ~gpio_clk;   // 50 MHz GPIO clock
   always #5   ft_clk   = ~ft_clk;     // 100 MHz FT601 clock

   top #(
      .GPIO_LEN(GPIO_LEN),
      .DATA_LEN(DATA_LEN),
      .BE_LEN(BE_LEN),
      .FIFO_DEPTH(FIFO_DEPTH)
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
      fpga_reset    = 1'b1;
      gpio_data     = {GPIO_LEN{1'b0}};
      ft_txe_n      = 1'b1;
      ft_rxf_n      = 1'b1;
      host_drive_en = 1'b0;
      host_data_drv = {DATA_LEN{1'b0}};
      host_be_drv   = {BE_LEN{1'b0}};
      exp_words_n   = 0;
      tx_words_n    = 0;
      tx_status_words_n = 0;
      rx_words_n    = 0;
      rx_active_cycles_n = 0;
      oe_active_cycles_n = 0;
      rx_burst_seen = 1'b0;
      tx_burst_seen = 1'b0;
      tx_payload_burst_seen = 1'b0;
      prev_ft_oe_n  = 1'b1;
      prev_ft_rd_n  = 1'b1;
      prev_ft_wr_n  = 1'b1;
      prev_ft_txe_n_neg = 1'b1;
      prev_ft_txe_n_pos = 1'b1;
      prev_ft_rxf_n = 1'b1;
      prev_ft_rxf_n_pos = 1'b1;
      allow_tx_burst_split = 1'b0;
      rx_expect_rd_after_oe      = 1'b0;
      rx_expect_release_after_rxf = 1'b0;
      rx_oe_assert_wait = 1'b0;
      rx_rd_assert_wait = 1'b0;
      rx_oe_release_wait = 1'b0;
      rx_rd_release_wait = 1'b0;
      rx_oe_assert_cycles_cur = 0;
      rx_rd_assert_cycles_cur = 0;
      rx_oe_release_cycles_cur = 0;
      rx_rd_release_cycles_cur = 0;
      rx_oe_assert_cycles_last = 0;
      rx_rd_assert_cycles_last = 0;
      rx_oe_release_cycles_last = 0;
      rx_rd_release_cycles_last = 0;
      rx_oe_assert_cycles_max = 0;
      rx_rd_assert_cycles_max = 0;
      rx_oe_release_cycles_max = 0;
      rx_rd_release_cycles_max = 0;
      rx_oe_assert_events_n = 0;
      rx_rd_assert_events_n = 0;
      rx_oe_release_events_n = 0;
      rx_rd_release_events_n = 0;
      tx_assert_wait = 1'b0;
      tx_release_wait = 1'b0;
      tx_assert_cycles_cur = 0;
      tx_release_cycles_cur = 0;
      tx_assert_cycles_last = 0;
      tx_release_cycles_last = 0;
      tx_assert_cycles_max = 0;
      tx_release_cycles_max = 0;
      tx_assert_events_n = 0;
      tx_release_events_n = 0;
      exp_tx_assert_cycles = -1;
      exp_tx_release_cycles = -1;
      exp_rx_oe_assert_cycles = -1;
      exp_rx_rd_assert_cycles = -1;
      exp_rx_oe_release_cycles = -1;
      exp_rx_rd_release_cycles = -1;
      cmd_valid_pulses_n = 0;
      rx_payload_check_en = 1'b0;
      tx_stream_only_mode = 1'b0;
      tx_total_words_n = 0;
      last_status_header_word = {DATA_LEN{1'b0}};
      last_status_word = {DATA_LEN{1'b0}};
      for (integer cap_i = 0; cap_i < TX_CAPTURE_WORDS_MAX; cap_i = cap_i + 1) begin
         tx_captured_words[cap_i] = {DATA_LEN{1'b0}};
         tx_captured_be[cap_i] = {BE_LEN{1'b0}};
      end
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
         rx_payload_check_en = 1'b0;
         tx_stream_only_mode = 1'b1;
         @(negedge gpio_clk);
         gpio_strob    = 1'b0;
         gpio_data     = {GPIO_LEN{1'b0}};
         ft_txe_n      = 1'b1;
         ft_rxf_n      = 1'b1;
         host_drive_en = 1'b0;
         host_data_drv = {DATA_LEN{1'b0}};
         host_be_drv   = {BE_LEN{1'b0}};
         fpga_reset    = 1'b1;

         #1;
         if (ft_reset_n !== 1'b0)
            fail("RESET_N output must assert low while FPGA_RESET is active");
         if (dut.gpio_rst_n_i !== 1'b0)
            fail("gpio_rst_n_i must assert low immediately after FPGA_RESET");
         if (dut.ft_rst_n_i !== 1'b0)
            fail("ft_rst_n_i must assert low immediately after FPGA_RESET");

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

         #1;
         if (ft_reset_n !== 1'b1)
            fail("RESET_N output must release high after FPGA_RESET is inactive");
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
            fd_p = $fopen("source/data_p", "r");
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

   task clear_monitors;
      integer cap_i;
      begin
         tx_words_n = 0;
         tx_status_words_n = 0;
         tx_total_words_n = 0;
         rx_words_n = 0;
         rx_active_cycles_n = 0;
         oe_active_cycles_n = 0;
         rx_burst_seen = 1'b0;
         tx_burst_seen = 1'b0;
         tx_payload_burst_seen = 1'b0;
         prev_ft_oe_n = ft_oe_n;
         prev_ft_rd_n = ft_rd_n;
         prev_ft_wr_n = ft_wr_n;
         prev_ft_txe_n_neg = ft_txe_n;
         prev_ft_txe_n_pos = ft_txe_n;
         prev_ft_rxf_n = ft_rxf_n;
         prev_ft_rxf_n_pos = ft_rxf_n;
         allow_tx_burst_split = 1'b0;
         rx_expect_rd_after_oe = 1'b0;
         rx_expect_release_after_rxf = 1'b0;
         rx_oe_assert_wait = 1'b0;
         rx_rd_assert_wait = 1'b0;
         rx_oe_release_wait = 1'b0;
         rx_rd_release_wait = 1'b0;
         rx_oe_assert_cycles_cur = 0;
         rx_rd_assert_cycles_cur = 0;
         rx_oe_release_cycles_cur = 0;
         rx_rd_release_cycles_cur = 0;
         rx_oe_assert_cycles_last = 0;
         rx_rd_assert_cycles_last = 0;
         rx_oe_release_cycles_last = 0;
         rx_rd_release_cycles_last = 0;
         rx_oe_assert_cycles_max = 0;
         rx_rd_assert_cycles_max = 0;
         rx_oe_release_cycles_max = 0;
         rx_rd_release_cycles_max = 0;
         rx_oe_assert_events_n = 0;
         rx_rd_assert_events_n = 0;
         rx_oe_release_events_n = 0;
         rx_rd_release_events_n = 0;
         tx_assert_wait = 1'b0;
         tx_release_wait = 1'b0;
         tx_assert_cycles_cur = 0;
         tx_release_cycles_cur = 0;
         tx_assert_cycles_last = 0;
         tx_release_cycles_last = 0;
         tx_assert_cycles_max = 0;
         tx_release_cycles_max = 0;
         tx_assert_events_n = 0;
         tx_release_events_n = 0;
         exp_tx_assert_cycles = -1;
         exp_tx_release_cycles = -1;
         exp_rx_oe_assert_cycles = -1;
         exp_rx_rd_assert_cycles = -1;
         exp_rx_oe_release_cycles = -1;
         exp_rx_rd_release_cycles = -1;
         cmd_valid_pulses_n = 0;
         rx_payload_check_en = 1'b0;
         tx_stream_only_mode = 1'b0;
         last_status_header_word = {DATA_LEN{1'b0}};
         last_status_word = {DATA_LEN{1'b0}};
         for (cap_i = 0; cap_i < TX_CAPTURE_WORDS_MAX; cap_i = cap_i + 1) begin
            tx_captured_words[cap_i] = {DATA_LEN{1'b0}};
            tx_captured_be[cap_i] = {BE_LEN{1'b0}};
         end
      end
   endtask

   task scenario_start(input [1023:0] name);
      begin
         $display("INFO: SCENARIO START [%0d ns] %0s", $time, name);
      end
   endtask

   task scenario_end(input [1023:0] name);
      begin
         $display("INFO: SCENARIO END   [%0d ns] %0s", $time, name);
      end
   endtask

   task set_tx_latency_expectations(
      input integer assert_cycles,
      input integer release_cycles
   );
      begin
         exp_tx_assert_cycles = assert_cycles;
         exp_tx_release_cycles = release_cycles;
      end
   endtask

   task set_rx_latency_expectations(
      input integer oe_assert_cycles,
      input integer rd_assert_cycles,
      input integer oe_release_cycles,
      input integer rd_release_cycles
   );
      begin
         exp_rx_oe_assert_cycles = oe_assert_cycles;
         exp_rx_rd_assert_cycles = rd_assert_cycles;
         exp_rx_oe_release_cycles = oe_release_cycles;
         exp_rx_rd_release_cycles = rd_release_cycles;
      end
   endtask

   task check_payload_tx_latency(
      input [1023:0] context_name
   );
      begin
         if (tx_assert_events_n == 0)
            fail("TX assert latency was never measured");
         if (tx_release_events_n == 0)
            fail("TX release latency was never measured");
         if ((exp_tx_assert_cycles >= 0) && (tx_assert_cycles_max !== exp_tx_assert_cycles)) begin
            $display("ERROR: %0s TX assert latency max got=%0d expected=%0d",
                     context_name, tx_assert_cycles_max, exp_tx_assert_cycles);
            $finish;
         end
         if ((exp_tx_release_cycles >= 0) && (tx_release_cycles_max !== exp_tx_release_cycles)) begin
            $display("ERROR: %0s TX release latency max got=%0d expected=%0d",
                     context_name, tx_release_cycles_max, exp_tx_release_cycles);
            $finish;
         end
      end
   endtask

   task check_payload_rx_latency(
      input [1023:0] context_name
   );
      begin
         if (rx_oe_assert_events_n == 0)
            fail("RX OE_N assert latency was never measured");
         if (rx_rd_assert_events_n == 0)
            fail("RX RD_N assert latency was never measured");
         if (rx_oe_release_events_n == 0)
            fail("RX OE_N release latency was never measured");
         if (rx_rd_release_events_n == 0)
            fail("RX RD_N release latency was never measured");
         if ((exp_rx_oe_assert_cycles >= 0) && (rx_oe_assert_cycles_max !== exp_rx_oe_assert_cycles)) begin
            $display("ERROR: %0s RX OE assert latency max got=%0d expected=%0d",
                     context_name, rx_oe_assert_cycles_max, exp_rx_oe_assert_cycles);
            $finish;
         end
         if ((exp_rx_rd_assert_cycles >= 0) && (rx_rd_assert_cycles_max !== exp_rx_rd_assert_cycles)) begin
            $display("ERROR: %0s RX RD assert latency max got=%0d expected=%0d",
                     context_name, rx_rd_assert_cycles_max, exp_rx_rd_assert_cycles);
            $finish;
         end
         if ((exp_rx_oe_release_cycles >= 0) && (rx_oe_release_cycles_max !== exp_rx_oe_release_cycles)) begin
            $display("ERROR: %0s RX OE release latency max got=%0d expected=%0d",
                     context_name, rx_oe_release_cycles_max, exp_rx_oe_release_cycles);
            $finish;
         end
         if ((exp_rx_rd_release_cycles >= 0) && (rx_rd_release_cycles_max !== exp_rx_rd_release_cycles)) begin
            $display("ERROR: %0s RX RD release latency max got=%0d expected=%0d",
                     context_name, rx_rd_release_cycles_max, exp_rx_rd_release_cycles);
            $finish;
         end
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

   task ft_set_txe_now(input val);
      begin
         #1;
         ft_txe_n = val;
      end
   endtask

   task ft_drive_rx_now(
      input [DATA_LEN-1:0] data_i,
      input [BE_LEN-1:0]   be_i,
      input                drive_en_i,
      input                rxf_n_i
   );
      begin
         #1;
         host_drive_en = drive_en_i;
         host_be_drv   = be_i;
         host_data_drv = data_i;
         ft_rxf_n      = rxf_n_i;
      end
   endtask

   task wait_gpio_cycles(input integer cycles);
      integer i;
      begin
         for (i = 0; i < cycles; i = i + 1)
            @(posedge gpio_clk);
      end
   endtask

   task send_gpio_word(
      input [DATA_LEN-1:0] word_i
   );
      begin
         send_one_gpio_byte(word_i[7:0], 1'b1);
         send_one_gpio_byte(word_i[15:8], 1'b1);
         send_one_gpio_byte(word_i[23:16], 1'b1);
         send_one_gpio_byte(word_i[31:24], 1'b1);
         @(negedge gpio_clk);
         gpio_strob = 1'b0;
      end
   endtask

   task send_first_expected_gpio_words(
      input integer count
   );
      integer i;
      begin
         for (i = 0; i < count; i = i + 1)
            send_gpio_word(exp_words[i]);
      end
   endtask

   task pulse_tx_recovery_clear_req_ft;
      begin
         @(negedge ft_clk);
         force dut.tx_recovery_clear_req_ft = 1'b1;
         @(posedge ft_clk);
         #1;
         release dut.tx_recovery_clear_req_ft;
      end
   endtask

   task pulse_rx_recovery_clear_req_ft;
      begin
         @(negedge ft_clk);
         force dut.rx_recovery_clear_req_ft = 1'b1;
         @(posedge ft_clk);
         #1;
         release dut.rx_recovery_clear_req_ft;
      end
   endtask

   task pulse_ft_state_clear_req_ft;
      begin
         @(negedge ft_clk);
         force dut.ft_state_clear_req_ft = 1'b1;
         @(posedge ft_clk);
         #1;
         release dut.ft_state_clear_req_ft;
      end
   endtask

   task wait_for_ft_rx_idle;
      integer timeout;
      begin
         timeout = 0;
         while ((((ft_rd_n !== 1'b1) || (ft_oe_n !== 1'b1)) || (dut.ft601_fsm.state !== 5'b00001)) && (timeout < 64)) begin
            @(posedge ft_clk);
            timeout = timeout + 1;
         end

         if ((ft_rd_n !== 1'b1) || (ft_oe_n !== 1'b1) || (dut.ft601_fsm.state !== 5'b00001))
            fail("FT601 RX path did not return to ARB/idle");
      end
   endtask

   task wait_for_ft_tx_idle;
      integer timeout;
      begin
         timeout = 0;
         while (((ft_wr_n !== 1'b1) || (dut.ft601_fsm.state !== 5'b00001)) && (timeout < 64)) begin
            @(posedge ft_clk);
            #TB_POSEDGE_SAMPLE_DELAY;
            timeout = timeout + 1;
         end

         if ((ft_wr_n !== 1'b1) || (dut.ft601_fsm.state !== 5'b00001))
            fail("FT601 TX path did not return to ARB/idle");
      end
   endtask

   task drive_ft_single_word(
      input [DATA_LEN-1:0] data_i,
      input [BE_LEN-1:0]   be_i
   );
      integer timeout;
      begin
         @(posedge ft_clk);
         ft_drive_rx_now(data_i, be_i, 1'b1, 1'b0);

         timeout = 0;
         while ((ft_rd_n !== 1'b0) || (ft_oe_n !== 1'b0)) begin
            @(negedge ft_clk);
            timeout = timeout + 1;
            if (timeout > 64)
               fail("single-word FT601 RX transaction did not start");
         end

         @(posedge ft_clk);
         ft_drive_rx_now(data_i, be_i, 1'b1, 1'b1);

         timeout = 0;
         while ((ft_rd_n !== 1'b1) || (ft_oe_n !== 1'b1)) begin
            @(negedge ft_clk);
            timeout = timeout + 1;
            if (timeout > 64)
               fail("single-word FT601 RX transaction did not complete");
         end

         wait_for_ft_rx_idle();
         ft_drive_rx_now({DATA_LEN{1'b0}}, {BE_LEN{1'b0}}, 1'b0, 1'b1);
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

   function [DATA_LEN-1:0] build_status_word;
      input loopback_mode_i;
      input tx_error_i;
      input rx_error_i;
      input tx_fifo_empty_i;
      input tx_fifo_full_i;
      input loopback_fifo_empty_i;
      input loopback_fifo_full_i;
      begin
         build_status_word = {DATA_LEN{1'b0}};
         build_status_word[0] = loopback_mode_i;
         build_status_word[1] = tx_error_i;
         build_status_word[2] = rx_error_i;
         build_status_word[3] = tx_fifo_empty_i;
         build_status_word[4] = tx_fifo_full_i;
         build_status_word[5] = loopback_fifo_empty_i;
         build_status_word[6] = loopback_fifo_full_i;
      end
   endfunction

   task expect_rx_word(
      input integer        wi,
      input [DATA_LEN-1:0] got_data,
      input [BE_LEN-1:0]   got_be
   );
      begin
         if (wi >= exp_words_n)
            fail("unexpected FT601 RX word");
         if (got_data !== exp_words[wi]) begin
            $display("ERROR: RX word [%0d] got=%h expected=%h", wi, got_data, exp_words[wi]);
            $finish;
         end
         if (got_be !== {BE_LEN{1'b1}}) begin
            $display("ERROR: RX BE [%0d] got=%h expected=%h", wi, got_be, {BE_LEN{1'b1}});
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

   task wait_for_rx_words(
      input integer expected_words,
      input integer timeout_cycles
   );
      integer i;
      begin
         for (i = 0; i < timeout_cycles; i = i + 1) begin
            if (rx_words_n == expected_words)
               i = timeout_cycles;
            else
               @(negedge ft_clk);
         end

         if (rx_words_n !== expected_words) begin
            $display("ERROR: RX timeout, got=%0d expected=%0d", rx_words_n, expected_words);
            $finish;
         end
      end
   endtask

   task wait_for_tx_status_words(
      input integer expected_words,
      input integer timeout_cycles
   );
      integer i;
      begin
         for (i = 0; i < timeout_cycles; i = i + 1) begin
            if (tx_status_words_n == expected_words)
               i = timeout_cycles;
            else
               @(negedge ft_clk);
         end

         if (tx_status_words_n !== expected_words) begin
            $display("ERROR: TX status timeout, got=%0d expected=%0d", tx_status_words_n, expected_words);
            $finish;
         end
      end
   endtask

   task wait_for_tx_total_words(
      input integer expected_words,
      input integer timeout_cycles
   );
      integer i;
      begin
         for (i = 0; i < timeout_cycles; i = i + 1) begin
            if (tx_total_words_n == expected_words)
               i = timeout_cycles;
            else
               @(negedge ft_clk);
         end

         if (tx_total_words_n !== expected_words) begin
            $display("ERROR: TX total-word timeout, got=%0d expected=%0d", tx_total_words_n, expected_words);
            $finish;
         end
      end
   endtask

   task expect_captured_tx_word(
      input integer        wi,
      input [DATA_LEN-1:0] expected_data,
      input [BE_LEN-1:0]   expected_be
   );
      begin
         if (wi >= tx_total_words_n)
            fail("captured TX stream is shorter than expected");
         if (wi >= TX_CAPTURE_WORDS_MAX)
            fail("captured TX stream index exceeds TX_CAPTURE_WORDS_MAX");
         if (tx_captured_words[wi] !== expected_data) begin
            $display("ERROR: TX captured word [%0d] got=%h expected=%h", wi, tx_captured_words[wi], expected_data);
            $finish;
         end
         if (tx_captured_be[wi] !== expected_be) begin
            $display("ERROR: TX captured BE [%0d] got=%h expected=%h", wi, tx_captured_be[wi], expected_be);
            $finish;
         end
      end
   endtask

   task expect_status_frame_at(
      input integer        first_word_index,
      input [DATA_LEN-1:0] expected_status_word
   );
      begin
         expect_captured_tx_word(first_word_index, STATUS_MAGIC, FULL_BE);
         expect_captured_tx_word(first_word_index + 1, expected_status_word, FULL_BE);
      end
   endtask

   task expect_no_tx_for_cycles(input integer cycles);
      integer start_words;
      begin
         start_words = tx_total_words_n;
         wait_ft_cycles(cycles);
         if (tx_total_words_n !== start_words)
            fail("FT601 transmitted data while TXE_N was inactive");
      end
   endtask

   task inject_txe_backpressure(
      input integer after_words,
      input integer stall_cycles
   );
      integer i;
      integer timeout;
      integer words_before_stall;
      begin
         while (tx_words_n < after_words)
            @(negedge ft_clk);

         timeout = 0;
         while ((((ft_wr_n !== 1'b0) || (dut.ft601_fsm.tx_adapter.word_valid_ff !== 1'b1)) ||
                 ((exp_words_n - tx_words_n) <= TX_BACKPRESSURE_GUARD_WORDS)) &&
                (timeout < 256)) begin
            @(negedge ft_clk);
            timeout = timeout + 1;
         end

         if (ft_wr_n !== 1'b0)
            fail("backpressure injection must start during an active TX burst");
         if (dut.ft601_fsm.tx_adapter.word_valid_ff !== 1'b1)
            fail("backpressure injection requires tx_data_valid_ff=1");
         if ((exp_words_n - tx_words_n) <= TX_BACKPRESSURE_GUARD_WORDS)
            fail("backpressure injection started too late, not enough payload words remain");

         @(posedge ft_clk);
         ft_set_txe_now(1'b1);
         $display("INFO: TXE_N deasserted high to emulate FT601 backpressure, tx_words=%0d remaining=%0d",
                  tx_words_n, exp_words_n - tx_words_n);

         timeout = 0;
         while ((ft_wr_n !== 1'b1) && (timeout < 8)) begin
            @(negedge ft_clk);
            timeout = timeout + 1;
         end
         if (ft_wr_n !== 1'b1)
            fail("WR_N did not release after TXE_N backpressure");

         words_before_stall = tx_words_n;

         for (i = 0; i < stall_cycles; i = i + 1)
            @(negedge ft_clk);

         if (tx_words_n !== words_before_stall)
            fail("TX payload advanced while TXE_N was high");

         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         $display("INFO: TXE_N asserted low again, FT601 accepts TX data");

         timeout = 0;
         while ((ft_wr_n !== 1'b0) && (timeout < 16)) begin
            @(negedge ft_clk);
            timeout = timeout + 1;
         end
         if (ft_wr_n !== 1'b0)
            fail("WR_N did not resume after TXE_N returned low");
      end
   endtask

   task drive_ft_loopback_stream;
      integer i;
      integer timeout;
      begin
         if (exp_words_n <= 0)
            fail("loopback stimulus is empty");

         @(posedge ft_clk);
         ft_drive_rx_now(exp_words[0], {BE_LEN{1'b1}}, 1'b1, 1'b0);

         timeout = 0;
         while ((ft_rd_n !== 1'b0) || (ft_oe_n !== 1'b0)) begin
            @(negedge ft_clk);
            timeout = timeout + 1;
            if (timeout > 64)
               fail("FT601 RX burst did not start");
         end

         if (exp_words_n > 1)
            ft_drive_rx_now(exp_words[1], {BE_LEN{1'b1}}, 1'b1, 1'b0);

         for (i = 1; i + 1 < exp_words_n; i = i + 1) begin
            @(posedge ft_clk);
            if ((ft_rd_n !== 1'b0) || (ft_oe_n !== 1'b0))
               fail("RX burst has an unexpected gap");
            ft_drive_rx_now(exp_words[i + 1], {BE_LEN{1'b1}}, 1'b1, 1'b0);
         end

         @(posedge ft_clk);
         ft_drive_rx_now({DATA_LEN{1'b0}}, {BE_LEN{1'b0}}, 1'b0, 1'b1);
      end
   endtask

   task drive_first_expected_loopback_words(
      input integer count
   );
      integer i;
      integer timeout;
      begin
         if (count <= 0)
            fail("loopback burst helper requires a positive word count");
         if (count > exp_words_n)
            fail("loopback burst helper count exceeds expected stimulus size");

         @(posedge ft_clk);
         ft_drive_rx_now(exp_words[0], FULL_BE, 1'b1, 1'b0);

         timeout = 0;
         while ((ft_rd_n !== 1'b0) || (ft_oe_n !== 1'b0)) begin
            @(negedge ft_clk);
            timeout = timeout + 1;
            if (timeout > 64)
               fail("short FT601 RX burst did not start");
         end

         if (count > 1)
            ft_drive_rx_now(exp_words[1], FULL_BE, 1'b1, 1'b0);

         for (i = 1; i + 1 < count; i = i + 1) begin
            @(posedge ft_clk);
            if ((ft_rd_n !== 1'b0) || (ft_oe_n !== 1'b0))
               fail("short RX burst has an unexpected gap");
            ft_drive_rx_now(exp_words[i + 1], FULL_BE, 1'b1, 1'b0);
         end

         @(posedge ft_clk);
         ft_drive_rx_now({DATA_LEN{1'b0}}, {BE_LEN{1'b0}}, 1'b0, 1'b1);
      end
   endtask

   task send_ft_command_frame(
      input [DATA_LEN-1:0] cmd_word
   );
      integer timeout;
      begin
         if (TB_VERBOSE_COMMAND)
            $display("INFO: Sending FT601 command frame magic=%h opcode=%h", CMD_MAGIC, cmd_word);

         @(posedge ft_clk);
         ft_drive_rx_now(CMD_MAGIC, FULL_BE, 1'b1, 1'b0);

         timeout = 0;
         while ((ft_rd_n !== 1'b0) || (ft_oe_n !== 1'b0)) begin
            @(negedge ft_clk);
            timeout = timeout + 1;
            if (timeout > 64)
               fail("FT601 command frame RX transaction did not start");
         end

         ft_drive_rx_now(cmd_word, FULL_BE, 1'b1, 1'b0);

         @(posedge ft_clk);
         @(posedge ft_clk);
         ft_drive_rx_now({DATA_LEN{1'b0}}, {BE_LEN{1'b0}}, 1'b0, 1'b1);

         timeout = 0;
         while ((ft_rd_n !== 1'b1) || (ft_oe_n !== 1'b1)) begin
            @(negedge ft_clk);
            timeout = timeout + 1;
            if (timeout > 64)
               fail("FT601 command frame RX transaction did not complete");
         end

         @(posedge ft_clk);
         wait_for_ft_rx_idle();
         ft_drive_rx_now({DATA_LEN{1'b0}}, {BE_LEN{1'b0}}, 1'b0, 1'b1);
         wait_ft_cycles(3);
      end
   endtask

   task enter_loopback_mode;
       integer timeout;
       begin
         if (dut.loopback_mode_ft !== 1'b0)
            fail("loopback_mode_ft must be 0 before loopback entry command");

         send_ft_command_frame(CMD_SET_LOOPBACK);

         timeout = 0;
         while ((dut.loopback_mode_ft !== 1'b1) && (timeout < 16)) begin
            @(posedge ft_clk);
            #1;
            timeout = timeout + 1;
         end

         if (dut.loopback_mode_ft !== 1'b1)
            fail("loopback_mode_ft did not assert after SET_LOOPBACK command");

         wait_gpio_cycles(4);
         if (dut.loopback_mode_gpio !== 1'b1)
            fail("loopback mode did not propagate into GPIO domain");

         timeout = 0;
         while ((dut.service_hold_ft !== 1'b0) && (timeout < 32)) begin
            @(posedge ft_clk);
            #1;
            timeout = timeout + 1;
         end

         if (dut.service_hold_ft !== 1'b0)
            fail("SET_LOOPBACK command did not return service-control to idle");

         $display("INFO: Loopback mode entered");
       end
    endtask

   task enter_normal_mode;
      integer timeout;
      begin
         if (dut.loopback_mode_ft !== 1'b1)
            fail("loopback_mode_ft must be 1 before normal-entry command");

         send_ft_command_frame(CMD_SET_NORMAL);

         timeout = 0;
         while ((dut.loopback_mode_ft !== 1'b0) && (timeout < 32)) begin
            @(posedge ft_clk);
            #1;
            timeout = timeout + 1;
         end

         if (dut.loopback_mode_ft !== 1'b0)
            fail("loopback_mode_ft did not deassert after SET_NORMAL command");

         wait_gpio_cycles(4);
         if (dut.loopback_mode_gpio !== 1'b0)
            fail("normal mode did not propagate into GPIO domain");

         timeout = 0;
         while ((dut.service_hold_ft !== 1'b0) && (timeout < 32)) begin
            @(posedge ft_clk);
            #1;
            timeout = timeout + 1;
         end

         if (dut.service_hold_ft !== 1'b0)
            fail("SET_NORMAL command did not return service-control to idle");

         $display("INFO: Normal mode entered");
       end
    endtask

   task pulse_fpga_reset_only;
      integer n;
      integer gpio_release_cycles;
      integer ft_release_cycles;
      begin
         $display("INFO: Pulsing FPGA_RESET to exit loopback mode");
         rx_payload_check_en = 1'b0;
         tx_stream_only_mode = 1'b1;

         @(negedge gpio_clk);
         gpio_strob    = 1'b0;
         gpio_data     = {GPIO_LEN{1'b0}};
         ft_txe_n      = 1'b1;
         ft_rxf_n      = 1'b1;
         host_drive_en = 1'b0;
         host_data_drv = {DATA_LEN{1'b0}};
         host_be_drv   = {BE_LEN{1'b0}};
         fpga_reset    = 1'b1;

         #1;
         if (ft_reset_n !== 1'b0)
            fail("RESET_N output must assert low while FPGA_RESET is active");

         for (n = 0; n < 4; n = n + 1)
            @(posedge gpio_clk);
         for (n = 0; n < 4; n = n + 1)
            @(posedge ft_clk);

         fpga_reset = 1'b0;

         #1;
         if (ft_reset_n !== 1'b1)
            fail("RESET_N output must release high after FPGA_RESET");

         gpio_release_cycles = 0;
         while ((dut.gpio_rst_n_i !== 1'b1) && (gpio_release_cycles < 4)) begin
            @(posedge gpio_clk);
            gpio_release_cycles = gpio_release_cycles + 1;
         end
         if (dut.gpio_rst_n_i !== 1'b1)
            fail("gpio_rst_n_i did not release after FPGA_RESET pulse");

         ft_release_cycles = 0;
         while ((dut.ft_rst_n_i !== 1'b1) && (ft_release_cycles < 4)) begin
            @(posedge ft_clk);
            ft_release_cycles = ft_release_cycles + 1;
         end
         if (dut.ft_rst_n_i !== 1'b1)
            fail("ft_rst_n_i did not release after FPGA_RESET pulse");

         wait_gpio_cycles(4);
         wait_ft_cycles(4);

         if (dut.loopback_mode_ft !== 1'b0)
            fail("loopback_mode_ft must return to 0 after FPGA_RESET");
         if (dut.loopback_mode_gpio !== 1'b0)
            fail("loopback mode must clear in GPIO domain after FPGA_RESET");

         $display("INFO: FPGA_RESET returned the design to normal mode");
      end
   endtask

   task test_fpga_reset_drives_ft601_reset;
      integer n;
      integer gpio_release_cycles;
      integer ft_release_cycles;
      begin
         $display("INFO: Checking FPGA_RESET drives FT601 RESET_N and resets both domains");
         rx_payload_check_en = 1'b0;
         tx_stream_only_mode = 1'b1;

         @(negedge gpio_clk);
         gpio_strob    = 1'b0;
         gpio_data     = {GPIO_LEN{1'b0}};
         ft_txe_n      = 1'b1;
         ft_rxf_n      = 1'b1;
         host_drive_en = 1'b0;
         host_data_drv = {DATA_LEN{1'b0}};
         host_be_drv   = {BE_LEN{1'b0}};
         fpga_reset    = 1'b1;

         #1;
         if (ft_reset_n !== 1'b0)
            fail("RESET_N output must assert low during FPGA_RESET");
         if (dut.ft_rst_n_i !== 1'b0)
            fail("ft_rst_n_i must assert low immediately after FPGA_RESET");
         if (dut.gpio_rst_n_i !== 1'b0)
            fail("gpio_rst_n_i must assert low during FPGA_RESET");
         if (ft_wr_n !== 1'b1)
            fail("WR_N must stay inactive during FPGA_RESET");
         if (ft_rd_n !== 1'b1)
            fail("RD_N must stay inactive during FPGA_RESET");
         if (ft_oe_n !== 1'b1)
            fail("OE_N must stay inactive during FPGA_RESET");
         if (ft_data_bus !== {DATA_LEN{1'bz}})
            fail("DATA bus must be tri-stated during FPGA_RESET");
         if (ft_be_bus !== {BE_LEN{1'bz}})
            fail("BE bus must be tri-stated during FPGA_RESET");

         for (n = 0; n < 4; n = n + 1)
            @(posedge gpio_clk);
         for (n = 0; n < 4; n = n + 1)
            @(posedge ft_clk);

         if (dut.drive_tx !== 1'b0)
            fail("drive_tx must clear during FPGA_RESET");

         fpga_reset = 1'b0;

         #1;
         if (ft_reset_n !== 1'b1)
            fail("RESET_N output must release high after FPGA_RESET");
         if (dut.gpio_rst_n_i !== 1'b0)
            fail("gpio_rst_n_i must remain low until synchronized FPGA_RESET release");
         if (dut.ft_rst_n_i !== 1'b0)
            fail("ft_rst_n_i must remain low until synchronized FPGA_RESET release");

         gpio_release_cycles = 0;
         while ((dut.gpio_rst_n_i !== 1'b1) && (gpio_release_cycles < 4)) begin
            @(posedge gpio_clk);
            gpio_release_cycles = gpio_release_cycles + 1;
         end
         if (dut.gpio_rst_n_i !== 1'b1)
            fail("gpio_rst_n_i did not release after FPGA_RESET pulse");

         ft_release_cycles = 0;
         while ((dut.ft_rst_n_i !== 1'b1) && (ft_release_cycles < 4)) begin
            @(posedge ft_clk);
            ft_release_cycles = ft_release_cycles + 1;
         end
         if (dut.ft_rst_n_i !== 1'b1)
            fail("ft_rst_n_i did not release after FPGA_RESET pulse");

         wait_gpio_cycles(4);
         wait_ft_cycles(4);

         if (dut.loopback_mode_ft !== 1'b0)
            fail("loopback_mode_ft must return to 0 after FPGA_RESET");
         if (dut.loopback_mode_gpio !== 1'b0)
            fail("loopback mode must clear in GPIO domain after FPGA_RESET");
         if (ft_wr_n !== 1'b1)
            fail("WR_N must stay inactive after FPGA_RESET release");
         if (ft_rd_n !== 1'b1)
            fail("RD_N must stay inactive after FPGA_RESET release");
         if (ft_oe_n !== 1'b1)
            fail("OE_N must stay inactive after FPGA_RESET release");

         $display("INFO: FPGA_RESET drove RESET_N and returned the design to normal mode");
      end
   endtask

   task test_gpio_mode;
      begin
         if (dut.loopback_mode_ft !== 1'b0)
            fail("GPIO-mode test started while loopback mode is active");
         $display("INFO: Starting GPIO to FT601 mode test");

         send_gpio_stream();
         $display("INFO: GPIO stimulus sent into packer/FIFO path");
         wait_gpio_cycles(8);

         expect_no_tx_for_cycles(8);
         $display("INFO: Confirmed TX path stays idle while TXE_N is inactive");
         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         $display("INFO: TXE_N asserted low, waiting for FT601 transmission");

         fork
            wait_for_tx_words(exp_words_n, 12000);
            inject_txe_backpressure(8, 2);
         join

         if (rx_active_cycles_n != 0)
            fail("RD_N became active in GPIO TX-only mode");
         if (oe_active_cycles_n != 0)
            fail("OE_N became active in GPIO TX-only mode");
         if (dut.normal_fifo_empty !== 1'b1)
            fail("TX FIFO is not empty after GPIO-mode transmission");
         check_payload_tx_latency("GPIO mode");
         $display("INFO: TX assert latency max=%0d FT clocks, release latency max=%0d FT clocks", tx_assert_cycles_max, tx_release_cycles_max);
         $display("INFO: GPIO mode test passed, transmitted %0d words", tx_words_n);
      end
   endtask

   task test_tx_recovery_clear_path;
      begin
         $display("INFO: Checking recovery clear primitive for async TX FIFO and gpio guard");

         force dut.tx_prefetch_en = 1'b0;
         dut.async_fifo.wr_ptr_bin = 14'd5;
         dut.async_fifo.rd_ptr_bin = 14'd3;
         dut.async_fifo.wr_ptr_gray = 14'd7;
         dut.async_fifo.rd_ptr_gray = 14'd2;
         dut.async_fifo.wr_ptr_gray_sync1 = 14'd7;
         dut.async_fifo.wr_ptr_gray_sync2 = 14'd7;
         dut.async_fifo.rd_ptr_gray_sync1 = 14'd2;
         dut.async_fifo.rd_ptr_gray_sync2 = 14'd2;
         dut.async_fifo.empty_ff = 1'b0;
         dut.async_fifo.full_ff = 1'b1;
         dut.tx_guard.tx_fifo_error_wr_ff = 1'b1;

         pulse_tx_recovery_clear_req_ft();
         wait_ft_cycles(12);
         wait_gpio_cycles(12);
         wait_ft_cycles(8);

         if (dut.normal_fifo_empty !== 1'b1) begin
            fail("TX recovery clear must force normal_fifo_empty high");
         end
         if (dut.normal_fifo_full !== 1'b0)
            fail("TX recovery clear must force normal_fifo_full low");
         if (dut.tx_guard.tx_fifo_error_wr_ff !== 1'b0)
            fail("TX recovery clear must clear gpio-domain TX guard sticky state");
         if (dut.async_fifo.wr_ptr_bin !== 0)
            fail("TX recovery clear must clear write pointer");
         if (dut.async_fifo.rd_ptr_bin !== 0)
            fail("TX recovery clear must clear read pointer");
         release dut.tx_prefetch_en;
      end
   endtask

   task test_rx_recovery_clear_path;
      begin
         $display("INFO: Checking recovery clear primitive for FT-domain loopback FIFO");

         dut.loopback_fifo.wr_ptr_ff = 14'd4;
         dut.loopback_fifo.rd_ptr_ff = 14'd1;
         dut.loopback_fifo.data_ff = 36'hF55667788;
         dut.loopback_fifo.empty_ff = 1'b0;
         dut.loopback_fifo.full_ff = 1'b1;

         pulse_rx_recovery_clear_req_ft();
         wait_ft_cycles(8);

         if (dut.loop_fifo_empty !== 1'b1)
            fail("RX recovery clear must force loopback FIFO empty");
         if (dut.loop_fifo_full !== 1'b0)
            fail("RX recovery clear must force loopback FIFO full low");
         if (dut.loopback_fifo.wr_ptr_ff !== 0)
            fail("RX recovery clear must clear loopback write pointer");
         if (dut.loopback_fifo.rd_ptr_ff !== 0)
            fail("RX recovery clear must clear loopback read pointer");
      end
   endtask

   task test_ft_state_clear_rx;
      begin
         $display("INFO: Checking recovery clear primitive for FT RX capture state");

         dut.ft601_fsm.state = 5'b10000;
         dut.ft601_fsm.rx_adapter.m_data_ff = 32'hCAFEBABE;
         dut.ft601_fsm.rx_adapter.m_keep_ff = 4'hF;
         dut.rx_stream_router.loopback_mode_p1_ff = 1'b1;
         dut.rx_stream_router.loopback_payload_en_ff = 1'b1;
         dut.rx_stream_router.cmd_valid_ff = 1'b1;
         dut.rx_stream_router.cmd_word_ff = 36'hFCAFEBABE;

         pulse_ft_state_clear_req_ft();
         wait_ft_cycles(4);

         if (dut.ft601_fsm.state !== 5'b00001)
            fail("FT-state clear must return FSM to ARB");
         if (dut.ft601_fsm.rx_adapter.m_data_ff !== {DATA_LEN{1'b0}})
            fail("FT-state clear must clear RX data register");
         if (dut.ft601_fsm.rx_adapter.m_keep_ff !== {BE_LEN{1'b0}})
            fail("FT-state clear must clear RX byte-enable register");
         if (dut.rx_stream_router.cmd_word_ff !== {FIFO_RX_LEN{1'b0}})
            fail("FT-state clear must clear captured FT RX word");
         if (dut.rx_stream_router.cmd_valid_ff !== 1'b0)
            fail("FT-state clear must clear FT RX valid flag");
         if (dut.rx_stream_router.loopback_payload_en_ff !== 1'b0)
            fail("FT-state clear must close loopback payload gate");
      end
   endtask

   task test_ft_state_clear_tx;
      begin
         $display("INFO: Checking recovery clear primitive for FT TX prefetch/state");

         dut.ft601_fsm.state = 5'b00100;
         dut.ft601_fsm.tx_adapter.lookahead_data_ff = 32'h33334444;
         dut.ft601_fsm.tx_adapter.lookahead_keep_ff = 4'hE;
         dut.ft601_fsm.tx_adapter.data_ff = 32'h55556666;
         dut.ft601_fsm.tx_adapter.keep_ff = 4'hD;
         dut.ft601_fsm.tx_adapter.word_valid_ff = 1'b1;
         dut.ft601_fsm.tx_adapter.lookahead_valid_ff = 1'b1;
         dut.ft601_fsm.tx_adapter.s_fire_pending_ff = 1'b1;

         pulse_ft_state_clear_req_ft();
         wait_ft_cycles(4);

         if (dut.ft601_fsm.state !== 5'b00001)
            fail("FT-state clear must return TX FSM to ARB");
         if (dut.ft601_fsm.tx_adapter.lookahead_data_ff !== {DATA_LEN{1'b0}})
            fail("FT-state clear must clear TX lookahead register");
         if (dut.ft601_fsm.tx_adapter.lookahead_keep_ff !== {BE_LEN{1'b0}})
            fail("FT-state clear must clear TX lookahead keep");
         if (dut.ft601_fsm.tx_adapter.data_ff !== {DATA_LEN{1'b0}})
            fail("FT-state clear must clear TX output data register");
         if (dut.ft601_fsm.tx_adapter.keep_ff !== {BE_LEN{1'b0}})
            fail("FT-state clear must clear TX output BE register");
         if (dut.ft601_fsm.tx_adapter.word_valid_ff !== 1'b0)
            fail("FT-state clear must clear TX data valid");
         if (dut.ft601_fsm.tx_adapter.lookahead_valid_ff !== 1'b0)
            fail("FT-state clear must clear TX lookahead valid");
         if (dut.ft601_fsm.tx_adapter.s_fire_pending_ff !== 1'b0)
            fail("FT-state clear must clear TX request pending");
      end
   endtask

   task test_cmd_clear_tx_recovery;
      begin
         if (dut.loopback_mode_ft !== 1'b0)
            fail("CMD_CLR_TX_ERROR test must start in normal mode");
         $display("INFO: Checking CMD_CLR_TX_ERROR real recovery path");

         force dut.tx_prefetch_en = 1'b0;
         cmd_valid_pulses_n = 0;

         dut.async_fifo.wr_ptr_bin = 14'd5;
         dut.async_fifo.rd_ptr_bin = 14'd3;
         dut.async_fifo.wr_ptr_gray = 14'd7;
         dut.async_fifo.rd_ptr_gray = 14'd2;
         dut.async_fifo.wr_ptr_gray_sync1 = 14'd7;
         dut.async_fifo.wr_ptr_gray_sync2 = 14'd7;
         dut.async_fifo.rd_ptr_gray_sync1 = 14'd2;
         dut.async_fifo.rd_ptr_gray_sync2 = 14'd2;
         dut.async_fifo.empty_ff = 1'b0;
         dut.async_fifo.full_ff = 1'b1;
         dut.tx_guard.tx_fifo_error_wr_ff = 1'b1;
         dut.service_cmd_decoder.tx_fifo_error_ff = 1'b1;
         dut.ft601_fsm.state = 5'b00100;
         dut.ft601_fsm.tx_adapter.lookahead_data_ff = 32'h33334444;
         dut.ft601_fsm.tx_adapter.lookahead_keep_ff = 4'hE;
         dut.ft601_fsm.tx_adapter.data_ff = 32'h55556666;
         dut.ft601_fsm.tx_adapter.keep_ff = 4'hD;
         dut.ft601_fsm.tx_adapter.word_valid_ff = 1'b1;
         dut.ft601_fsm.tx_adapter.lookahead_valid_ff = 1'b1;
         dut.ft601_fsm.tx_adapter.s_fire_pending_ff = 1'b1;

         send_ft_command_frame(CMD_CLR_TX_ERROR);
         wait_ft_cycles(12);
         wait_gpio_cycles(12);
         wait_ft_cycles(8);

         if (cmd_valid_pulses_n !== 1)
            fail("CMD_CLR_TX_ERROR must generate exactly one cmd_valid pulse");
         if (dut.service_cmd_decoder.tx_fifo_error_ff !== 1'b0)
            fail("CMD_CLR_TX_ERROR must clear host TX sticky error");
         if (dut.tx_guard.tx_fifo_error_wr_ff !== 1'b0)
            fail("CMD_CLR_TX_ERROR must clear gpio TX write guard sticky error");
         if (dut.normal_fifo_empty !== 1'b1)
            fail("CMD_CLR_TX_ERROR must recover async TX FIFO to empty");
         if (dut.normal_fifo_full !== 1'b0)
            fail("CMD_CLR_TX_ERROR must clear async TX FIFO full");
         if (dut.async_fifo.wr_ptr_bin !== 0)
            fail("CMD_CLR_TX_ERROR must clear TX FIFO write pointer");
         if (dut.async_fifo.rd_ptr_bin !== 0)
            fail("CMD_CLR_TX_ERROR must clear TX FIFO read pointer");
         if (dut.ft601_fsm.state !== 5'b00001)
            fail("CMD_CLR_TX_ERROR must return FSM to ARB");
         if (dut.ft601_fsm.tx_adapter.lookahead_data_ff !== {DATA_LEN{1'b0}})
            fail("CMD_CLR_TX_ERROR must clear TX lookahead register");
         if (dut.ft601_fsm.tx_adapter.data_ff !== {DATA_LEN{1'b0}})
            fail("CMD_CLR_TX_ERROR must clear TX output register");
         if (dut.ft601_fsm.tx_adapter.word_valid_ff !== 1'b0)
            fail("CMD_CLR_TX_ERROR must clear TX valid state");
         if (dut.ft601_fsm.tx_adapter.lookahead_valid_ff !== 1'b0)
            fail("CMD_CLR_TX_ERROR must clear TX lookahead valid");
         if (dut.ft601_fsm.tx_adapter.s_fire_pending_ff !== 1'b0)
            fail("CMD_CLR_TX_ERROR must clear TX request pending");
         if (dut.tx_recovery_clear_req_ft !== 1'b0)
            fail("CMD_CLR_TX_ERROR TX recovery pulse must self-clear");
         if (dut.ft_state_clear_req_ft !== 1'b0)
            fail("CMD_CLR_TX_ERROR FT-state recovery pulse must self-clear");
         if (dut.loopback_mode_ft !== 1'b0)
            fail("CMD_CLR_TX_ERROR must not change current mode");
         if (dut.rx_error !== 1'b0)
            fail("CMD_CLR_TX_ERROR test must not leave unrelated RX sticky error asserted");

         release dut.tx_prefetch_en;
      end
   endtask

   task test_cmd_reset_ft_state_keeps_tx_fifo;
      begin
         if (dut.loopback_mode_ft !== 1'b0)
            fail("CMD_RESET_FT_STATE test must start in normal mode");

         ft_txe_n = 1'b1;
         send_first_expected_gpio_words(4);
         wait_gpio_cycles(8);
         wait_ft_cycles(4);

         if (dut.normal_fifo_empty !== 1'b0)
            fail("CMD_RESET_FT_STATE test needs pending normal TX data");

         cmd_valid_pulses_n = 0;
         send_ft_command_frame(CMD_RESET_FT_STATE);
         wait_ft_cycles(12);
         wait_gpio_cycles(4);

         if (cmd_valid_pulses_n !== 1)
            fail("CMD_RESET_FT_STATE must generate exactly one cmd_valid pulse");
         if (dut.tx_recovery_clear_req_ft !== 1'b0)
            fail("CMD_RESET_FT_STATE must not request TX async FIFO clear");
         if (dut.rx_recovery_clear_req_ft !== 1'b0)
            fail("CMD_RESET_FT_STATE must not request loopback FIFO clear");
         if (dut.normal_fifo_empty !== 1'b0)
            fail("CMD_RESET_FT_STATE must preserve pending normal TX FIFO data");
         if (dut.loopback_mode_ft !== 1'b0)
            fail("CMD_RESET_FT_STATE must leave runtime mode in normal state");
         if (ft_reset_n !== 1'b1)
            fail("CMD_RESET_FT_STATE must not toggle physical FT601 RESET_N");
         if (dut.ft_state_clear_req_ft !== 1'b0)
            fail("CMD_RESET_FT_STATE FT-state clear pulse must self-clear");
      end
   endtask

   task test_cmd_clear_rx_recovery;
      begin
         if (dut.loopback_mode_ft !== 1'b1)
            fail("CMD_CLR_RX_ERROR test must start in loopback mode");
         $display("INFO: Checking CMD_CLR_RX_ERROR real recovery path");

         cmd_valid_pulses_n = 0;
         dut.loopback_fifo.wr_ptr_ff = 14'd4;
         dut.loopback_fifo.rd_ptr_ff = 14'd1;
         dut.loopback_fifo.data_ff = 36'hF55667788;
         dut.loopback_fifo.empty_ff = 1'b0;
         dut.loopback_fifo.full_ff = 1'b1;
         dut.service_cmd_decoder.rx_fifo_error_ff = 1'b1;
         dut.ft601_fsm.state = 5'b10000;
         dut.ft601_fsm.rx_adapter.m_data_ff = 32'hCAFEBABE;
         dut.ft601_fsm.rx_adapter.m_keep_ff = 4'hF;
         dut.rx_stream_router.loopback_mode_p1_ff = 1'b1;
         dut.rx_stream_router.loopback_payload_en_ff = 1'b1;
         dut.rx_stream_router.cmd_valid_ff = 1'b1;
         dut.rx_stream_router.cmd_word_ff = 36'hFCAFEBABE;

         send_ft_command_frame(CMD_CLR_RX_ERROR);
         wait_ft_cycles(12);
         wait_gpio_cycles(4);

         if (cmd_valid_pulses_n !== 1)
            fail("CMD_CLR_RX_ERROR must generate exactly one cmd_valid pulse");
         if (dut.service_cmd_decoder.rx_fifo_error_ff !== 1'b0)
            fail("CMD_CLR_RX_ERROR must clear host RX sticky error");
         if (dut.loop_fifo_empty !== 1'b1)
            fail("CMD_CLR_RX_ERROR must recover loopback FIFO to empty");
         if (dut.loop_fifo_full !== 1'b0)
            fail("CMD_CLR_RX_ERROR must clear loopback FIFO full");
         if (dut.loopback_fifo.wr_ptr_ff !== 0)
            fail("CMD_CLR_RX_ERROR must clear loopback FIFO write pointer");
         if (dut.loopback_fifo.rd_ptr_ff !== 0)
            fail("CMD_CLR_RX_ERROR must clear loopback FIFO read pointer");
         if (dut.ft601_fsm.state !== 5'b00001)
            fail("CMD_CLR_RX_ERROR must return FSM to ARB");
         if (dut.ft601_fsm.rx_adapter.m_data_ff !== {DATA_LEN{1'b0}})
            fail("CMD_CLR_RX_ERROR must clear FT RX capture register");
         if (dut.ft601_fsm.rx_adapter.m_keep_ff !== {BE_LEN{1'b0}})
            fail("CMD_CLR_RX_ERROR must clear FT RX BE register");
         if (dut.rx_stream_router.cmd_word_ff !== {FIFO_RX_LEN{1'b0}})
            fail("CMD_CLR_RX_ERROR must clear captured FT RX word");
         if (dut.rx_stream_router.cmd_valid_ff !== 1'b0)
            fail("CMD_CLR_RX_ERROR must clear FT RX valid flag");
         if (dut.rx_stream_router.loopback_payload_en_ff !== 1'b0)
            fail("CMD_CLR_RX_ERROR must close loopback payload gate");
         if (dut.rx_recovery_clear_req_ft !== 1'b0)
            fail("CMD_CLR_RX_ERROR RX recovery pulse must self-clear");
         if (dut.ft_state_clear_req_ft !== 1'b0)
            fail("CMD_CLR_RX_ERROR FT-state recovery pulse must self-clear");
         if (dut.loopback_mode_ft !== 1'b1)
            fail("CMD_CLR_RX_ERROR must not change current mode");
         if (dut.loopback_mode_gpio !== 1'b1)
            fail("CMD_CLR_RX_ERROR must not drop loopback mode in GPIO domain");
      end
   endtask

   task test_cmd_clear_all_recovery;
      begin
         if (dut.loopback_mode_ft !== 1'b1)
            fail("CMD_CLR_ALL_ERROR test must start in loopback mode");
         $display("INFO: Checking CMD_CLR_ALL_ERROR combined recovery path");

         force dut.tx_prefetch_en = 1'b0;
         cmd_valid_pulses_n = 0;

         dut.async_fifo.wr_ptr_bin = 14'd6;
         dut.async_fifo.rd_ptr_bin = 14'd2;
         dut.async_fifo.wr_ptr_gray = 14'd5;
         dut.async_fifo.rd_ptr_gray = 14'd3;
         dut.async_fifo.wr_ptr_gray_sync1 = 14'd5;
         dut.async_fifo.wr_ptr_gray_sync2 = 14'd5;
         dut.async_fifo.rd_ptr_gray_sync1 = 14'd3;
         dut.async_fifo.rd_ptr_gray_sync2 = 14'd3;
         dut.async_fifo.empty_ff = 1'b0;
         dut.async_fifo.full_ff = 1'b1;
         dut.tx_guard.tx_fifo_error_wr_ff = 1'b1;
         dut.service_cmd_decoder.tx_fifo_error_ff = 1'b1;

         dut.loopback_fifo.wr_ptr_ff = 14'd5;
         dut.loopback_fifo.rd_ptr_ff = 14'd1;
         dut.loopback_fifo.data_ff = 36'hE10203040;
         dut.loopback_fifo.empty_ff = 1'b0;
         dut.loopback_fifo.full_ff = 1'b1;
         dut.service_cmd_decoder.rx_fifo_error_ff = 1'b1;

         dut.ft601_fsm.state = 5'b00100;
         dut.ft601_fsm.rx_adapter.m_data_ff = 32'h12345678;
         dut.ft601_fsm.rx_adapter.m_keep_ff = 4'hC;
         dut.ft601_fsm.tx_adapter.lookahead_data_ff = 32'h76543210;
         dut.ft601_fsm.tx_adapter.lookahead_keep_ff = 4'hA;
         dut.ft601_fsm.tx_adapter.data_ff = 32'h0BADF00D;
         dut.ft601_fsm.tx_adapter.keep_ff = 4'hB;
         dut.ft601_fsm.tx_adapter.word_valid_ff = 1'b1;
         dut.ft601_fsm.tx_adapter.lookahead_valid_ff = 1'b1;
         dut.ft601_fsm.tx_adapter.s_fire_pending_ff = 1'b1;
         dut.rx_stream_router.loopback_mode_p1_ff = 1'b1;
         dut.rx_stream_router.loopback_payload_en_ff = 1'b1;
         dut.rx_stream_router.cmd_valid_ff = 1'b1;
         dut.rx_stream_router.cmd_word_ff = 36'hE55667788;

         send_ft_command_frame(CMD_CLR_ALL_ERROR);
         wait_ft_cycles(12);
         wait_gpio_cycles(12);
         wait_ft_cycles(8);

         if (cmd_valid_pulses_n !== 1)
            fail("CMD_CLR_ALL_ERROR must generate exactly one cmd_valid pulse");
         if (dut.service_cmd_decoder.tx_fifo_error_ff !== 1'b0)
            fail("CMD_CLR_ALL_ERROR must clear host TX sticky error");
         if (dut.service_cmd_decoder.rx_fifo_error_ff !== 1'b0)
            fail("CMD_CLR_ALL_ERROR must clear host RX sticky error");
         if (dut.tx_guard.tx_fifo_error_wr_ff !== 1'b0)
            fail("CMD_CLR_ALL_ERROR must clear gpio TX guard sticky error");
         if (dut.normal_fifo_empty !== 1'b1)
            fail("CMD_CLR_ALL_ERROR must recover async TX FIFO to empty");
         if (dut.normal_fifo_full !== 1'b0)
            fail("CMD_CLR_ALL_ERROR must clear async TX FIFO full");
         if (dut.loop_fifo_empty !== 1'b1)
            fail("CMD_CLR_ALL_ERROR must recover loopback FIFO to empty");
         if (dut.loop_fifo_full !== 1'b0)
            fail("CMD_CLR_ALL_ERROR must clear loopback FIFO full");
         if (dut.ft601_fsm.state !== 5'b00001)
            fail("CMD_CLR_ALL_ERROR must return FSM to ARB");
         if (dut.ft601_fsm.rx_adapter.m_data_ff !== {DATA_LEN{1'b0}})
            fail("CMD_CLR_ALL_ERROR must clear FT RX capture state");
         if (dut.ft601_fsm.tx_adapter.lookahead_data_ff !== {DATA_LEN{1'b0}})
            fail("CMD_CLR_ALL_ERROR must clear FT TX lookahead state");
         if (dut.ft601_fsm.tx_adapter.data_ff !== {DATA_LEN{1'b0}})
            fail("CMD_CLR_ALL_ERROR must clear FT TX output state");
         if (dut.ft601_fsm.tx_adapter.word_valid_ff !== 1'b0)
            fail("CMD_CLR_ALL_ERROR must clear TX valid state");
         if (dut.ft601_fsm.tx_adapter.lookahead_valid_ff !== 1'b0)
            fail("CMD_CLR_ALL_ERROR must clear TX lookahead valid");
         if (dut.ft601_fsm.tx_adapter.s_fire_pending_ff !== 1'b0)
            fail("CMD_CLR_ALL_ERROR must clear TX request pending");
         if (dut.rx_stream_router.cmd_word_ff !== {FIFO_RX_LEN{1'b0}})
            fail("CMD_CLR_ALL_ERROR must clear captured FT RX word");
         if (dut.rx_stream_router.cmd_valid_ff !== 1'b0)
            fail("CMD_CLR_ALL_ERROR must clear FT RX valid flag");
         if (dut.rx_stream_router.loopback_payload_en_ff !== 1'b0)
            fail("CMD_CLR_ALL_ERROR must close loopback payload gate");
         if (dut.tx_recovery_clear_req_ft !== 1'b0)
            fail("CMD_CLR_ALL_ERROR TX recovery pulse must self-clear");
         if (dut.rx_recovery_clear_req_ft !== 1'b0)
            fail("CMD_CLR_ALL_ERROR RX recovery pulse must self-clear");
         if (dut.ft_state_clear_req_ft !== 1'b0)
            fail("CMD_CLR_ALL_ERROR FT-state recovery pulse must self-clear");
         if (dut.loopback_mode_ft !== 1'b1)
            fail("CMD_CLR_ALL_ERROR must not change current mode");

         release dut.tx_prefetch_en;
      end
   endtask

   task test_loopback_mode;
      begin
         if (dut.loopback_mode_ft !== 1'b1)
            fail("Loopback-mode test started while loopback mode is not active");
         $display("INFO: Starting FT601 loopback mode test");

         ft_txe_n = 1'b1;
         rx_payload_check_en = 1'b1;
         $display("INFO: TXE_N held high during FT601 receive phase");

         send_ft_command_frame(CMD_CLR_RX_ERROR);
         if (rx_words_n !== 0)
            fail("control frame must not increment loopback RX payload counter");
         if (cmd_valid_pulses_n !== 1)
            fail("one control frame must generate exactly one cmd_valid pulse");
         if (dut.loop_fifo_empty !== 1'b1)
            fail("control frame must not leave data inside loopback FIFO");
         wait_gpio_cycles(4);

         drive_ft_loopback_stream();
         $display("INFO: FT601 RX stimulus burst driven into DUT from data_p words");
         wait_for_ft_rx_idle();
         wait_ft_cycles(4);
         $display("INFO: DUT accepted payload words into loopback path, counted=%0d", rx_words_n);

         expect_no_tx_for_cycles(8);
         $display("INFO: Confirmed TX path stays idle while TXE_N is inactive");
         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         $display("INFO: TXE_N asserted low, FT601 may accept loopback data");

         fork
            wait_for_tx_words(exp_words_n, 16000);
            inject_txe_backpressure(8, 2);
         join
         $display("INFO: DUT returned %0d words back to FT601", tx_words_n);

         if (rx_active_cycles_n == 0)
            fail("RD_N never became active in loopback mode");
         if (oe_active_cycles_n == 0)
            fail("OE_N never became active in loopback mode");
         if (dut.loop_fifo_empty !== 1'b1)
            fail("Loopback FIFO is not empty after loopback transmission");
         rx_payload_check_en = 1'b0;
         check_payload_tx_latency("Loopback mode");
         check_payload_rx_latency("Loopback mode");
         $display("INFO: TX assert latency max=%0d FT clocks, release latency max=%0d FT clocks", tx_assert_cycles_max, tx_release_cycles_max);
         $display("INFO: RX assert latency max: RXF_N->OE_N=%0d FT clocks, RXF_N->RD_N=%0d FT clocks",
                  rx_oe_assert_cycles_max, rx_rd_assert_cycles_max);
         $display("INFO: RX release latency max: RXF_N->OE_N=%0d FT clocks, RXF_N->RD_N=%0d FT clocks",
                  rx_oe_release_cycles_max, rx_rd_release_cycles_max);
         $display("INFO: Loopback mode test passed, looped back %0d words", tx_words_n);
      end
   endtask

   task test_opcode_without_magic_is_payload;
      begin
         if (dut.loopback_mode_ft !== 1'b1)
            fail("opcode-without-magic test must start in loopback mode");
         if (dut.loop_fifo_empty !== 1'b1)
            fail("opcode-without-magic test requires empty loopback FIFO");

         $display("INFO: Checking that opcode without CMD_MAGIC is treated as loopback payload");
         cmd_valid_pulses_n = 0;
         drive_ft_single_word(CMD_SET_NORMAL, FULL_BE);
         wait_ft_cycles(4);

         if (cmd_valid_pulses_n !== 0)
            fail("opcode without CMD_MAGIC must not generate cmd_valid");
         if (dut.loopback_mode_ft !== 1'b1)
            fail("opcode without CMD_MAGIC must not switch mode");
         if (dut.loop_fifo_empty !== 1'b0)
            fail("opcode without CMD_MAGIC must be stored as loopback payload");
      end
   endtask

   task test_unknown_command_frame_ignored;
      begin
         if (dut.loopback_mode_ft !== 1'b1)
            fail("unknown-command-frame test must start in loopback mode");

         $display("INFO: Checking that unknown command frame is ignored");
         enter_normal_mode();
         enter_loopback_mode();
         cmd_valid_pulses_n = 0;
         send_ft_command_frame(CMD_UNKNOWN);
         wait_ft_cycles(4);

         if (cmd_valid_pulses_n !== 0)
            fail("unknown command frame must not generate cmd_valid");
         if (dut.loopback_mode_ft !== 1'b1)
            fail("unknown command frame must not switch mode");
         if (dut.loop_fifo_empty !== 1'b1)
            fail("unknown command frame must not be written into loopback FIFO");
      end
   endtask

   task test_mode_switch_normal_to_loopback;
      begin
         if (dut.loopback_mode_ft !== 1'b0)
            fail("normal-to-loopback switch test must start in normal mode");

         $display("INFO: Checking CMD_SET_LOOPBACK controlled switch with filled normal TX path");

         ft_txe_n = 1'b1;
         send_first_expected_gpio_words(4);
         wait_gpio_cycles(8);

         if (dut.normal_fifo_empty !== 1'b0)
            fail("normal-to-loopback switch test needs non-empty normal TX FIFO");

         cmd_valid_pulses_n = 0;
         enter_loopback_mode();

         if (cmd_valid_pulses_n !== 1)
            fail("CMD_SET_LOOPBACK must generate exactly one cmd_valid pulse");
         if (dut.normal_fifo_empty !== 1'b1)
            fail("CMD_SET_LOOPBACK must clear normal TX FIFO during mode switch");
         if (dut.normal_fifo_full !== 1'b0)
            fail("CMD_SET_LOOPBACK must clear normal TX FIFO full flag");

         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         expect_no_tx_for_cycles(12);
         @(posedge ft_clk);
         ft_set_txe_now(1'b1);
      end
   endtask

   task test_mode_switch_loopback_to_normal;
      begin
         if (dut.loopback_mode_ft !== 1'b1)
            fail("loopback-to-normal switch test must start in loopback mode");

         $display("INFO: Checking CMD_SET_NORMAL controlled switch with filled loopback path");

         ft_txe_n = 1'b1;
         dut.loopback_fifo.mem[0] = {4'hF, exp_words[0]};
         dut.loopback_fifo.mem[1] = {4'hF, exp_words[1]};
         dut.loopback_fifo.wr_ptr_ff = 14'd2;
         dut.loopback_fifo.rd_ptr_ff = 14'd0;
         dut.loopback_fifo.data_ff = {4'hF, exp_words[0]};
         dut.loopback_fifo.empty_ff = 1'b0;
         dut.loopback_fifo.full_ff = 1'b0;

         cmd_valid_pulses_n = 0;
         enter_normal_mode();

         if (cmd_valid_pulses_n !== 1)
            fail("CMD_SET_NORMAL must generate exactly one cmd_valid pulse");
         if (dut.loop_fifo_empty !== 1'b1)
            fail("CMD_SET_NORMAL must clear loopback FIFO during mode switch");
         if (dut.loop_fifo_full !== 1'b0)
            fail("CMD_SET_NORMAL must clear loopback FIFO full flag");

         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         expect_no_tx_for_cycles(12);
         @(posedge ft_clk);
         ft_set_txe_now(1'b1);

         clear_monitors();
         test_gpio_mode();
      end
   endtask

   task test_mode_switch_after_recent_tx_burst;
      integer tx_words_before_switch;
      begin
         if (dut.loopback_mode_ft !== 1'b0)
            fail("recent-TX switch test must start in normal mode");

         $display("INFO: Checking CMD_SET_LOOPBACK after recently active FT TX burst");

         clear_monitors();
         ft_txe_n = 1'b1;
         send_first_expected_gpio_words(8);
         wait_gpio_cycles(12);

         if (dut.normal_fifo_empty !== 1'b0)
            fail("recent-TX switch test needs pending normal TX FIFO data");

         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         wait_for_tx_words(2, 1200);
         @(posedge ft_clk);
         ft_set_txe_now(1'b1);
         wait_ft_cycles(4);

         if (dut.normal_fifo_empty !== 1'b0)
            fail("recent-TX switch test needs remaining normal TX data before switch");

         tx_words_before_switch = tx_words_n;
         cmd_valid_pulses_n = 0;
         enter_loopback_mode();

         if (cmd_valid_pulses_n !== 1)
            fail("recent-TX CMD_SET_LOOPBACK must generate exactly one cmd_valid pulse");
         if (dut.normal_fifo_empty !== 1'b1)
            fail("CMD_SET_LOOPBACK must clear remaining normal TX data after recent TX burst");

         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         expect_no_tx_for_cycles(12);
         if (tx_words_n !== tx_words_before_switch)
            fail("old normal-mode TX data leaked after switch to loopback");
         @(posedge ft_clk);
         ft_set_txe_now(1'b1);
      end
   endtask

   task expect_status_frame(
      input [DATA_LEN-1:0] expected_word
   );
      begin
         if (tx_total_words_n !== 2)
            fail("exactly two TX words are expected for a pure status response");
         expect_status_frame_at(0, expected_word);
      end
   endtask

   task test_get_status_normal_mode;
      reg [DATA_LEN-1:0] expected_status;
      begin
         if (dut.loopback_mode_ft !== 1'b0)
            fail("normal-mode status test must start in normal mode");

         $display("INFO: Checking CMD_GET_STATUS in normal mode");
         expected_status = build_status_word(1'b0, 1'b0, 1'b0, 1'b1, 1'b0, 1'b1, 1'b0);

         cmd_valid_pulses_n = 0;
         tx_stream_only_mode = 1'b1;
         send_ft_command_frame(CMD_GET_STATUS);
         if (cmd_valid_pulses_n !== 1)
            fail("CMD_GET_STATUS must generate exactly one cmd_valid pulse");

         expect_no_tx_for_cycles(8);
         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         wait_for_tx_total_words(2, 256);
         expect_status_frame(expected_status);
         @(posedge ft_clk);
         ft_set_txe_now(1'b1);
         wait_for_ft_tx_idle();
         tx_stream_only_mode = 1'b0;
      end
   endtask

   task test_get_status_loopback_mode;
      reg [DATA_LEN-1:0] expected_status;
      begin
         if (dut.loopback_mode_ft !== 1'b1)
            fail("loopback-mode status test must start in loopback mode");

         $display("INFO: Checking CMD_GET_STATUS in loopback mode");
         expected_status = build_status_word(1'b1, 1'b0, 1'b0, 1'b1, 1'b0, 1'b1, 1'b0);

         cmd_valid_pulses_n = 0;
         tx_stream_only_mode = 1'b1;
         send_ft_command_frame(CMD_GET_STATUS);
         if (cmd_valid_pulses_n !== 1)
            fail("CMD_GET_STATUS must generate exactly one cmd_valid pulse in loopback mode");

         expect_no_tx_for_cycles(8);
         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         wait_for_tx_total_words(2, 256);
         expect_status_frame(expected_status);
         @(posedge ft_clk);
         ft_set_txe_now(1'b1);
         tx_stream_only_mode = 1'b0;
      end
   endtask

   task test_get_status_after_clear;
      reg [DATA_LEN-1:0] expected_status;
      begin
         if (dut.loopback_mode_ft !== 1'b0)
            fail("status-after-clear test must start in normal mode");

         $display("INFO: Checking CMD_GET_STATUS after injected errors and after CMD_CLR_ALL_ERROR");

         dut.tx_guard.tx_fifo_error_wr_ff = 1'b1;
         dut.service_cmd_decoder.rx_fifo_error_ff = 1'b1;
         wait_ft_cycles(2);
         wait_gpio_cycles(2);

         expected_status = build_status_word(1'b0, 1'b1, 1'b1, 1'b1, 1'b0, 1'b1, 1'b0);
         cmd_valid_pulses_n = 0;
         tx_stream_only_mode = 1'b1;
         send_ft_command_frame(CMD_GET_STATUS);
         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         wait_for_tx_total_words(2, 256);
         expect_status_frame(expected_status);
         @(posedge ft_clk);
         ft_set_txe_now(1'b1);

         send_ft_command_frame(CMD_CLR_ALL_ERROR);
         wait_ft_cycles(8);
         wait_gpio_cycles(8);

         if (dut.tx_guard.tx_fifo_error_wr_ff !== 1'b0)
            fail("CMD_CLR_ALL_ERROR must clear tx_write_guard fault source");
         if (dut.service_cmd_decoder.rx_fifo_error_ff !== 1'b0)
            fail("CMD_CLR_ALL_ERROR must clear RX diagnostic sticky state");

         clear_monitors();
         expected_status = build_status_word(1'b0, 1'b0, 1'b0, 1'b1, 1'b0, 1'b1, 1'b0);
         tx_stream_only_mode = 1'b1;
         send_ft_command_frame(CMD_GET_STATUS);
         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         wait_for_tx_total_words(2, 256);
         expect_status_frame(expected_status);
         @(posedge ft_clk);
         ft_set_txe_now(1'b1);
         tx_stream_only_mode = 1'b0;
      end
   endtask

   task test_get_status_with_pending_normal_tx;
      reg [DATA_LEN-1:0] expected_status;
      begin
         if (dut.loopback_mode_ft !== 1'b0)
            fail("status-with-pending-TX test must start in normal mode");

         $display("INFO: Checking CMD_GET_STATUS near pending normal TX payload without data loss");

         ft_txe_n = 1'b1;
         send_first_expected_gpio_words(4);
         wait_gpio_cycles(8);
         wait_ft_cycles(8);

         if (dut.normal_fifo_empty !== 1'b0)
            fail("status-with-pending-TX test needs non-empty normal TX FIFO");

         expected_status = build_status_word(1'b0, 1'b0, 1'b0, 1'b0, 1'b0, 1'b1, 1'b0);
         cmd_valid_pulses_n = 0;
         tx_stream_only_mode = 1'b1;
         send_ft_command_frame(CMD_GET_STATUS);
         if (cmd_valid_pulses_n !== 1)
            fail("CMD_GET_STATUS must generate exactly one cmd_valid pulse with pending TX payload");

         expect_no_tx_for_cycles(8);
         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         wait_for_tx_total_words(3, 1200);
         expect_status_frame_at(0, expected_status);
         if (tx_captured_be[2] !== FULL_BE)
            fail("loopback payload after status must keep full BE");
         if (tx_captured_words[2] === STATUS_MAGIC)
            fail("loopback payload must not repeat STATUS_MAGIC after the status frame");
         @(posedge ft_clk);
         ft_set_txe_now(1'b1);
         tx_stream_only_mode = 1'b0;
      end
   endtask

   task test_get_status_with_pending_loopback_tx;
      reg [DATA_LEN-1:0] expected_status;
      begin
         if (dut.loopback_mode_ft !== 1'b1)
            fail("status-with-pending-loopback test must start in loopback mode");

         $display("INFO: Checking CMD_GET_STATUS with pending loopback payload before TX burst");

         ft_txe_n = 1'b1;
         drive_first_expected_loopback_words(4);
         wait_ft_cycles(8);

         if (dut.loop_fifo_empty !== 1'b0)
            fail("status-with-pending-loopback test needs non-empty loopback FIFO");

         expected_status = build_status_word(1'b1, 1'b0, 1'b0, 1'b1, 1'b0, 1'b0, 1'b0);
         cmd_valid_pulses_n = 0;
         tx_stream_only_mode = 1'b1;
         send_ft_command_frame(CMD_GET_STATUS);
         if (cmd_valid_pulses_n !== 1)
            fail("CMD_GET_STATUS must generate exactly one cmd_valid pulse with pending loopback payload");

         expect_no_tx_for_cycles(8);
         @(posedge ft_clk);
         ft_set_txe_now(1'b0);
         wait_for_tx_total_words(6, 1200);
         expect_status_frame_at(0, expected_status);
         expect_captured_tx_word(2, exp_words[0], FULL_BE);
         expect_captured_tx_word(3, exp_words[1], FULL_BE);
         expect_captured_tx_word(4, exp_words[2], FULL_BE);
         expect_captured_tx_word(5, exp_words[3], FULL_BE);
         @(posedge ft_clk);
         ft_set_txe_now(1'b1);
         wait_for_ft_tx_idle();
         tx_stream_only_mode = 1'b0;
      end
   endtask

   // Capture FT601 pins after the DUT posedge output boundary has settled.
   always @(posedge ft_clk or negedge ft_reset_n) begin
      if (!ft_reset_n) begin
         tx_words_n <= 0;
         rx_words_n <= 0;
         rx_active_cycles_n <= 0;
         oe_active_cycles_n <= 0;
         rx_burst_seen <= 1'b0;
         tx_burst_seen <= 1'b0;
         tx_payload_burst_seen <= 1'b0;
         prev_ft_oe_n  <= 1'b1;
         prev_ft_rd_n  <= 1'b1;
         prev_ft_wr_n  <= 1'b1;
         prev_ft_txe_n_neg <= 1'b1;
         prev_ft_rxf_n <= 1'b1;
         allow_tx_burst_split <= 1'b0;
         rx_expect_rd_after_oe       <= 1'b0;
         rx_expect_release_after_rxf <= 1'b0;
      end
      else begin
         #TB_POSEDGE_SAMPLE_DELAY;

         if (dut.service_cmd_decoder.cmd_valid)
            cmd_valid_pulses_n <= cmd_valid_pulses_n + 1;

         if (rx_expect_rd_after_oe) begin
            if (ft_rd_n === 1'b0)
               rx_expect_rd_after_oe <= 1'b0;
         end

         if (rx_expect_release_after_rxf) begin
            if ((ft_oe_n === 1'b1) && (ft_rd_n === 1'b1))
               rx_expect_release_after_rxf <= 1'b0;
         end

         if (prev_ft_oe_n && !ft_oe_n) begin
            if (ft_rd_n !== 1'b1)
               fail("RD_N must stay inactive in the cycle OE_N first asserts");
            rx_expect_rd_after_oe <= 1'b1;
         end

         if (prev_ft_rd_n && !ft_rd_n) begin
            if ((prev_ft_oe_n !== 1'b0) || (ft_oe_n !== 1'b0))
               fail("RD_N must assert only after OE_N is already active");
         end

         if (!prev_ft_rxf_n && ft_rxf_n && ((!ft_oe_n) || (!ft_rd_n)))
            rx_expect_release_after_rxf <= 1'b1;

         if (dut.fifo_append) begin
            if (!rx_burst_seen) begin
               if (TB_VERBOSE_STREAM)
                  $display("INFO: FT601 RX burst started");
               rx_burst_seen <= 1'b1;
            end
            if (rx_payload_check_en && dut.loop_fifo_wen && (rx_words_n < 2))
               if (TB_VERBOSE_STREAM)
                  $display("INFO: RX sample[%0d] data=%h be=%h", rx_words_n, dut.loop_fifo_wdata[DATA_LEN-1:0], dut.loop_fifo_wdata[FIFO_RX_LEN-1:DATA_LEN]);
            if (rx_payload_check_en && dut.loop_fifo_wen) begin
               expect_rx_word(rx_words_n, dut.loop_fifo_wdata[DATA_LEN-1:0], dut.loop_fifo_wdata[FIFO_RX_LEN-1:DATA_LEN]);
               rx_words_n <= rx_words_n + 1;
            end
         end

         if (!ft_wr_n) begin
            if (host_drive_en)
               fail("DATA/BE bus contention during FT601 TX");
            if (!tx_burst_seen) begin
               if (TB_VERBOSE_STREAM)
                  $display("INFO: FT601 TX burst started");
               tx_burst_seen <= 1'b1;
            end
            if (tx_total_words_n < TX_CAPTURE_WORDS_MAX) begin
               tx_captured_words[tx_total_words_n] <= ft_data_bus;
               tx_captured_be[tx_total_words_n] <= ft_be_bus;
            end
            tx_total_words_n <= tx_total_words_n + 1;

            if (tx_stream_only_mode) begin
               tx_payload_burst_seen <= 1'b0;
               if (TB_VERBOSE_STREAM)
                  $display("INFO: TX stream[%0d] data=%h be=%h", tx_total_words_n, ft_data_bus, ft_be_bus);
            end
            else begin
               tx_payload_burst_seen <= 1'b1;
               if (tx_words_n < 2) begin
                  if (TB_VERBOSE_STREAM)
                     $display("INFO: TX sample[%0d] data=%h be=%h", tx_words_n, ft_data_bus, ft_be_bus);
               end
               expect_tx_word(tx_words_n, ft_data_bus, ft_be_bus);
               tx_words_n <= tx_words_n + 1;
            end
         end
         else if (!allow_tx_burst_split && tx_payload_burst_seen && !prev_ft_txe_n_neg && !ft_txe_n && !prev_ft_wr_n && (tx_words_n < exp_words_n)) begin
            fail("WR_N must stay active for a continuous TX burst while TXE_N is low");
         end
         else if (prev_ft_wr_n == 1'b0) begin
            tx_payload_burst_seen <= 1'b0;
         end

         if (!ft_rd_n)
            rx_active_cycles_n <= rx_active_cycles_n + 1;
         if (!ft_oe_n)
            oe_active_cycles_n <= oe_active_cycles_n + 1;

         prev_ft_oe_n  <= ft_oe_n;
         prev_ft_rd_n  <= ft_rd_n;
         prev_ft_wr_n  <= ft_wr_n;
         prev_ft_txe_n_neg <= ft_txe_n;
         prev_ft_rxf_n <= ft_rxf_n;
      end
   end

   always @(posedge ft_clk or negedge ft_reset_n) begin
      if (!ft_reset_n) begin
         prev_ft_rxf_n_pos  <= 1'b1;
         rx_oe_assert_wait  <= 1'b0;
         rx_rd_assert_wait  <= 1'b0;
         rx_oe_release_wait <= 1'b0;
         rx_rd_release_wait <= 1'b0;
         rx_oe_assert_cycles_cur <= 0;
         rx_rd_assert_cycles_cur <= 0;
         rx_oe_release_cycles_cur <= 0;
         rx_rd_release_cycles_cur <= 0;
         rx_oe_assert_cycles_last <= 0;
         rx_rd_assert_cycles_last <= 0;
         rx_oe_release_cycles_last <= 0;
         rx_rd_release_cycles_last <= 0;
         rx_oe_assert_cycles_max <= 0;
         rx_rd_assert_cycles_max <= 0;
         rx_oe_release_cycles_max <= 0;
         rx_rd_release_cycles_max <= 0;
         rx_oe_assert_events_n <= 0;
         rx_rd_assert_events_n <= 0;
         rx_oe_release_events_n <= 0;
         rx_rd_release_events_n <= 0;
         prev_ft_txe_n_pos  <= 1'b1;
         tx_assert_wait     <= 1'b0;
         tx_release_wait    <= 1'b0;
         tx_assert_cycles_cur <= 0;
         tx_release_cycles_cur <= 0;
         tx_assert_cycles_last <= 0;
         tx_release_cycles_last <= 0;
         tx_assert_cycles_max  <= 0;
         tx_release_cycles_max <= 0;
         tx_assert_events_n    <= 0;
         tx_release_events_n   <= 0;
      end
      else begin
         #TB_POSEDGE_SAMPLE_DELAY;

         if (rx_oe_assert_wait) begin
            if (!ft_oe_n) begin
               rx_oe_assert_wait <= 1'b0;
               rx_oe_assert_cycles_last <= rx_oe_assert_cycles_cur;
               rx_oe_assert_events_n <= rx_oe_assert_events_n + 1;
               if (rx_oe_assert_cycles_cur > rx_oe_assert_cycles_max)
                  rx_oe_assert_cycles_max <= rx_oe_assert_cycles_cur;
               if ((exp_rx_oe_assert_cycles >= 0) && (rx_oe_assert_cycles_cur !== exp_rx_oe_assert_cycles))
                  fail("RXF_N active to OE_N active latency mismatch");
               if (TB_VERBOSE_LATENCY)
                  $display("INFO: RXF_N active to OE_N active latency = %0d FT clocks", rx_oe_assert_cycles_cur);
            end
            else
               rx_oe_assert_cycles_cur <= rx_oe_assert_cycles_cur + 1;
         end

         if (rx_rd_assert_wait) begin
            if (!ft_rd_n) begin
               rx_rd_assert_wait <= 1'b0;
               rx_rd_assert_cycles_last <= rx_rd_assert_cycles_cur;
               rx_rd_assert_events_n <= rx_rd_assert_events_n + 1;
               if (rx_rd_assert_cycles_cur > rx_rd_assert_cycles_max)
                  rx_rd_assert_cycles_max <= rx_rd_assert_cycles_cur;
               if ((exp_rx_rd_assert_cycles >= 0) && (rx_rd_assert_cycles_cur !== exp_rx_rd_assert_cycles))
                  fail("RXF_N active to RD_N active latency mismatch");
               if (TB_VERBOSE_LATENCY)
                  $display("INFO: RXF_N active to RD_N active latency = %0d FT clocks", rx_rd_assert_cycles_cur);
            end
            else
               rx_rd_assert_cycles_cur <= rx_rd_assert_cycles_cur + 1;
         end

         if (rx_oe_release_wait) begin
            if (ft_oe_n) begin
               rx_oe_release_wait <= 1'b0;
               rx_oe_release_cycles_last <= rx_oe_release_cycles_cur;
               rx_oe_release_events_n <= rx_oe_release_events_n + 1;
               if (rx_oe_release_cycles_cur > rx_oe_release_cycles_max)
                  rx_oe_release_cycles_max <= rx_oe_release_cycles_cur;
               if ((exp_rx_oe_release_cycles >= 0) && (rx_oe_release_cycles_cur !== exp_rx_oe_release_cycles))
                  fail("RXF_N inactive to OE_N inactive latency mismatch");
               if (TB_VERBOSE_LATENCY)
                  $display("INFO: RXF_N inactive to OE_N inactive latency = %0d FT clocks", rx_oe_release_cycles_cur);
            end
            else
               rx_oe_release_cycles_cur <= rx_oe_release_cycles_cur + 1;
         end

         if (rx_rd_release_wait) begin
            if (ft_rd_n) begin
               rx_rd_release_wait <= 1'b0;
               rx_rd_release_cycles_last <= rx_rd_release_cycles_cur;
               rx_rd_release_events_n <= rx_rd_release_events_n + 1;
               if (rx_rd_release_cycles_cur > rx_rd_release_cycles_max)
                  rx_rd_release_cycles_max <= rx_rd_release_cycles_cur;
               if ((exp_rx_rd_release_cycles >= 0) && (rx_rd_release_cycles_cur !== exp_rx_rd_release_cycles))
                  fail("RXF_N inactive to RD_N inactive latency mismatch");
               if (TB_VERBOSE_LATENCY)
                  $display("INFO: RXF_N inactive to RD_N inactive latency = %0d FT clocks", rx_rd_release_cycles_cur);
            end
            else
               rx_rd_release_cycles_cur <= rx_rd_release_cycles_cur + 1;
         end

         if (tx_assert_wait) begin
            if (!ft_wr_n) begin
               tx_assert_wait       <= 1'b0;
               tx_assert_cycles_last <= tx_assert_cycles_cur;
               tx_assert_events_n   <= tx_assert_events_n + 1;
               if (tx_assert_cycles_cur > tx_assert_cycles_max)
                  tx_assert_cycles_max <= tx_assert_cycles_cur;
               if ((exp_tx_assert_cycles >= 0) && (tx_assert_cycles_cur !== exp_tx_assert_cycles))
                  fail("TXE_N active to WR_N active latency mismatch");
               if (TB_VERBOSE_LATENCY)
                  $display("INFO: TXE_N active to WR_N active latency = %0d FT clocks", tx_assert_cycles_cur);
            end
            else
               tx_assert_cycles_cur <= tx_assert_cycles_cur + 1;
         end

         if (tx_release_wait) begin
            if (ft_wr_n) begin
               tx_release_wait        <= 1'b0;
               tx_release_cycles_last <= tx_release_cycles_cur;
               tx_release_events_n    <= tx_release_events_n + 1;
               if (tx_release_cycles_cur > tx_release_cycles_max)
                  tx_release_cycles_max <= tx_release_cycles_cur;
               if ((exp_tx_release_cycles >= 0) && (tx_release_cycles_cur !== exp_tx_release_cycles))
                  fail("TXE_N inactive to WR_N inactive latency mismatch");
               if (TB_VERBOSE_LATENCY)
                  $display("INFO: TXE_N inactive to WR_N inactive latency = %0d FT clocks", tx_release_cycles_cur);
            end
            else
               tx_release_cycles_cur <= tx_release_cycles_cur + 1;
         end

         if (prev_ft_txe_n_pos && !ft_txe_n) begin
            if (ft_wr_n) begin
               tx_assert_wait      <= 1'b1;
               tx_assert_cycles_cur <= 1;
            end
            else begin
               tx_assert_cycles_last <= 1;
               tx_assert_events_n    <= tx_assert_events_n + 1;
               if (tx_assert_cycles_max < 1)
                  tx_assert_cycles_max <= 1;
               if ((exp_tx_assert_cycles >= 0) && (1 !== exp_tx_assert_cycles))
                  fail("TXE_N active to WR_N active latency mismatch");
               if (TB_VERBOSE_LATENCY)
                  $display("INFO: TXE_N active to WR_N active latency = 1 FT clocks");
            end
         end

         if (!prev_ft_txe_n_pos && ft_txe_n) begin
            if (!ft_wr_n) begin
               tx_release_wait      <= 1'b1;
               tx_release_cycles_cur <= 1;
            end
            else begin
               tx_release_cycles_last <= 1;
               tx_release_events_n    <= tx_release_events_n + 1;
               if (tx_release_cycles_max < 1)
                  tx_release_cycles_max <= 1;
               if ((exp_tx_release_cycles >= 0) && (1 !== exp_tx_release_cycles))
                  fail("TXE_N inactive to WR_N inactive latency mismatch");
               if (TB_VERBOSE_LATENCY)
                  $display("INFO: TXE_N inactive to WR_N inactive latency = 1 FT clocks");
            end
         end

         if (prev_ft_rxf_n_pos && !ft_rxf_n) begin
            if (ft_oe_n) begin
               rx_oe_assert_wait <= 1'b1;
               rx_oe_assert_cycles_cur <= 1;
            end
            else begin
               rx_oe_assert_cycles_last <= 1;
               rx_oe_assert_events_n <= rx_oe_assert_events_n + 1;
               if (rx_oe_assert_cycles_max < 1)
                  rx_oe_assert_cycles_max <= 1;
               if ((exp_rx_oe_assert_cycles >= 0) && (1 !== exp_rx_oe_assert_cycles))
                  fail("RXF_N active to OE_N active latency mismatch");
               if (TB_VERBOSE_LATENCY)
                  $display("INFO: RXF_N active to OE_N active latency = 1 FT clocks");
            end

            if (ft_rd_n) begin
               rx_rd_assert_wait <= 1'b1;
               rx_rd_assert_cycles_cur <= 1;
            end
            else begin
               rx_rd_assert_cycles_last <= 1;
               rx_rd_assert_events_n <= rx_rd_assert_events_n + 1;
               if (rx_rd_assert_cycles_max < 1)
                  rx_rd_assert_cycles_max <= 1;
               if ((exp_rx_rd_assert_cycles >= 0) && (1 !== exp_rx_rd_assert_cycles))
                  fail("RXF_N active to RD_N active latency mismatch");
               if (TB_VERBOSE_LATENCY)
                  $display("INFO: RXF_N active to RD_N active latency = 1 FT clocks");
            end
         end

         if (!prev_ft_rxf_n_pos && ft_rxf_n) begin
            if (!ft_oe_n) begin
               rx_oe_release_wait <= 1'b1;
               rx_oe_release_cycles_cur <= 1;
            end
            else begin
               rx_oe_release_cycles_last <= 1;
               rx_oe_release_events_n <= rx_oe_release_events_n + 1;
               if (rx_oe_release_cycles_max < 1)
                  rx_oe_release_cycles_max <= 1;
               if ((exp_rx_oe_release_cycles >= 0) && (1 !== exp_rx_oe_release_cycles))
                  fail("RXF_N inactive to OE_N inactive latency mismatch");
               if (TB_VERBOSE_LATENCY)
                  $display("INFO: RXF_N inactive to OE_N inactive latency = 1 FT clocks");
            end

            if (!ft_rd_n) begin
               rx_rd_release_wait <= 1'b1;
               rx_rd_release_cycles_cur <= 1;
            end
            else begin
               rx_rd_release_cycles_last <= 1;
               rx_rd_release_events_n <= rx_rd_release_events_n + 1;
               if (rx_rd_release_cycles_max < 1)
                  rx_rd_release_cycles_max <= 1;
               if ((exp_rx_rd_release_cycles >= 0) && (1 !== exp_rx_rd_release_cycles))
                  fail("RXF_N inactive to RD_N inactive latency mismatch");
               if (TB_VERBOSE_LATENCY)
                  $display("INFO: RXF_N inactive to RD_N inactive latency = 1 FT clocks");
            end
         end

         prev_ft_rxf_n_pos <= ft_rxf_n;
         prev_ft_txe_n_pos <= ft_txe_n;
      end
   end

   initial begin
      $display("INFO: Testbench start. Universal bitstream mode");
      load_vectors();
      build_expected_words();

      tb_reset();
      clear_monitors();
      scenario_start("test_tx_recovery_clear_path");
      test_tx_recovery_clear_path();
      scenario_end("test_tx_recovery_clear_path");

      tb_reset();
      clear_monitors();
      if (dut.loopback_mode_ft !== 1'b0)
         fail("Design must start in normal mode after reset");

      scenario_start("test_rx_recovery_clear_path");
      test_rx_recovery_clear_path();
      scenario_end("test_rx_recovery_clear_path");
      scenario_start("test_ft_state_clear_rx");
      test_ft_state_clear_rx();
      scenario_end("test_ft_state_clear_rx");

      scenario_start("test_ft_state_clear_tx");
      test_ft_state_clear_tx();
      scenario_end("test_ft_state_clear_tx");

      tb_reset();
      clear_monitors();
      if (dut.loopback_mode_ft !== 1'b0)
         fail("Design must stay in normal mode before CMD_CLR_TX_ERROR recovery test");
      scenario_start("test_cmd_clear_tx_recovery");
      test_cmd_clear_tx_recovery();
      scenario_end("test_cmd_clear_tx_recovery");

      tb_reset();
      clear_monitors();
      scenario_start("test_cmd_reset_ft_state_keeps_tx_fifo");
      test_cmd_reset_ft_state_keeps_tx_fifo();
      scenario_end("test_cmd_reset_ft_state_keeps_tx_fifo");

      tb_reset();
      clear_monitors();
      scenario_start("test_gpio_mode");
      test_gpio_mode();
      scenario_end("test_gpio_mode");

      @(posedge ft_clk);
      ft_set_txe_now(1'b1);
      wait_ft_cycles(4);

      clear_monitors();
      enter_loopback_mode();
      scenario_start("test_cmd_clear_rx_recovery");
      test_cmd_clear_rx_recovery();
      scenario_end("test_cmd_clear_rx_recovery");
      scenario_start("test_cmd_clear_all_recovery");
      test_cmd_clear_all_recovery();
      scenario_end("test_cmd_clear_all_recovery");

      tb_reset();
      clear_monitors();
      scenario_start("test_mode_switch_normal_to_loopback");
      test_mode_switch_normal_to_loopback();
      scenario_end("test_mode_switch_normal_to_loopback");
      scenario_start("test_mode_switch_loopback_to_normal");
      test_mode_switch_loopback_to_normal();
      scenario_end("test_mode_switch_loopback_to_normal");

      tb_reset();
      clear_monitors();
      scenario_start("test_mode_switch_after_recent_tx_burst");
      test_mode_switch_after_recent_tx_burst();
      scenario_end("test_mode_switch_after_recent_tx_burst");

      tb_reset();
      clear_monitors();
      scenario_start("test_get_status_normal_mode");
      test_get_status_normal_mode();
      scenario_end("test_get_status_normal_mode");

      tb_reset();
      clear_monitors();
      scenario_start("test_get_status_after_clear");
      test_get_status_after_clear();
      scenario_end("test_get_status_after_clear");

      tb_reset();
      clear_monitors();
      scenario_start("test_get_status_with_pending_normal_tx");
      test_get_status_with_pending_normal_tx();
      scenario_end("test_get_status_with_pending_normal_tx");

      tb_reset();
      clear_monitors();
      enter_loopback_mode();
      clear_monitors();
      scenario_start("test_get_status_loopback_mode");
      test_get_status_loopback_mode();
      scenario_end("test_get_status_loopback_mode");
      clear_monitors();
      scenario_start("test_get_status_with_pending_loopback_tx");
      test_get_status_with_pending_loopback_tx();
      scenario_end("test_get_status_with_pending_loopback_tx");
      clear_monitors();
      scenario_start("test_opcode_without_magic_is_payload");
      test_opcode_without_magic_is_payload();
      scenario_end("test_opcode_without_magic_is_payload");
      enter_normal_mode();
      enter_loopback_mode();
      clear_monitors();
      scenario_start("test_unknown_command_frame_ignored");
      test_unknown_command_frame_ignored();
      scenario_end("test_unknown_command_frame_ignored");
      clear_monitors();
      scenario_start("test_loopback_mode");
      test_loopback_mode();
      scenario_end("test_loopback_mode");

      scenario_start("test_fpga_reset_drives_ft601_reset");
      test_fpga_reset_drives_ft601_reset();
      scenario_end("test_fpga_reset_drives_ft601_reset");

      enter_loopback_mode();

      scenario_start("pulse_fpga_reset_only");
      pulse_fpga_reset_only();
      scenario_end("pulse_fpga_reset_only");

      $display("TEST PASSED. Universal bitstream flow verified, words=%0d", exp_words_n);
      $finish;
   end

   initial begin
      $dumpfile("testbench.vcd");
      $dumpvars(0, testbench);
   end

endmodule
