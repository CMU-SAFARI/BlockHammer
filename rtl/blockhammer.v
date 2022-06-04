`timescale 1ns / 1ps

module blockhammer(
  input         clk,
  input         rst,
  input [15:0]  row_addr,
  input [2:0]   core_id,
  input         in_valid,
  output        is_safe
  );
  
  wire rhb_match;
  wire bfs_aggressor;
  wire bfs_perf_attack;
  
  row_hist_buffer rhb(
    .clk(clk),
    .rst(rst),
    .row_addr(row_addr),
    .insert_valid(in_valid),
    .match(rhb_match)
  ); 
  
  bloom_filters bfs(
    .clk(clk),
    .rst(rst),
    .row_addr(row_addr),
    .core_id(core_id),
    .insert_valid(in_valid),
    .aggressor(bfs_aggressor),
    .perf_attack(bfs_perf_attack)
  );
  
  assign is_safe = !(rhb_match && bfs_match);
  
endmodule
