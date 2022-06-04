`timescale 1ns / 1ps

module row_hist_buffer #(
    parameter CK_PERIOD = 500, // clock period in picoseconds -> for timestamping
    parameter tAI       = 473010, // tAI in picoseconds
    parameter N         = 12
    )
  (
  input         clk,
  input         rst,
  input [15:0]  row_addr,
  input         insert_valid,
  output        match
  );
  
  localparam nCK_PER_tAI          = tAI/CK_PERIOD;
  localparam TIMESTAMP_WIDTH      = $clog2(nCK_PER_tAI) + 1;
  localparam ts_offset            = TIMESTAMP_WIDTH + 1;
  
  reg [3:0] head_ptr_r, head_ptr_ns;
  reg [3:0] tail_ptr_r, tail_ptr_ns;
  
  reg [TIMESTAMP_WIDTH-1:0] stamp_r;
  
  reg [15:0]                  buf_addr_r[N-1:0], buf_addr_ns[N-1:0];
  reg [TIMESTAMP_WIDTH-1:0]   buf_ts_r[N-1:0], buf_ts_ns[N-1:0];
  reg                         buf_valid_r[N-1:0], buf_valid_ns[N-1:0];
  
  integer i; // for verilog vector magic

  // intermediate values for retire logic
  reg                       head_is_stale     = 0;
  reg[TIMESTAMP_WIDTH-1:0]  head_time_diff    = 0;
  
  // intermediate values for match logic
  reg[N-1:0]                match_vec         = 0;
  reg                       entry_match       = 0;
  
  always @* begin
    // ns init
    head_ptr_ns       = head_ptr_r;
    tail_ptr_ns       = tail_ptr_r;
    match_vec         = 0;
    for(i=0 ; i<N ; i=i+1) begin
      buf_addr_ns[i]  = buf_addr_r[i];
      buf_ts_ns[i]    = buf_ts_r[i];
      buf_valid_ns[i] = buf_valid_r[i];
    end
    // head removal logic
    head_time_diff  = stamp_r - buf_ts_r[head_ptr_r];
    head_is_stale   = (head_time_diff > nCK_PER_tAI) && buf_valid_r[head_ptr_r]; // TODO might not need buf_valid signal here
    if(head_is_stale) begin
      head_ptr_ns = head_ptr_r + 1;    // incr. head
      buf_valid_ns[head_ptr_r] = 1'b0; // invalidate head
    end
    // insertion
    if(insert_valid) begin
      tail_ptr_ns = tail_ptr_r + 1;
      buf_valid_ns[tail_ptr_r] = 1'b1;
      buf_addr_ns[tail_ptr_r]  = row_addr;
      buf_ts_ns[tail_ptr_r]    = stamp_r;
    end
    // match logic
    for(i=0 ; i<N ; i=i+1)
      match_vec[i]    = buf_valid_r[i] && (buf_addr_r[i] == row_addr);
    
    entry_match = |match_vec;
  end
  
  assign match = entry_match;

  always @(posedge clk) begin
    if(rst) begin
      head_ptr_r <= 0;
      tail_ptr_r <= 0;
      for(i=0 ; i<N ; i=i+1) begin
        buf_addr_r[i]  <= buf_addr_ns[i];
        buf_ts_r[i]    <= buf_ts_ns[i];
        buf_valid_r[i] <= buf_valid_ns[i];
      end
      stamp_r    <= 0;
    end
    else begin
      head_ptr_r <= head_ptr_ns;
      tail_ptr_r <= tail_ptr_ns;
      for(i=0 ; i<N ; i=i+1) begin
        buf_addr_r[i]   <= buf_addr_ns[i];
        buf_ts_r[i]     <= buf_ts_ns[i];
        buf_valid_r[i]  <= buf_valid_ns[i];
      end
      stamp_r    <= stamp_r + 1;
    end
  end
  
endmodule
