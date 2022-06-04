# Define key paths
set target_library [<path_to_65_nm_database_file>]
set link_library [<path_to_65nm_database_file>]
set search_path [<path_to_verilog_files>]

# Bring sources into Synopsys environment and elaborate the design
analyze -format verilog -library WORK {<verilog_module>.v}
elaborate <verilog_module_name> -architecture verilog -library WORK -update

# Define clocking constraints
create_clk clk -period <clk_period_in_nanoseconds>

# Synthesize the design
compile_ultra

# Generate reports
report_timing
report_area
report_power