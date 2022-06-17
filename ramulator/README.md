# About

This is a modified Ramulator version which implements a class called RowHammerDefense. 
RowHammerDefense class collects RowHammer-related statistics (e.g., activation counts) and implements the following RowHammer defense mechanisms:

- PARA,        //Y. Kim et al. ``Flipping Bits...,'' in ISCA'14

- CBT,         //S. M. Seyedzadeh et al., ``Counter-Based...,” CAL'17.

- PROHIT,      //M. Son et al., ``Making DRAM Stronger Against Row Hammering,'' in DAC'17

- MRLOC,       //J. M. You et al., ``MRLoc: Mitigating Row-Hammering Based on Memory Locality,'' in DAC'19

- TWICE,       //E. Lee et al., ``TWiCe: Preventing Row-Hammering...,'' in ISCA'19.

- GRAPHENE,    //Y. Park et al., ``Graphene: Strong yet Lightweight Row Hammer Protection,'' in MICRO'20

- BLOCKHAMMER, //A. G. Yaglikci et al., ``BlockHammer: Preventing RowHammer at Low Cost...'' in HPCA'21

One can create a RowHammerDefense object for each channel, rank, or bank. 

This repo is developed for BlockHammer, but kept evolving after the paper is published. Therefore, it ccan reeport 

## Citation
Please cite our full HPCA 2021 paper if you use this Ramulator version.

> A. Giray Yaglikci, Minesh Patel, Jeremie S. Kim, Roknoddin Azizi, Ataberk Olgun, Lois Orosa, Hasan Hassan, Jisung Park, Konstantinos Kanellopoulos, Taha Shahroodi, Saugata Ghose, and Onur Mutlu,
**"BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows"**
Proceedings of the 27th International Symposium on High-Performance Computer Architecture (HPCA), Virtual, February-March 2021.

```bibtex
@inproceedings{yaglikci2021blockhammer,
  title={{BlockHammer: Preventing RowHammer at Low Cost by Blacklisting Rapidly-Accessed DRAM Rows}},
  author={Ya{\u{g}}l{\i}k{\c{c}}{\i}, A Giray and Patel, Minesh and Kim, Jeremie S. and Azizibarzoki, Roknoddin and Olgun, Ataberk and Orosa, Lois and Hassan, Hasan and Park, Jisung and Kanellopoullos, Konstantinos and Shahroodi, Taha and Ghose, Saugata and Mutlu, Onur},
  booktitle={HPCA},
  year={2021}
}
```

## Interface with Ramulator
A RowHammerDefense object communicates with the rest of the simulator via three questions: 

### Is a row activation RowHammer-safe?

This question is important for throttling-based RowHammer defenses. The memory request scheduler asks this question when it checks whether a DRAM command is ready for issuing. 

```c++
// This function is called in DRAM::is_ready function,
// only if the command is an ACT.
// Returns the clock cycle until when the row activation is *not* RowHammer-safe
long is_rowhammer_safe(int row_id, long clk, bool is_real_hammer, int coreid);
```

### Should we perform a reactive victim row refresh? If so, which one?
This question is important for reactive refresh RowHammer defenses. The memory request scheduler asks this question when it *issues* a DRAM row activation.

```c++
// This function is called in Controller::issue_cmd, 
// only if the issued command is an ACT.
// Returns the address of the row that needs to be refreshed
// Return -1 if no more rows need to be refreshed
// Controller::issue_cmd function enqueues the neccessary refreshes
// This function is repeatedly called until it returns -1
int get_row_to_refresh(int row_id, long clk, int adj_row_refresh_cnt, bool is_real_hammer, int coreid)
```

### Should a thread be throttled? If so, at what scale?
This question is again important for throttling-based mechanisms. 

```c++
// This function is called in Controller::enqueue
// Returns a value in between 0 and 1, 
// where 0 means don't enqueue any requests
// while 1 means enqueue freely.
// The request is enqueud if the thread has fewer on-the-fly requests than the queue size x returned value
float get_throttling_coeff(int row_id, int coreid, bool is_real_hammer)
```

## Additional command-line arguments
```c++
// RowHammer
{"rowhammer_stats", "rowhammer_stats"},
{"rowhammer_defense", "none"},     // Which defense mechanism to simulate?
{"rowhammer_defense_level", "-1"}, // Which hierarchical level to put the RowHammmer defense? (e.g., per rank/per bank)
{"rowhammer_threshold", "65536"},  // # of ACTs to induce a bit flip (aka. HCfirst and MAC)
{"rowhammer_br", "1"},             // RowHammer Blast Radius
{"aggrow0",  "-1"}, {"aggrow1",  "-1"}, {"aggrow2",  "-1"}, {"aggrow3",  "-1"},
{"aggrow4",  "-1"}, {"aggrow5",  "-1"}, {"aggrow6",  "-1"}, {"aggrow7",  "-1"},
{"aggrow8",  "-1"}, {"aggrow9",  "-1"}, {"aggrow10", "-1"}, {"aggrow11", "-1"}, 
{"aggrow12", "-1"}, {"aggrow13", "-1"}, {"aggrow14", "-1"}, {"aggrow15", "-1"},

// PARA
{"para_threshold", "0.001f"},

// BlockHammer
{"bf_hash_cnt", "0"},         // Number of hash functions in BlockHammer's Bloom filters
{"bf_size", "1024"},          // Bloom filter size (i.e., number of counters in a counting Bloom filter)
{"blockhammer_dryrun", "0"},  // if 1, blacklisting is active, but does not throttle
{"blockhammer_nth", "2"},     // Blacklisting threshold
{"blockhammer_nbf", "128"},   // Number of activations in a tCBF time window
{"blockhammer_tth", "1.0f"},  // Throttling threshold

// TWiCe
{"twice_threshold", "32768"},

// CBT
{"cbt_total_levels", "10"},
{"cbt_counter_no", "32"},

//ProHit
{"prh_cold", "3"},
{"prh_hot", "4"},

// Graphene
{"grw", "1383784"}, // tREFW (64ms) / tRC (46.25ns)
{"grt", "0"}, // if 0, internally set to NRH / 4
{"grk", "1"}, // how many times will graphene be reset in a refresh window.
``` 

## Guidelines for Reproducing Results

Please do not hesitate to contact A. Giray Yaglikci, regarding the issues you may face at agirayyaglikci@gmail.com

### Traces
To reproduce the performance results, you can find the traces under [`traces`](traces) directory. 

### Simulating BlockHammer
[`tests/test_blockhammer.sh`](tests/test_blockhammer.sh) script shows how to configure BlockHammer using command line parameters. 

The following snippet from [`tests/test_blockhammer.sh`](tests/test_blockhammer.sh) shows the configuration used for generating the results in Figs 4 and 5. 

```
mechcmd="$mechcmd -c rowhammer_defense=blockhammer";
mechcmd="$mechcmd -c blockhammer_nth=8192";
mechcmd="$mechcmd -c blockhammer_nbf=1048576";
mechcmd="$mechcmd -c bf_size=1024";
mechcmd="$mechcmd -c blockhammer_tth=0.9";
```

To regenerate the results in Fig 6, please refer to Table 7 in the extended version of the paper, available here: [`https://people.inf.ethz.ch/omutlu/pub/BlockHammer_preventing-DRAM-rowhammer-at-low-cost_hpca21.pdf`](https://people.inf.ethz.ch/omutlu/pub/BlockHammer_preventing-DRAM-rowhammer-at-low-cost_hpca21.pdf)

### Collecting Simulation Results

There are two useful Python files under tests directory for your convenience: 

- [`tests/StatParser3.py`](tests/StatParser3.py): A Python3 class that parses Ramulator's stats output. 

- [`tests/collect_results.py`](tests/collect_results.py): This is a script, written in Python3, to parse all simulation outputs in a directory tree and summarizes the most useful statistics into a csv file along with their normalized values with respect to the baseline simulation results. 

## Other Differences
 - This version of Ramulator has DRAMPower implemented. Just pass `-p collect_energy=true -p drampower_specs_file=path/to/your/csv/formatted/DRAMPower/configuration/file`

# Legacy README of Ramulator: A DRAM Simulator

Ramulator is a fast and cycle-accurate DRAM simulator \[1\] that supports a
wide array of commercial, as well as academic, DRAM standards:

- DDR3 (2007), DDR4 (2012)
- LPDDR3 (2012), LPDDR4 (2014)
- GDDR5 (2009)
- WIO (2011), WIO2 (2014)
- HBM (2013)
- SALP \[2\]
- TL-DRAM \[3\]
- RowClone \[4\]
- DSARP \[5\]

[\[1\] Kim et al. *Ramulator: A Fast and Extensible DRAM Simulator.* IEEE CAL
2015.](https://users.ece.cmu.edu/~omutlu/pub/ramulator_dram_simulator-ieee-cal15.pdf)  
[\[2\] Kim et al. *A Case for Exploiting Subarray-Level Parallelism (SALP) in
DRAM.* ISCA 2012.](https://users.ece.cmu.edu/~omutlu/pub/salp-dram_isca12.pdf)  
[\[3\] Lee et al. *Tiered-Latency DRAM: A Low Latency and Low Cost DRAM
Architecture.* HPCA 2013.](https://users.ece.cmu.edu/~omutlu/pub/tldram_hpca13.pdf)  
[\[4\] Seshadri et al. *RowClone: Fast and Energy-Efficient In-DRAM Bulk Data
Copy and Initialization.* MICRO
2013.](https://users.ece.cmu.edu/~omutlu/pub/rowclone_micro13.pdf)  
[\[5\] Chang et al. *Improving DRAM Performance by Parallelizing Refreshes with
Accesses.* HPCA 2014.](https://users.ece.cmu.edu/~omutlu/pub/dram-access-refresh-parallelization_hpca14.pdf)


## Usage

Ramulator supports three different usage modes.

1. **Memory Trace Driven:** Ramulator directly reads memory traces from a
  file, and simulates only the DRAM subsystem. Each line in the trace file 
  represents a memory request, with the hexadecimal address followed by 'R' 
  or 'W' for read or write.

  - 0x12345680 R
  - 0x4cbd56c0 W
  - ...


2. **CPU Trace Driven:** Ramulator directly reads instruction traces from a 
  file, and simulates a simplified model of a "core" that generates memory 
  requests to the DRAM subsystem. Each line in the trace file represents a 
  memory request, and can have one of the following two formats.

  - `<num-cpuinst> <addr-read>`: For a line with two tokens, the first token 
        represents the number of CPU (i.e., non-memory) instructions before
        the memory request, and the second token is the decimal address of a
        *read*. 

  - `<num-cpuinst> <addr-read> <addr-writeback>`: For a line with three tokens,
        the third token is the decimal address of the *writeback* request, 
        which is the dirty cache-line eviction caused by the read request
        before it.

3. **gem5 Driven:** Ramulator runs as part of a full-system simulator (gem5
  \[6\]), from which it receives memory request as they are generated.

For some of the DRAM standards, Ramulator is also capable of reporting
power consumption by relying on DRAMPower \[7\] as the backend. 

[\[6\] The gem5 Simulator System.](http://www.gem5.org)  
[\[7\] Chandrasekar et al. *DRAMPower: Open-Source DRAM Power & Energy
Estimation Tool.* IEEE CAL 2015.](http://www.drampower.info)


## Getting Started

Ramulator requires a C++11 compiler (e.g., `clang++`, `g++-5`).

1. **Memory Trace Driven**

        $ cd ramulator
        $ make -j
        $ ./ramulator configs/DDR3-config.cfg --mode=dram dram.trace
        Simulation done. Statistics written to DDR3.stats
        # NOTE: dram.trace is a very short trace file provided only as an example.
        $ ./ramulator configs/DDR3-config.cfg --mode=dram --stats my_output.txt dram.trace
        Simulation done. Statistics written to my_output.txt
        # NOTE: optional --stats flag changes the statistics output filename

2. **CPU Trace Driven**

        $ cd ramulator
        $ make -j
        $ ./ramulator configs/DDR3-config.cfg --mode=cpu cpu.trace
        Simulation done. Statistics written to DDR3.stats
        # NOTE: cpu.trace is a very short trace file provided only as an example.
        $ ./ramulator configs/DDR3-config.cfg --mode=cpu --stats my_output.txt cpu.trace
        Simulation done. Statistics written to my_output.txt
        # NOTE: optional --stats flag changes the statistics output filename

3. **gem5 Driven**

   *Requires SWIG 2.0.12+, gperftools (`libgoogle-perftools-dev` package on Ubuntu)*

        $ hg clone http://repo.gem5.org/gem5-stable
        $ cd gem5-stable
        $ hg update -c 10231  # Revert to stable version from 5/31/2014 (10231:0e86fac7254c)
        $ patch -Np1 --ignore-whitespace < /path/to/ramulator/gem5-0e86fac7254c-ramulator.patch
        $ cd ext/ramulator
        $ mkdir Ramulator
        $ cp -r /path/to/ramulator/src Ramulator
        # Compile gem5
        # Run gem5 with `--mem-type=ramulator` and `--ramulator-config=configs/DDR3-config.cfg`

  By default, gem5 uses the atomic CPU and uses atomic memory accesses, i.e. a detailed memory model like ramulator is not really used. To actually run gem5 in timing mode, a CPU type need to be specified by command line parameter `--cpu-type`. e.g. `--cpu-type=timing`
        
## Simulation Output

Ramulator will report a series of statistics for every run, which are written
to a file.  We have provided a series of gem5-compatible statistics classes in
`Statistics.h`.

**Memory Trace/CPU Trace Driven**: When run in memory trace driven or CPU trace
driven mode, Ramulator will write these statistics to a file.  By default, the
filename will be `<standard_name>.stats` (e.g., `DDR3.stats`).  You can write
the statistics file to a different filename by adding `--stats <filename>` to
the command line after the `--mode` switch (see examples above).

**gem5 Driven**: Ramulator automatically integrates its statistics into gem5.
Ramulator's statistics are written directly into the gem5 statistic file, with
the prefix `ramulator.` added to each stat's name.

*NOTE: When creating your own stats objects, don't place them inside STL
containers that are automatically resized (e.g, vector).  Since these
containers copy on resize, you will end up with duplicate statistics printed
in the output file.*


## Reproducing Results from Paper (Kim et al. \[1\])


### Debugging & Verification (Section 4.1)

For debugging and verification purposes, Ramulator can print the trace of every
DRAM command it issues along with their address and timing information. To do
so, please turn on the `print_cmd_trace` variable in the configuration file.


### Comparison Against Other Simulators (Section 4.2)

For comparing Ramulator against other DRAM simulators, we provide a script that
automates the process: `test_ddr3.py`. Before you run this script, however, you
must specify the location of their executables and configuration files at
designated lines in the script's source code: 

* Ramulator
* DRAMSim2 (https://wiki.umd.edu/DRAMSim2): `test_ddr3.py` lines 39-40
* USIMM, (http://www.cs.utah.edu/~rajeev/jwac12): `test_ddr3.py` lines 54-55
* DrSim (http://lph.ece.utexas.edu/public/Main/DrSim): `test_ddr3.py` lines 66-67
* NVMain (http://wiki.nvmain.org): `test_ddr3.py`  lines 78-79

Please refer to their respective websites to download, build, and set-up the
other simulators. The simulators must to be executed in saturation mode (always
filling up the request queues when possible).

All five simulators were configured using the same parameters:

* DDR3-1600K (11-11-11), 1 Channel, 1 Rank, 2Gb x8 chips
* FR-FCFS Scheduling
* Open-Row Policy
* 32/32 Entry Read/Write Queues
* High/Low Watermarks for Write Queue: 28/16

Finally, execute `test_ddr3.py <num-requests>` to start off the simulation.
Please make sure that there are no other active processes during simulation to
yield accurate measurements of memory usage and CPU time.


### Cross-Sectional Study of DRAM Standards (Section 4.3)

Please use the CPU traces (SPEC 2006) provided in the `cputraces` folder to run
CPU trace driven simulations.


## Other Tips

### Power Estimation

For estimating power consumption, Ramulator can record the trace of every DRAM
command it issues to a file in DRAMPower \[7\] format.  To do so, please turn
on the `record_cmd_trace` variable in the configuration file.  The resulting
DRAM command trace (e.g., `cmd-trace-chan-N-rank-M.cmdtrace`) should be fed
into DRAMPower with the correct configuration (standard/speed/organization)
to estimate energy/power usage for a single rank (a limitation of DRAMPower).


### Contributors

- Yoongu Kim (Carnegie Mellon University)
- Weikun Yang (Peking University)
- Kevin Chang (Carnegie Mellon University)
- Donghyuk Lee (Carnegie Mellon University)
- Vivek Seshadri (Carnegie Mellon University)
- Saugata Ghose (Carnegie Mellon University)
- Tianshi Li (Carnegie Mellon University)
- @henryzh
