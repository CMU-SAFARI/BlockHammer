`timescale 1ns / 1ps

module bloom_filters(
  input         clk,
  input         rst,
  input [15:0]  row_addr,
  input [2:0]   core_id,
  input         insert_valid,
  output        aggressor,
  output        perf_attack
  );
  
  integer i;
  
  localparam COUNTER_THRESHOLD = 4096; // set this accordingly?
  localparam NBFOVER2          = 32*1024; // set this accordingly?
  localparam ACT_CTR_SIZE      = $clog2(NBFOVER2+1);
  
  // 15 bit LFSR that will loop over all 2^15-1 possible values (pseudo-random sequence)
  // we assign XNOR tap indices according to the Xilinx app. note:
  // https://www.xilinx.com/support/documentation/application_notes/xapp210.pdf
  reg   [15:0]              lfsr1_r, lfsr1_ns;
  reg   [15:0]              lfsr2_r, lfsr2_ns;

  reg                       active_filter_r, active_filter_ns;
  reg   [ACT_CTR_SIZE-1:0]  activate_ctr_r, activate_ctr_ns;
  
  reg   [9:0]   hash_preseed       [7:0]; 
  reg   [9:0]   hash1              [7:0]; 
  reg   [9:0]   hash2              [7:0];
  
  reg [15:0] bf1_ns [1023:0], bf1_r [1023:0];
  reg [15:0] bf2_ns [1023:0], bf2_r [1023:0];
  
  //match logic stuff
  reg [15:0] comp_reduce1 [7:0];
  reg [15:0] comp_reduce2 [7:0];
  reg [7:0]  comp_vector1;
  reg [7:0]  comp_vector2;
  // if every counter value is greater than X
  assign aggressor    = active_filter_r ? &comp_vector1[3:0] : &comp_vector2[3:0]; 
  assign perf_attack  = active_filter_r ? &comp_vector1[7:4] : &comp_vector2[7:4]; 
  
  always @* begin
    // init elements
    for(i=0 ; i<4 ; i=i+1) begin
      comp_reduce1[i] = 0;
      comp_reduce2[i] = 0;
    end
    active_filter_ns = active_filter_r;
    activate_ctr_ns  = insert_valid ? activate_ctr_r + 1 : activate_ctr_r;
    for(i=0; i<1024 ; i=i+1) begin
      bf1_ns[i] = bf1_r[i];
      bf2_ns[i] = bf2_r[i];
    end
    lfsr1_ns = lfsr1_r;
    lfsr2_ns = lfsr2_r;
    // compute hashes
    hash_preseed[0] = row_addr[15:6];
    hash_preseed[1] = row_addr[11:2];
    hash_preseed[2] = {row_addr[5:0],row_addr[15:12]};
    hash_preseed[3] = {row_addr[10:5],row_addr[1:0],row_addr[15:14]};
    hash_preseed[4] = {row_addr[12:9],row_addr[2:0],core_id};
    hash_preseed[5] = {row_addr[14:11],core_id,row_addr[5:4],row_addr[8]};
    hash_preseed[6] = {core_id,row_addr[15:9]};
    hash_preseed[7] = {row_addr[7:6],core_id,row_addr[4:0]};

    for(i=0 ; i<7 ; i=i+1) begin
      hash1[i] = hash_preseed[i] ^ lfsr1_r[9:0];
      hash2[i] = hash_preseed[i] ^ lfsr2_r[9:0];
    end

    // access elements
    for(i=0 ; i<1024 ; i=i+1) begin
      if(insert_valid) begin
        if( (hash1[0] == i) || (hash1[1] == i) || (hash1[2] == i) || (hash1[3] == i) ||
            (hash1[4] == i) || (hash1[5] == i) || (hash1[6] == i) || (hash1[7] == i) )
          bf1_ns[i] = bf1_r[i] + 1;
        if( (hash2[0] == i) || (hash2[1] == i) || (hash2[2] == i) || (hash2[3] == i) ||
            (hash2[4] == i) || (hash2[5] == i) || (hash2[6] == i) || (hash2[7] == i))
          bf2_ns[i] = bf2_r[i] + 1;
      end
    end
    for(i=0 ; i<1024 ; i=i+1) begin
      if(hash1[0] == i)
        comp_reduce1[0] = bf1_r[i];
      if(hash1[1] == i)
        comp_reduce1[1] = bf1_r[i];
      if(hash1[2] == i)
        comp_reduce1[2] = bf1_r[i];
      if(hash1[3] == i)
        comp_reduce1[3] = bf1_r[i];
      if(hash1[4] == i)
        comp_reduce1[4] = bf1_r[i];
      if(hash1[5] == i)
        comp_reduce1[5] = bf1_r[i];
      if(hash1[6] == i)
        comp_reduce1[6] = bf1_r[i];
      if(hash1[7] == i)
        comp_reduce1[7] = bf1_r[i];
        
      if(hash2[0] == i)
        comp_reduce2[0] = bf2_r[i];
      if(hash2[1] == i)
        comp_reduce2[1] = bf2_r[i];
      if(hash2[2] == i)
        comp_reduce2[2] = bf2_r[i];
      if(hash2[3] == i)
        comp_reduce2[3] = bf2_r[i];  
      if(hash2[4] == i)
        comp_reduce2[4] = bf2_r[i];
      if(hash2[5] == i)
        comp_reduce2[5] = bf2_r[i];
      if(hash2[6] == i)
        comp_reduce2[6] = bf2_r[i];
      if(hash2[7] == i)
        comp_reduce2[7] = bf2_r[i]; 
    end
    for(i=0 ; i<7 ; i=i+1) begin
        comp_vector1[i] = comp_reduce1[i] > COUNTER_THRESHOLD;
        comp_vector2[i] = comp_reduce2[i] > COUNTER_THRESHOLD;
    end
    // swap filters when needed
    if(activate_ctr_r == NBFOVER2) begin
      lfsr1_ns = {lfsr1_r[13:0],~(lfsr1_r[14] ^ lfsr1_r[13])};
      lfsr2_ns = {lfsr2_r[13:0],~(lfsr2_r[14] ^ lfsr2_r[13])};
      activate_ctr_ns = 0;
      active_filter_ns = ~active_filter_r;
      if(active_filter_r) begin
        for(i=0 ; i<1024 ; i=i+1)
          bf2_ns[i] = 0;
      end
      else begin
        for(i=0 ; i<1024 ; i=i+1)
          bf1_ns[i] = 0;
      end
    end
  end
  
  always @(posedge clk) begin
    if(rst) begin
      activate_ctr_r <= 0;
      active_filter_r <= 0;
      for(i=0; i<1024 ; i=i+1) begin
        bf1_r[i] <= 0;
        bf2_r[i] <= 0;
      end
      lfsr1_r <= 15'd6969;
      lfsr2_r <= 15'd1337;
    end
    else begin
      lfsr1_r <= lfsr1_ns;
      lfsr2_r <= lfsr2_ns;
      activate_ctr_r  <= activate_ctr_ns;
      active_filter_r <= active_filter_ns;
      for(i=0; i<1024 ; i=i+1) begin
        if(bf1_ns[i] != 16'b0) begin
          bf1_r[i] <= bf1_ns[i];
          bf2_r[i] <= bf2_ns[i];
        end
      end
    end
  end  
  
endmodule
