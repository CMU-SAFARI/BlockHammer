#!/bin/bash 

OUTDIR="./datadir"
TRACEDIR="../traces"

# Create the output directory
mkdir -p $OUTDIR;
echo "*" > $OUTDIR/.gitignore

# Make sure the code is compiled
cd ..;
make -j;
cd -;

# Baseline arguments
cmd="../ramulator";	
cmd="$cmd ../configs/DDR4-config.cfg"; 
cmd="$cmd --mode=cpu";
cmd="$cmd -p mapping=default_mapping"
cmd="$cmd -p mem_scheduler=FRFCFS_PriorHit";
cmd="$cmd -p aggrow0=0_24 -p aggrow1=0_26";
cmd="$cmd -p rowhammer_threshold=32768"
cmd="$cmd -p collect_energy=true -p drampower_specs_file=../DRAMPower/memspecs/MICRON_4Gb_DDR4-2400_8bit_A.csv"

# Running single-core workloads
sccmd="$cmd -p l3_size=1048576" # 1MB
for tracefile in `ls $TRACEDIR`; 
do
  echo ""
  echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++";
  echo "Testing $tracefile ";
  echo "--------------------------------------------------------------------------";
  BASEOUTDIR="$OUTDIR/$tracefile/base";
  mkdir -p $BASEOUTDIR;
  if [ -f "$BASEOUTDIR/ramulator.stats" ]; then     # File exists
    if [ -s "$BASEOUTDIR/ramulator.stats" ]; then   # File is not empty
      echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++";
      continue
    fi  
  fi 

  basecmd="$sccmd -p outdir=$BASEOUTDIR"
  basecmd="$basecmd -t $TRACEDIR/$tracefile"
  basecmd="$basecmd > $BASEOUTDIR/std.out"
  echo "Running $basecmd";
  eval $basecmd;
  
  MECHOUTDIR="$OUTDIR/$tracefile/mech";
  mkdir -p $MECHOUTDIR;
  if [ -f "$MECHOUTDIR/ramulator.stats" ]; then     # File exists
    if [ -s "$MECHOUTDIR/ramulator.stats" ]; then   # File is not empty
      echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++";
      continue
    fi  
  fi

  mechcmd="$sccmd -p outdir=$MECHOUTDIR";
  mechcmd="$mechcmd -c rowhammer_defense=blockhammer";
  mechcmd="$mechcmd -c blockhammer_nth=1024";
  mechcmd="$mechcmd -c blockhammer_nbf=1048576";
  mechcmd="$mechcmd -c bf_size=16384";
  mechcmd="$mechcmd -c blockhammer_tth=0.9";
  mechcmd="$mechcmd -t $TRACEDIR/$tracefile";
  mechcmd="$mechcmd > $MECHOUTDIR/std.out"
  echo "Running $mechcmd";
  eval $mechcmd;
  echo "Updating the results file";
  python collect_results.py $OUTDIR;
  echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++";
done

# Running a multiprogrammed workloads
mccmd="$cmd -p l3_size=8388608" # 8MB (1MB for each of 8 cores)
mcfx7args=" -t $TRACEDIR/429.mcf -t $TRACEDIR/429.mcf -t $TRACEDIR/429.mcf -t $TRACEDIR/429.mcf -t $TRACEDIR/429.mcf -t $TRACEDIR/429.mcf -t $TRACEDIR/429.mcf"
workloadnames=( "429.mcfx8" "429.mcfx7_underattack" )
traceargs=( "-t $TRACEDIR/429.mcf $mcfx7args" "-t $TRACEDIR/hammerall.4clxor $mcfx7args" )

for i in 0 1;
do
  workloadname=${workloadnames[i]};
  tracearg=${traceargs[i]};
  echo "";
  echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++";
  echo "Testing $workloadname ";
  echo "--------------------------------------------------------------------------";
  BASEOUTDIR="$OUTDIR/$workloadname/base";
  mkdir -p $BASEOUTDIR;
  skipbase=0
  if [ -f "$BASEOUTDIR/ramulator.stats" ]; then     # File exists
    if [ -s "$BASEOUTDIR/ramulator.stats" ]; then   # File is not empty
      echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++";
      skipbase=1
    fi  
  fi 

  if [ $skipbase -eq 0 ]; then
    basecmd="$mccmd -p outdir=$BASEOUTDIR";
    basecmd="$basecmd $tracearg";
    basecmd="$basecmd > $BASEOUTDIR/std.out";
    echo "Running $basecmd";
    eval $basecmd;
  fi

  MECHOUTDIR="$OUTDIR/$workloadname/mech";
  mkdir -p $MECHOUTDIR;
  skipmech=0
  if [ -f "$MECHOUTDIR/ramulator.stats" ]; then     # File exists
    if [ -s "$MECHOUTDIR/ramulator.stats" ]; then   # File is not empty
      echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++";
      skipmech=1
    fi  
  fi

  if [ $skipmech -eq 0 ]; then
    mechcmd="$mccmd -p core_limit_instr_exempt_flags=1"
    mechcmd="$mechcmd -p outdir=$MECHOUTDIR";
    mechcmd="$mechcmd -c rowhammer_defense=blockhammer";
    mechcmd="$mechcmd -c blockhammer_nth=1024";
    mechcmd="$mechcmd -c blockhammer_nbf=1048576";
    mechcmd="$mechcmd -c bf_size=16384";
    mechcmd="$mechcmd -c blockhammer_tth=0.9";
    mechcmd="$mechcmd $tracearg";
    mechcmd="$mechcmd > $MECHOUTDIR/std.out"
    echo "Running $mechcmd";
    # eval $mechcmd;
  fi
  echo "Updating the results file";
  python collect_results.py $OUTDIR;
  echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++";
done

