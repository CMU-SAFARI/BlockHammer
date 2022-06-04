#include "Processor.h"
#include "Config.h"
#include "Controller.h"
#include "SpeedyController.h"
#include "Memory.h"
#include "DRAM.h"
#include "Statistics.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>
#include <functional>
#include <map>
#include <chrono>

/* Standards */
#include "Gem5Wrapper.h"
#include "DDR3.h"
#include "DDR4.h"
//#include "DSARP.h"
#include "GDDR5.h"
//#include "LPDDR3.h"
#include "LPDDR4.h"
#include "WideIO.h"
//#include "WideIO2.h"
//#include "HBM.h"
// #include "SALP.h"
#include "ALDRAM.h"
//#include "TLDRAM.h"

using namespace std;
using namespace ramulator;

bool ramulator::warmup_complete = false;

ScalarStat* warmup_time;
ScalarStat* simulation_time;

// @Giray: This array keeps the number 
vector<float> throttling_coeffs;
 
template<typename T>
void run_dramtrace(const Config& configs, Memory<T, Controller>& memory, const char* tracename) {

    printf("Running DRAM trace.\n");
    /* initialize DRAM trace */
    Trace trace(tracename);

    /* run simulation */
    bool stall = false, end = false;
    int reads = 0, writes = 0, clks = 0;
    long addr = 0;
    Request::Type type = Request::Type::READ;
    map<int, int> latencies;
    auto read_complete = [&latencies](Request& r){latencies[r.depart - r.arrive]++;};

    Request req(addr, type, read_complete);

    while (!end || memory.pending_requests()){
        if (!end && !stall){
            end = !trace.get_dramtrace_request(addr, type);
        }

        if (!end){
            req.addr = addr;
            req.type = type;
            // std::cout << &req << std::endl;
            stall = !memory.send(req);
            if (!stall){
                if (type == Request::Type::READ) reads++;
                else if (type == Request::Type::WRITE) writes++;
            }
        }
        memory.tick();
        clks ++;
        Stats::curTick++; // memory clock, global, for Statistics
    }
    // This a workaround for statistics set only initially lost in the end
    memory.finish();
    Stats::statlist.printall();

}

template <typename T>
void run_cputrace(Config& configs, Memory<T, Controller>& memory, const std::vector<std::string>& files)
{
    for (int i = 0; i < 64 ; i++)
      throttling_coeffs.push_back(1.0);
    
    int cpu_tick = configs.get_int("cpu_tick");
    int mem_tick = configs.get_int("mem_tick");
    auto send = bind(&Memory<T, Controller>::send, &memory, placeholders::_1);
    auto upgrade_prefetch_req = bind(&Memory<T, Controller>::upgrade_prefetch_req, &memory, placeholders::_1);
    Processor proc(configs, files, send, upgrade_prefetch_req, memory, throttling_coeffs);

    long warmup_insts = configs.get_long("warmup_insts");
    bool is_warming_up = (warmup_insts != 0);

    auto start = std::chrono::steady_clock::now();

    for(long i = 0; is_warming_up; i++){
        proc.tick();
        Stats::curTick++;
        if (i % cpu_tick == (cpu_tick - 1))
            for (int j = 0; j < mem_tick; j++)
                memory.tick();

        is_warming_up = false;
        for(unsigned int c = 0; c < proc.cores.size(); c++){
            if(proc.cores[c]->get_insts() < warmup_insts)
                is_warming_up = true;
        }

    }
    auto warmup_duration = std::chrono::duration_cast<std::chrono::seconds>
                                    (std::chrono::steady_clock::now() - start);

    warmup_complete = true;
    printf("Warmup complete! Resetting stats...\n");
    Stats::reset_stats();
    proc.reset_stats();
    assert(proc.get_insts() == 0);

    printf("Starting the simulation...\n");
    configs.enable_sim_options();


    cpu_tick = configs.get_int("cpu_tick"); // reload option in case it is different in sim_options
    mem_tick = configs.get_int("mem_tick"); // reload option in case it is different in sim_options.
    memory.reload_options(configs); // FIXME

    start = std::chrono::steady_clock::now();

    bool is_early_exit = configs.get_bool("early_exit");
    bool exit_at_cycle = configs.get_bool("exit_at_cycle");
    long expected_limit_cycles = configs.get_long("expected_limit_cycles");

    int tick_mult = cpu_tick * mem_tick;
    for (long i = 0; ; i = (i+1) % tick_mult) {
        // cout << "iteration: " << i << endl;
        if ((i % mem_tick) == 0) { // We use mem_tick to check when to tick the CPU.
                                                 // It is due to the definition of the tick ratios.
                                                 // e.g., When the CPU is ticked cpu_tick times,
                                                 // the memory controller should be ticked mem_tick times
            // cout << "proctick @ " << i << endl;
            proc.tick();
            Stats::curTick++; // processor clock, global, for Statistics

            if(exit_at_cycle){
                if(proc.finished()){
                    // cout << "Exit at cycle: proc finished!" << endl;
                    break; 
                }
	        }

            if (configs.calc_weighted_speedup()) {
                if (proc.has_reached_limit() && !exit_at_cycle) {
                    // cout << "Proc reached limit!" << endl;
                    break;
                }
            } else {
                if (is_early_exit) {
                    if (proc.finished()){
                        // cout << "Early exit: proc finished!" << endl;
                        break;
                    }
                } else {
                    if (proc.finished() && (memory.pending_requests() == 0)){
                        // cout << "no pending requests && proc finished!" << endl;
                        break;
                    }
                }
            }
        }

        if ((i % cpu_tick) == 0)
            memory.tick();
        

    }
    // This a workaround for statistics set only initially lost in the end
    memory.finish();

    auto simulation_duration = std::chrono::duration_cast<std::chrono::seconds>
                                    (std::chrono::steady_clock::now() - start);

    (*warmup_time) += warmup_duration.count();
    (*simulation_time) += simulation_duration.count();

    Stats::statlist.printall();
}

template<typename T>
void start_run(Config& configs, T* spec, const vector<std::string>& files) {
  // initiate controller and memory
  printf("Starting the run.\n");
  int C = configs.get_int("channels"), R = configs.get_int("ranks");
  printf("I have %d channels and %d ranks.\n", C, R);
  // Check and Set channel, rank number
  spec->set_channel_number(C);
  spec->set_rank_number(R);
  printf("Spec channels and ranks are set. \n");

  std::vector<Controller<T>*> ctrls;
  for (int c = 0 ; c < C ; c++) {
    DRAM<T>* channel = new DRAM<T>(spec, configs, T::Level::Channel, 0, "");
    printf("DRAM is created. Let's put a controller\n");
    // channel->id = c;
    channel->regStats("");
    Controller<T>* ctrl = new Controller<T>(configs, channel, throttling_coeffs);
    printf("Controller is created.\n");
    ctrls.push_back(ctrl);
  }
  printf("config:mapping: %s", configs.get_str("mapping").c_str());
  printf("Creating the memory subsystem with controller.\n");
  Memory<T, Controller> memory(configs, ctrls);

  printf("Memory subsystem is created. Starting reading the trace\n");
  assert(files.size() != 0);
  if (configs["mode"] == "cpu") {
    run_cputrace(configs, memory, files);
  } else if (configs["mode"] == "dram") {
    run_dramtrace(configs, memory, files[0].c_str());
  }
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: %s <configs-file> --mode=cpu,dram [--stats <filename>] <trace-filename1> <trace-filename2>\n"
            "Example: %s ramulator-configs.cfg --mode=cpu cpu.trace cpu.trace\n", argv[0], argv[0]);
        return 0;
    }


    Config configs(argv[1]);
    configs.parse_cmdline(argc, argv); // cmdline overwrites configs file options

    const std::string& standard = configs["standard"];
    assert(standard != "" || "DRAM standard should be specified.");

    const std::string& mode = configs["mode"];
    assert(mode != "" || "The trace type (\'mode\') should be specified.");

    string outdir = configs.get_str("outdir");
    cout << "Writing all outputs under: " << outdir << endl;
    string stats_out = outdir + string("/") + configs.get_str("stats");
    Stats::statlist.output(stats_out);

    cout << "Writing stats under: " << stats_out << endl;
    warmup_time = new ScalarStat();
    simulation_time = new ScalarStat();
    warmup_time
            ->name("warmup_time")
            .desc("Time in second taken to complete the warmup phase.")
            .precision(0)
            ;

    simulation_time
            ->name("simulation_time")
            .desc("Time in second taken to complete the simulation.")
            .precision(0)
            ;

    //std::vector<const char*> files(&argv[trace_start], &argv[argc]);
    cout << "Getting trace files." << endl;
    std::vector<std::string> files = configs.get_trace_files();
    configs.dump();

    if (files.size() < 1) {
        cerr << "Error! You should specify at least one trace input file." << endl;
        exit(-1);
    }
    cout << "Ok I found at least one tracefile." << endl;
    //configs.update<int>("cores", argc - trace_start);
    configs.update<int>("cores", files.size());
    cout << "Updated cores." << endl;
 //   if (standard == "DDR3") {
 //     DDR3* ddr3 = new DDR3(configs["org"], configs["speed"]);
 //     start_run(configs, ddr3, files);
    //}else
     if (standard == "DDR4") {
      DDR4* ddr4 = new DDR4(configs, configs["org"], configs["speed"]);
      start_run(configs, ddr4, files);
    // } else if (standard == "SALP-MASA") {
    //   //SALP* salp8 = new SALP(configs["org"], configs["speed"], "SALP-MASA", configs.get_int("subarrays"));
    //   SALP* salp8 = new SALP(configs, "SALP-MASA");
    //   start_run(configs, salp8, files);
//    } else if (standard == "LPDDR3") {
//      LPDDR3* lpddr3 = new LPDDR3(configs["org"], configs["speed"]);
//      start_run(configs, lpddr3, files);
    } 
    // else if (standard == "LPDDR4") {
    //   // total cap: 2GB, 1/2 of others
    //   //LPDDR4* lpddr4 = new LPDDR4(configs["org"], configs["speed"]);
    //   LPDDR4* lpddr4 = new LPDDR4(configs);
    //   start_run(configs, lpddr4, files);
    // } //else if (standard == "GDDR5") {
 //     GDDR5* gddr5 = new GDDR5(configs["org"], configs["speed"]);
  //    start_run(configs, gddr5, files);
 //   } else if (standard == "HBM") {
 //     HBM* hbm = new HBM(configs["org"], configs["speed"]);
 //     start_run(configs, hbm, files);
//    } else if (standard == "WideIO") {
//      // total cap: 1GB, 1/4 of others
//      WideIO* wio = new WideIO(configs["org"], configs["speed"]);
//      start_run(configs, wio, files);
//    } else if (standard == "WideIO2") {
//      // total cap: 2GB, 1/2 of others
//      WideIO2* wio2 = new WideIO2(configs["org"], configs["speed"], configs.get_int("channels"));
//      wio2->channel_width *= 2;
//      start_run(configs, wio2, files);
//    }
    // Various refresh mechanisms
//      else if (standard == "DSARP") {
//      DSARP* dsddr3_dsarp = new DSARP(configs["org"], configs["speed"], DSARP::Type::DSARP, configs.get_int("subarrays"));
//      start_run(configs, dsddr3_dsarp, files);
   /* } else if (standard == "ALDRAM") {
      ALDRAM* aldram = new ALDRAM(configs["org"], configs["speed"]);
      start_run(configs, aldram, files);*/
    //} else if (standard == "TLDRAM") {
    //  TLDRAM* tldram = new TLDRAM(configs["org"], configs["speed"], configs.get_int("subarrays"));
    //  start_run(configs, tldram, files);
    //}

    printf("Simulation done. Statistics written to %s\n", stats_out.c_str());

    return 0;
}