cmd="../ramulator ../configs/DDR4-config.cfg --mode=cpu"
cmd="$cmd -p mapping=default_mapping"
cmd="$cmd -p mem_scheduler=FRFCFS_PriorHit"
cmd="$cmd -p aggrow0=0_24"
cmd="$cmd -p aggrow1=0_26"
cmd="$cmd -p rowhammer_threshold=32768"
cmd="$cmd -p collect_energy=true"
cmd="$cmd -p drampower_specs_file=../DRAMPower/memspecs/MICRON_4Gb_DDR4-2400_8bit_A.csv"
cmd="$cmd -p l3_size=8388608"
cmd="$cmd -p core_limit_instr_exempt_flags=1"
cmd="$cmd -p outdir=./datadir/429.mcf_underattack/base"
cmd="$cmd -t ../traces/hammerall.4clxor"
cmd="$cmd -t ../traces/429.mcf"

echo $cmd
eval $cmd

cmd="../ramulator ../configs/DDR4-config.cfg --mode=cpu"
cmd="$cmd -p mapping=default_mapping"
cmd="$cmd -p mem_scheduler=FRFCFS_PriorHit"
cmd="$cmd -p aggrow0=0_24"
cmd="$cmd -p aggrow1=0_26"
cmd="$cmd -p rowhammer_threshold=32768"
cmd="$cmd -p collect_energy=true"
cmd="$cmd -p drampower_specs_file=../DRAMPower/memspecs/MICRON_4Gb_DDR4-2400_8bit_A.csv"
cmd="$cmd -p l3_size=8388608"
cmd="$cmd -p core_limit_instr_exempt_flags=1"
cmd="$cmd -p outdir=./datadir/429.mcf_underattack/mech"
cmd="$cmd -c rowhammer_defense=blockhammer";
cmd="$cmd -c blockhammer_nth=1024";
cmd="$cmd -c blockhammer_nbf=1048576";
cmd="$cmd -c bf_size=16384";
cmd="$cmd -c blockhammer_tth=0.9";
cmd="$cmd -t ../traces/hammerall.4clxor"
cmd="$cmd -t ../traces/429.mcf"

echo $cmd
eval $cmd