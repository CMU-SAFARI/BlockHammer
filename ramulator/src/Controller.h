#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#include <cassert>
#include <cstdio>
#include <deque>
#include <fstream>
#include <list>
#include <string>
#include <vector>
#include <cmath>

#include "Config.h"
#include "DRAM.h"
#include "Refresh.h"
#include "Request.h"
#include "Scheduler.h"
#include "Statistics.h"
#include "RowHammerDefense.h"
#include "ALDRAM.h"
#include "libdrampower/LibDRAMPower.h"
#include "CSVParser.h"

#include "CROWTable.h"

// #define COLLECT_QSTATE

using namespace std;

namespace ramulator
{

    extern bool warmup_complete;

template <typename T>
class Controller
{
protected:
    // For counting bandwidth
    ScalarStat read_transaction_bytes;
    ScalarStat write_transaction_bytes;

    ScalarStat row_hits;
    ScalarStat row_misses;
    ScalarStat row_conflicts;
    VectorStat read_row_hits;
    VectorStat read_row_misses;
    VectorStat read_row_conflicts;
    VectorStat write_row_hits;
    VectorStat write_row_misses;
    VectorStat write_row_conflicts;
    ScalarStat useless_activates;

    ScalarStat read_latency_avg;
    ScalarStat read_latency_sum;
    VectorStat read_latency_sum_per_core;
    VectorStat read_latency_avg_per_core;

    ScalarStat req_queue_length_avg;
    ScalarStat req_queue_length_sum;
    ScalarStat req_queue_length_max;
    ScalarStat read_req_queue_length_avg;
    ScalarStat read_req_queue_length_sum;
    ScalarStat write_req_queue_length_avg;
    ScalarStat write_req_queue_length_sum;

    ScalarStat issued_read;

    ScalarStat parallelized_victim_refresh;
    ScalarStat directly_issued_victim_refresh;

    // CROW
    ScalarStat crow_invPRE;
    ScalarStat crow_invACT;
    ScalarStat crow_full_restore;
    ScalarStat crow_skip_full_restore;
    ScalarStat crow_num_hits;
    ScalarStat crow_num_all_hits;
    ScalarStat crow_num_misses;
    ScalarStat crow_num_copies;

    ScalarStat crow_num_fr_set;
    ScalarStat crow_num_fr_notset;
    ScalarStat crow_num_fr_ref;
    ScalarStat crow_num_fr_restore;
    ScalarStat crow_num_hits_with_fr;
    ScalarStat crow_bypass_copying;

    ScalarStat crow_idle_cycle_due_trcd;
    ScalarStat crow_idle_cycle_due_tras;

    ScalarStat tl_dram_invalidate_due_to_write;
    ScalarStat tl_dram_precharge_cached_row_due_to_write;
    ScalarStat tl_dram_precharge_failed_due_to_timing;
    // END - CROW

    //DRAMPOWER
    ScalarStat act_energy;
    ScalarStat pre_energy;
    ScalarStat read_energy;
    ScalarStat write_energy;

    ScalarStat act_stdby_energy;
    ScalarStat pre_stdby_energy;
    ScalarStat idle_energy_act;
    ScalarStat idle_energy_pre;

    ScalarStat f_act_pd_energy;
    ScalarStat f_pre_pd_energy;
    ScalarStat s_act_pd_energy;
    ScalarStat s_pre_pd_energy;
    ScalarStat sref_energy;
    ScalarStat sref_ref_energy;
    ScalarStat sref_ref_act_energy;
    ScalarStat sref_ref_pre_energy;

    ScalarStat spup_energy;
    ScalarStat spup_ref_energy;
    ScalarStat spup_ref_act_energy;
    ScalarStat spup_ref_pre_energy;
    ScalarStat pup_act_energy;
    ScalarStat pup_pre_energy;

    ScalarStat IO_power;
    ScalarStat WR_ODT_power;
    ScalarStat TermRD_power;
    ScalarStat TermWR_power;

    ScalarStat read_io_energy;
    ScalarStat write_term_energy;
    ScalarStat read_oterm_energy;
    ScalarStat write_oterm_energy;
    ScalarStat io_term_energy;

    ScalarStat ref_energy;

    ScalarStat total_energy;
    ScalarStat average_power;

    // drampower counter

    // Number of activate commands
    ScalarStat numberofacts_s;
    // Number of precharge commands
    ScalarStat numberofpres_s;
    // Number of reads commands
    ScalarStat numberofreads_s;
    // Number of writes commands
    ScalarStat numberofwrites_s;
    // Number of refresh commands
    ScalarStat numberofrefs_s;
    // Number of precharge cycles
    ScalarStat precycles_s;
    // Number of active cycles
    ScalarStat actcycles_s;
    // Number of Idle cycles in the active state
    ScalarStat idlecycles_act_s;
    // Number of Idle cycles in the precharge state
    ScalarStat idlecycles_pre_s;
    // Number of fast-exit activate power-downs
    ScalarStat f_act_pdns_s;
    // Number of slow-exit activate power-downs
    ScalarStat s_act_pdns_s;
    // Number of fast-exit precharged power-downs
    ScalarStat f_pre_pdns_s;
    // Number of slow-exit activate power-downs
    ScalarStat s_pre_pdns_s;
    // Number of self-refresh commands
    ScalarStat numberofsrefs_s;
    // Number of clock cycles in fast-exit activate power-down mode
    ScalarStat f_act_pdcycles_s;
    // Number of clock cycles in slow-exit activate power-down mode
    ScalarStat s_act_pdcycles_s;
    // Number of clock cycles in fast-exit precharged power-down mode
    ScalarStat f_pre_pdcycles_s;
    // Number of clock cycles in slow-exit precharged power-down mode
    ScalarStat s_pre_pdcycles_s;
    // Number of clock cycles in self-refresh mode
    ScalarStat sref_cycles_s;
    // Number of clock cycles in activate power-up mode
    ScalarStat pup_act_cycles_s;
    // Number of clock cycles in precharged power-up mode
    ScalarStat pup_pre_cycles_s;
    // Number of clock cycles in self-refresh power-up mode
    ScalarStat spup_cycles_s;

    // Number of active auto-refresh cycles in self-refresh mode
    ScalarStat sref_ref_act_cycles_s;
    // Number of precharged auto-refresh cycles in self-refresh mode
    ScalarStat sref_ref_pre_cycles_s;
    // Number of active auto-refresh cycles during self-refresh exit
    ScalarStat spup_ref_act_cycles_s;
    // Number of precharged auto-refresh cycles during self-refresh exit
    ScalarStat spup_ref_pre_cycles_s;
    //End of DRAMPower Stats

#ifndef INTEGRATED_WITH_GEM5
    VectorStat record_read_hits;
    VectorStat record_read_misses;
    VectorStat record_read_conflicts;
    VectorStat record_write_hits;
    VectorStat record_write_misses;
    VectorStat record_write_conflicts;
    VectorStat record_read_latency_avg_per_core;
#endif

public:
    /* Member Variables */
    long clk = 0;
    DRAM<T>* channel;

    Scheduler<T>* scheduler;  // determines the highest priority request whose commands will be issued
    RowPolicy<T>* rowpolicy;  // determines the row-policy (e.g., closed-row vs. open-row)
    RowTable<T>* rowtable;  // tracks metadata about rows (e.g., which are open and for how long)
    Refresh<T>* refresh;

    typedef std::list<Request>::iterator ReqIter;
    struct Queue {
        list<Request> q;
        unsigned int max = 64;
        unsigned int size() const {return q.size();}
    };

    list<int> actlist;

    Queue readq;  // queue for read requests
    Queue writeq;  // queue for write requests
    Queue actq; // read and write requests for which activate was issued are moved to
                   // actq, which has higher priority than readq and writeq.
                   // This is an optimization
                   // for avoiding useless activations (i.e., PRECHARGE
                   // after ACTIVATE w/o READ of WRITE command)
    Queue rowhammerq;
    Queue otherq;  // queue for all "other" requests (e.g., refresh and para acts)

//    vector<Request> blockedq; // requests that are blocked by BlockHammeer
//    long earliest_unblock_time = 0;

    deque<Request> pending;  // read requests that are about to receive data from DRAM
    bool write_mode = false;  // whether write requests should be prioritized over reads
    //long refreshed = 0;  // last time refresh requests were generated

    /* Command trace for DRAMPower 3.1 */
    string cmd_trace_prefix = "cmd-trace-";
    vector<ofstream> cmd_trace_files;
    ofstream queue_state_file;
    ofstream tdelay_distr_file;


    bool record_cmd_trace = false;
    /* Commands to stdout */
    bool print_cmd_trace = false;

    bool enable_crow = false;
    bool enable_crow_upperbound = false;
    bool enable_tl_dram = false;
    int crow_evict_threshold = 0;
    int crow_half_life = 0;
    int crow_table_grouped_SAs = 0;
    float crow_to_mru_frac = 0.0f;
    unsigned int copy_rows_per_SA = 0;
    unsigned int weak_rows_per_SA = 0;
    float refresh_mult = 1.0f;
    bool prioritize_evict_fully_restored = false;
    bool collect_row_act_histogram = false;
    int num_SAs = 0;

    bool is_DDR4 = false, is_LPDDR4 = false; // Hasan

    CROWTable<T>* crow_table = nullptr;
    int* ref_counters;
    unsigned long crow_table_inv_interval;
    vector<int> crow_table_inv_index;
    bool inv_next_copy_row = false;


    bool refresh_disabled = false;
    bool is_verbose = false;
    int target_row_to_refresh = -2;
    bool collect_latencies = false;
    bool collect_energy = false;
    string drampower_specs_file = "";

    int num_cores;
    vector<long> num_on_the_fly_reqs;
    vector<long> cum_on_the_fly_reqs;
    vector<long> queue_sample_cnts;
    list<long> tqueue;
    list<long> tdelay;
    list<long> tservice;
    list<long> hammerflags;
    long last_queue_state_dump_clk = 0;
    int line_cnt = 0;
    vector<bool> bank_idle_status;

    vector<float> * throttling_coeffs_ptr;
    vector< vector<float> > throttling_coeffs_per_bank;

    libDRAMPower* drampower;
    int update_drampower_counters = 0;
    /* Constructor */
    //Controller(const Config& configs, DRAM<T>* channel) :
    Controller(const Config& configs, DRAM<T>* channel, vector<float> &throttling_coeffs) :
        channel(channel),
        scheduler(new Scheduler<T>(this, configs)),
        rowpolicy(new RowPolicy<T>(this, configs)),
        rowtable(new RowTable<T>(this)),
        refresh(new Refresh<T>(this)),
        cmd_trace_files(channel->children.size()),
        throttling_coeffs_ptr(&throttling_coeffs)
    {
        record_cmd_trace = configs.get_bool("record_cmd_trace");
        print_cmd_trace = configs.get_bool("print_cmd_trace");
        if (record_cmd_trace){
            cout << "I will record the command traces into ";
            cmd_trace_prefix = configs.get_str("outdir") + "/" + configs.get_str("cmd_trace");
            string prefix = cmd_trace_prefix + "chan-" + to_string(channel->id) + "-rank-";
            string suffix = ".cmdtrace";
            for (unsigned int i = 0; i < channel->children.size(); i++){
                string cmd_trace_file_name = prefix + to_string(i) + suffix;
                cmd_trace_files[i].open(cmd_trace_file_name);
                printf("%s\n",cmd_trace_file_name.c_str());
            }
        }

        this->collect_energy = configs.get_bool("collect_energy");
        this->drampower_specs_file = configs.get_str("drampower_specs_file");
        if (this->collect_energy){
            // cout << "DRAMPower integration is not completed yet." << endl;
            CSVParser * drampower_csvparser = new CSVParser();
            drampower = new libDRAMPower(drampower_csvparser->getMemSpecFromCSV(this->drampower_specs_file), true);
	}
    //	cerr << "Creating a DRAMPower MemSpec from file: " << this->drampower_specs_file << endl;
	//DRAMPower::MemorySpecification memSpec(DRAMPower::MemSpecParser::getMemSpecFromXML(this->drampower_specs_file));
	// Initialize counters and append header
	num_cores = configs.get_num_traces();
	num_on_the_fly_reqs.resize(num_cores,0);
	cum_on_the_fly_reqs.resize(num_cores,0);
	queue_sample_cnts.resize(num_cores,0);
	string queue_state_file_name = configs.get_str("outdir") + "/" + "chan-" + to_string(channel->id) + "_queuestate.csv";

        #ifdef COLLECT_QSTATE
	queue_state_file.open(queue_state_file_name);
        queue_state_file << "clk,num_reqs";
	for (int i = 0 ; i < num_cores ; i++)
	  queue_state_file << ",core" << i;
	queue_state_file << endl;
        #endif

        #ifdef COLLECT_TDELAY
	string tdelay_distr_file_name = configs.get_str("outdir") + "/" + "chan-" + to_string(channel->id) + "_tdelay_distr.csv";
	tdelay_distr_file.open(tdelay_distr_file_name);
        tdelay_distr_file << "tqueue,tdelay,tservice,hammer" << endl;
        #endif
	num_SAs = configs.get_int("subarrays");

        refresh_disabled = configs.get_bool("disable_refresh");
        collect_latencies = configs.get_bool("collect_latencies");
        cout << "Reloading the configs." << endl;

        // reload_options(configs);

        is_DDR4 = channel->spec->standard_name == "DDR4";
        is_LPDDR4 = channel->spec->standard_name == "LPDDR4";
        cout << "Registering stats" << endl;

        int num_banks = channel->get_num_banks();
        for (int i = 0 ; i < num_banks ; i++){
            bank_idle_status.push_back(true);
        }
        otherq.max = 600; // for CBT

        row_hits
            .name("row_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row hits per channel per core")
            .precision(0)
            ;
        row_misses
            .name("row_misses_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row misses per channel per core")
            .precision(0)
            ;
        row_conflicts
            .name("row_conflicts_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row conflicts per channel per core")
            .precision(0)
            ;

        read_row_hits
            .init(configs.get_int("cores"))
            .name("read_row_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row hits for read requests per channel per core")
            .precision(0)
            ;
        read_row_misses
            .init(configs.get_int("cores"))
            .name("read_row_misses_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row misses for read requests per channel per core")
            .precision(0)
            ;
        read_row_conflicts
            .init(configs.get_int("cores"))
            .name("read_row_conflicts_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row conflicts for read requests per channel per core")
            .precision(0)
            ;

        write_row_hits
            .init(configs.get_int("cores"))
            .name("write_row_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row hits for write requests per channel per core")
            .precision(0)
            ;
        write_row_misses
            .init(configs.get_int("cores"))
            .name("write_row_misses_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row misses for write requests per channel per core")
            .precision(0)
            ;
        write_row_conflicts
            .init(configs.get_int("cores"))
            .name("write_row_conflicts_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row conflicts for write requests per channel per core")
            .precision(0)
            ;

        useless_activates
            .name("useless_activates_"+to_string(channel->id)+ "_core")
            .desc("Number of useless activations. E.g, ACT -> PRE w/o RD or WR")
            .precision(0)
            ;

        read_transaction_bytes
            .name("read_transaction_bytes_"+to_string(channel->id))
            .desc("The total byte of read transaction per channel")
            .precision(0)
            ;
        write_transaction_bytes
            .name("write_transaction_bytes_"+to_string(channel->id))
            .desc("The total byte of write transaction per channel")
            .precision(0)
            ;

        read_latency_sum
            .name("read_latency_sum_"+to_string(channel->id))
            .desc("The memory latency cycles (in memory time domain) sum for all read requests in this channel")
            .precision(0)
            ;
        read_latency_sum_per_core
            .init(configs.get_int("cores"))
            .name("read_latency_sum_per_core"+to_string(channel->id))
            .desc("The memory latency cycles (in memory time domain) sum for all read requests per core")
            .precision(0)
            ;
        read_latency_avg
            .name("read_latency_avg_"+to_string(channel->id))
            .desc("The average memory latency cycles (in memory time domain) per request for all read requests in this channel")
            .precision(6)
            ;
        read_latency_avg_per_core
            .init(configs.get_int("cores"))
            .name("read_latency_avg_per_core"+to_string(channel->id))
            .desc("The average memory latency cycles (in memory time domain) per request per each core")
            .precision(6)
            ;
        req_queue_length_sum
            .name("req_queue_length_sum_"+to_string(channel->id))
            .desc("Sum of read and write queue length per memory cycle per channel.")
            .precision(0)
            ;
        req_queue_length_avg
            .name("req_queue_length_avg_"+to_string(channel->id))
            .desc("Average of read and write queue length per memory cycle per channel.")
            .precision(6)
            ;

        req_queue_length_max
            .name("req_queue_length_max_"+to_string(channel->id))
            .desc("Average of read and write queue length per memory cycle per channel.")
            .precision(0)
            ;

        read_req_queue_length_sum
            .name("read_req_queue_length_sum_"+to_string(channel->id))
            .desc("Read queue length sum per memory cycle per channel.")
            .precision(0)
            ;
        read_req_queue_length_avg
            .name("read_req_queue_length_avg_"+to_string(channel->id))
            .desc("Read queue length average per memory cycle per channel.")
            .precision(6)
            ;

        write_req_queue_length_sum
            .name("write_req_queue_length_sum_"+to_string(channel->id))
            .desc("Write queue length sum per memory cycle per channel.")
            .precision(0)
            ;
        write_req_queue_length_avg
            .name("write_req_queue_length_avg_"+to_string(channel->id))
            .desc("Write queue length average per memory cycle per channel.")
            .precision(6)
            ;

        // CROW
        crow_invPRE
            .name("crow_invPRE_channel_"+to_string(channel->id) + "_core")
            .desc("Number of Precharge commands issued to be able to activate an entry in the CROW table.")
            .precision(0)
            ;

        crow_invACT
            .name("crow_invACT_channel_"+to_string(channel->id) + "_core")
            .desc("Number of Activate command issued to fully activate the entry to invalidate from the CROW table.")
            .precision(0)
            ;

        crow_full_restore
            .name("crow_full_restore_channel_"+to_string(channel->id) + "_core")
            .desc("Number of Activate commands issued to fully restore an entry that is being discarded due to inserting a new entry.")
            .precision(0)
            ;

        crow_skip_full_restore
            .name("crow_skip_full_restore_channel_"+to_string(channel->id) + "_core")
            .desc("Number of times full restore was not needed (FR bit not set) when discarding an entry due to inserting a new one.")
            .precision(0)
            ;

        crow_num_hits
            .name("crow_num_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of hits to the CROW table (without additional activations needed for full restoration).")
            .precision(0)
            ;

        crow_num_all_hits
            .name("crow_num_all_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of hits to the CROW table.")
            .precision(0)
            ;

        crow_num_misses
            .name("crow_num_misses_channel_"+to_string(channel->id) + "_core")
            .desc("Number of misses to the CROW table.")
            .precision(0)
            ;

        crow_num_copies
            .name("crow_num_copies_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row copy operation CROW performed.")
            .precision(0)
            ;

        crow_num_fr_set
            .name("crow_num_fr_set_channel_"+to_string(channel->id) + "_core")
            .desc("Number of times FR bit is set when precharging.")
            .precision(0)
            ;
        crow_num_fr_notset
            .name("crow_num_fr_notset_channel_"+to_string(channel->id) + "_core")
            .desc("Number of times FR bit is not set when precharging.")
            .precision(0)
            ;
        crow_num_fr_ref
            .name("crow_num_fr_ref_channel_"+to_string(channel->id) + "_core")
            .desc("Number of times FR bit is set since the row won't be refreshed soon.")
            .precision(0)
            ;
        crow_num_fr_restore
            .name("crow_num_fr_restore_channel_"+to_string(channel->id) + "_core")
            .desc("Number of times FR bit is set since the row is not fully restored.")
            .precision(0)
            ;

        crow_num_hits_with_fr
            .name("crow_num_hits_with_fr_channel_"+to_string(channel->id) + "_core")
            .desc("Number of CROWTable hits to FR bit set entries.")
            .precision(0)
            ;

        crow_idle_cycle_due_trcd
            .name("crow_cycles_trcd_stall_channel_"+to_string(channel->id) + "_core")
            .desc("Number of cycles for which the command bus was idle but there was a request waiting for tRCD.")
            .precision(0)
            ;

        crow_idle_cycle_due_tras
            .name("crow_cycles_tras_stall_channel_"+to_string(channel->id) + "_core")
            .desc("Number of cycles for which the command bus was idle but there was a request waiting for tRAS.")
            .precision(0)
            ;

        crow_bypass_copying
            .name("crow_bypass_copying_channel_"+to_string(channel->id) + "_core")
            .desc("Number of rows not copied to a copy row due to having only rows above the hit threshold already cached.")
            .precision(0)
            ;

        tl_dram_invalidate_due_to_write
            .name("tl_dram_invalidate_due_to_write_channel_"+to_string(channel->id) + "_core")
            .desc("Number of TL-DRAM cached rows invalidated during activation due to pending writes.")
            .precision(0)
            ;

        tl_dram_precharge_cached_row_due_to_write
            .name("tl_dram_precharge_cached_row_due_to_write_channel_"+to_string(channel->id) + "_core")
            .desc("Number of TL-DRAM cached rows precharged to write data.")
            .precision(0)
            ;

        tl_dram_precharge_failed_due_to_timing
            .name("tl_dram_precharge_failed_due_to_timing_channel_"+to_string(channel->id) + "_core")
            .desc("Number of cycles failed to issue a PRE command to TL-DRAM cache row.")
            .precision(0)
            ;
        // END - CROW
	
        // DRAMPOWER
          // init DRAMPower stats
          act_energy
              .name("act_energy_" + to_string(channel->id))
              .desc("act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pre_energy
              .name("pre_energy_" + to_string(channel->id))
              .desc("pre_energy_" + to_string(channel->id))
              .precision(6)
              ;
          read_energy
              .name("read_energy_" + to_string(channel->id))
              .desc("read_energy_" + to_string(channel->id))
              .precision(6)
              ;
          write_energy
              .name("write_energy_" + to_string(channel->id))
              .desc("write_energy_" + to_string(channel->id))
              .precision(6)
              ;

          act_stdby_energy
              .name("act_stdby_energy_" + to_string(channel->id))
              .desc("act_stdby_energy_" + to_string(channel->id))
              .precision(6)
              ;

          pre_stdby_energy
              .name("pre_stdby_energy_" + to_string(channel->id))
              .desc("pre_stdby_energy_" + to_string(channel->id))
              .precision(6)
              ;

          idle_energy_act
              .name("idle_energy_act_" + to_string(channel->id))
              .desc("idle_energy_act_" + to_string(channel->id))
              .precision(6)
              ;

          idle_energy_pre
              .name("idle_energy_pre_" + to_string(channel->id))
              .desc("idle_energy_pre_" + to_string(channel->id))
              .precision(6)
              ;

          f_act_pd_energy
              .name("f_act_pd_energy_" + to_string(channel->id))
              .desc("f_act_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          f_pre_pd_energy
              .name("f_pre_pd_energy_" + to_string(channel->id))
              .desc("f_pre_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          s_act_pd_energy
              .name("s_act_pd_energy_" + to_string(channel->id))
              .desc("s_act_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          s_pre_pd_energy
              .name("s_pre_pd_energy_" + to_string(channel->id))
              .desc("s_pre_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_energy
              .name("sref_energy_" + to_string(channel->id))
              .desc("sref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_ref_energy
              .name("sref_ref_energy_" + to_string(channel->id))
              .desc("sref_ref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_ref_act_energy
              .name("sref_ref_act_energy_" + to_string(channel->id))
              .desc("sref_ref_act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_ref_pre_energy
              .name("sref_ref_pre_energy_" + to_string(channel->id))
              .desc("sref_ref_pre_energy_" + to_string(channel->id))
              .precision(6)
              ;

          spup_energy
              .name("spup_energy_" + to_string(channel->id))
              .desc("spup_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_ref_energy
              .name("spup_ref_energy_" + to_string(channel->id))
              .desc("spup_ref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_ref_act_energy
              .name("spup_ref_act_energy_" + to_string(channel->id))
              .desc("spup_ref_act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_ref_pre_energy
              .name("spup_ref_pre_energy_" + to_string(channel->id))
              .desc("spup_ref_pre_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pup_act_energy
              .name("pup_act_energy_" + to_string(channel->id))
              .desc("pup_act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pup_pre_energy
              .name("pup_pre_energy_" + to_string(channel->id))
              .desc("pup_pre_energy_" + to_string(channel->id))
              .precision(6)
              ;

          IO_power
              .name("IO_power_" + to_string(channel->id))
              .desc("IO_power_" + to_string(channel->id))
              .precision(6)
              ;
          WR_ODT_power
              .name("WR_ODT_power_" + to_string(channel->id))
              .desc("WR_ODT_power_" + to_string(channel->id))
              .precision(6)
              ;
          TermRD_power
              .name("TermRD_power_" + to_string(channel->id))
              .desc("TermRD_power_" + to_string(channel->id))
              .precision(6)
              ;
          TermWR_power
              .name("TermWR_power_" + to_string(channel->id))
              .desc("TermWR_power_" + to_string(channel->id))
              .precision(6)
              ;

          read_io_energy
              .name("read_io_energy_" + to_string(channel->id))
              .desc("read_io_energy_" + to_string(channel->id))
              .precision(6)
              ;
          write_term_energy
              .name("write_term_energy_" + to_string(channel->id))
              .desc("write_term_energy_" + to_string(channel->id))
              .precision(6)
              ;
          read_oterm_energy
              .name("read_oterm_energy_" + to_string(channel->id))
              .desc("read_oterm_energy_" + to_string(channel->id))
              .precision(6)
              ;
          write_oterm_energy
              .name("write_oterm_energy_" + to_string(channel->id))
              .desc("write_oterm_energy_" + to_string(channel->id))
              .precision(6)
              ;
          io_term_energy
              .name("io_term_energy_" + to_string(channel->id))
              .desc("io_term_energy_" + to_string(channel->id))
              .precision(6)
              ;

          ref_energy
              .name("ref_energy_" + to_string(channel->id))
              .desc("ref_energy_" + to_string(channel->id))
              .precision(6)
              ;

          total_energy
              .name("total_energy_" + to_string(channel->id))
              .desc("total_energy_" + to_string(channel->id))
              .precision(6)
              ;
          average_power
              .name("average_power_" + to_string(channel->id))
              .desc("average_power_" + to_string(channel->id))
              .precision(6)
              ;

          numberofacts_s
              .name("numberofacts_s_" + to_string(channel->id))
              .desc("Number of activate commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofpres_s
              .name("numberofpres_s_" + to_string(channel->id))
              .desc("Number of precharge commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofreads_s
              .name("numberofreads_s_" + to_string(channel->id))
              .desc("Number of reads commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofwrites_s
              .name("numberofwrites_s_" + to_string(channel->id))
              .desc("Number of writes commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofrefs_s
              .name("numberofrefs_s_" + to_string(channel->id))
              .desc("Number of refresh commands_" + to_string(channel->id))
              .precision(0)
              ;
          precycles_s
              .name("precycles_s_" + to_string(channel->id))
              .desc("Number of precharge cycles_" + to_string(channel->id))
              .precision(0)
              ;
          actcycles_s
              .name("actcycles_s_" + to_string(channel->id))
              .desc("Number of active cycles_" + to_string(channel->id))
              .precision(0)
              ;
          idlecycles_act_s
              .name("idlecycles_act_s_" + to_string(channel->id))
              .desc("Number of Idle cycles in the active state_" + to_string(channel->id))
              .precision(0)
              ;
          idlecycles_pre_s
              .name("idlecycles_pre_s_" + to_string(channel->id))
              .desc("Number of Idle cycles in the precharge state_" + to_string(channel->id))
              .precision(0)
              ;
          f_act_pdns_s
              .name("f_act_pdns_s_" + to_string(channel->id))
              .desc("Number of fast-exit activate power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          s_act_pdns_s
              .name("s_act_pdns_s_" + to_string(channel->id))
              .desc("Number of slow-exit activate power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          f_pre_pdns_s
              .name("f_pre_pdns_s_" + to_string(channel->id))
              .desc("Number of fast-exit precharged power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          s_pre_pdns_s
              .name("s_pre_pdns_s_" + to_string(channel->id))
              .desc("Number of slow-exit activate power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          numberofsrefs_s
              .name("numberofsrefs_s_" + to_string(channel->id))
              .desc("Number of self-refresh commands_" + to_string(channel->id))
              .precision(0)
              ;
          f_act_pdcycles_s
              .name("f_act_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in fast-exit activate power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          s_act_pdcycles_s
              .name("s_act_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in slow-exit activate power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          f_pre_pdcycles_s
              .name("f_pre_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in fast-exit precharged power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          s_pre_pdcycles_s
              .name("s_pre_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in slow-exit precharged power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          sref_cycles_s
              .name("sref_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in self-refresh mode_" + to_string(channel->id))
              .precision(0)
              ;
          pup_act_cycles_s
              .name("pup_act_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in activate power-up mode_" + to_string(channel->id))
              .precision(0)
              ;
          pup_pre_cycles_s
              .name("pup_pre_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in precharged power-up mode_" + to_string(channel->id))
              .precision(0)
              ;
          spup_cycles_s
              .name("spup_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in self-refresh power-up mode_" + to_string(channel->id))
              .precision(0)
              ;
          sref_ref_act_cycles_s
              .name("sref_ref_act_cycles_s_" + to_string(channel->id))
              .desc("Number of active auto-refresh cycles in self-refresh mode_" + to_string(channel->id))
              .precision(0)
              ;
          sref_ref_pre_cycles_s
              .name("sref_ref_pre_cycles_s_" + to_string(channel->id))
              .desc("Number of precharged auto-refresh cycles in self-refresh mode_" + to_string(channel->id))
              .precision(0)
              ;
          spup_ref_act_cycles_s
              .name("spup_ref_act_cycles_s_" + to_string(channel->id))
              .desc("Number of active auto-refresh cycles during self-refresh exit_" + to_string(channel->id))
              .precision(0)
              ;
          spup_ref_pre_cycles_s
              .name("spup_ref_pre_cycles_s_" + to_string(channel->id))
              .desc("Number of precharged auto-refresh cycles during self-refresh exit_" + to_string(channel->id))
              .precision(0)
              ;	

#ifndef INTEGRATED_WITH_GEM5
        record_read_hits
            .init(configs.get_int("cores"))
            .name("record_read_hits")
            .desc("record read hit count for this core when it reaches request limit or to the end")
            ;

        record_read_misses
            .init(configs.get_int("cores"))
            .name("record_read_misses")
            .desc("record_read_miss count for this core when it reaches request limit or to the end")
            ;

        record_read_conflicts
            .init(configs.get_int("cores"))
            .name("record_read_conflicts")
            .desc("record read conflict count for this core when it reaches request limit or to the end")
            ;

        record_write_hits
            .init(configs.get_int("cores"))
            .name("record_write_hits")
            .desc("record write hit count for this core when it reaches request limit or to the end")
            ;

        record_write_misses
            .init(configs.get_int("cores"))
            .name("record_write_misses")
            .desc("record write miss count for this core when it reaches request limit or to the end")
            ;

        record_write_conflicts
            .init(configs.get_int("cores"))
            .name("record_write_conflicts")
            .desc("record write conflict for this core when it reaches request limit or to the end")
            ;
        record_read_latency_avg_per_core
            .init(configs.get_int("cores"))
            .name("record_read_latency_avg_"+to_string(channel->id))
            .desc("The memory latency cycles (in memory time domain) average per core in this channel")
            .precision(6)
            ;

#endif
    }

    ~Controller(){
        delete scheduler;
        delete rowpolicy;
        delete rowtable;
        delete channel;
        delete refresh;
        for (auto& file : cmd_trace_files)
            file.close();
        cmd_trace_files.clear();
        #ifdef COLLECT_QSTATE
        queue_state_file.close();
        #endif
        #ifdef COLLECT_TDELAY
        tdelay_distr_file.close();
        #endif
        delete crow_table;
        delete[] ref_counters;
        delete drampower;
    }

    void finish(long read_req, long dram_cycles) {
      read_latency_avg = read_latency_sum.value() / read_req;
      req_queue_length_avg = req_queue_length_sum.value() / dram_cycles;
      read_req_queue_length_avg = read_req_queue_length_sum.value() / dram_cycles;
      write_req_queue_length_avg = write_req_queue_length_sum.value() / dram_cycles;

	  for(int coreid = 0; coreid < read_latency_avg_per_core.size() ; coreid++){
	  	read_latency_avg_per_core[coreid] = read_latency_sum_per_core[coreid].value()/
                                        (float)(read_row_hits[coreid].value() + read_row_misses[coreid].value() +
                                                read_row_conflicts[coreid].value());
	  }

	if(collect_energy){  
        // DRAMPower stuff: 
        drampower->updateCounters(true); // last update
        drampower->calcEnergy();
        act_energy = drampower->getEnergy().act_energy;
        pre_energy = drampower->getEnergy().pre_energy;
        read_energy = drampower->getEnergy().read_energy;
        write_energy = drampower->getEnergy().write_energy;

        act_stdby_energy = drampower->getEnergy().act_stdby_energy;
        pre_stdby_energy = drampower->getEnergy().pre_stdby_energy;
        idle_energy_act = drampower->getEnergy().idle_energy_act;
        idle_energy_pre = drampower->getEnergy().idle_energy_pre;

        f_act_pd_energy = drampower->getEnergy().f_act_pd_energy;
        f_pre_pd_energy = drampower->getEnergy().f_pre_pd_energy;
        s_act_pd_energy = drampower->getEnergy().s_act_pd_energy;
        s_pre_pd_energy = drampower->getEnergy().s_pre_pd_energy;
        sref_energy = drampower->getEnergy().sref_energy;
        sref_ref_energy = drampower->getEnergy().sref_ref_energy;
        sref_ref_act_energy = drampower->getEnergy().sref_ref_act_energy;
        sref_ref_pre_energy = drampower->getEnergy().sref_ref_pre_energy;

        spup_energy = drampower->getEnergy().spup_energy;
        spup_ref_energy = drampower->getEnergy().spup_ref_energy;
        spup_ref_act_energy = drampower->getEnergy().spup_ref_act_energy;
        spup_ref_pre_energy = drampower->getEnergy().spup_ref_pre_energy;
        pup_act_energy = drampower->getEnergy().pup_act_energy;
        pup_pre_energy = drampower->getEnergy().pup_pre_energy;

        IO_power = drampower->getPower().IO_power;
        WR_ODT_power = drampower->getPower().WR_ODT_power;
        TermRD_power = drampower->getPower().TermRD_power;
        TermWR_power = drampower->getPower().TermWR_power;

        read_io_energy = drampower->getEnergy().read_io_energy;
        write_term_energy = drampower->getEnergy().write_term_energy;
        read_oterm_energy = drampower->getEnergy().read_oterm_energy;
        write_oterm_energy = drampower->getEnergy().write_oterm_energy;
        io_term_energy = drampower->getEnergy().io_term_energy;

        ref_energy = drampower->getEnergy().ref_energy;

        total_energy = drampower->getEnergy().total_energy;
        average_power = drampower->getPower().average_power;
        //drampower counter
        numberofacts_s = 1;
        numberofpres_s = 1;
        numberofreads_s = 1;
        numberofwrites_s = 1;
        numberofrefs_s = drampower->counters.numberofrefs;
        precycles_s = drampower->counters.precycles;
        actcycles_s = drampower->counters.actcycles;
        idlecycles_act_s = drampower->counters.idlecycles_act;
        idlecycles_pre_s = drampower->counters.idlecycles_pre;
        f_act_pdns_s = drampower->counters.f_act_pdns;
        s_act_pdns_s = drampower->counters.s_act_pdns;
        f_pre_pdns_s = drampower->counters.f_pre_pdns;
        s_pre_pdns_s = drampower->counters.s_pre_pdns;
        numberofsrefs_s = drampower->counters.numberofsrefs;
        f_act_pdcycles_s = drampower->counters.f_act_pdcycles;
        s_act_pdcycles_s = drampower->counters.s_act_pdcycles;
        f_pre_pdcycles_s = drampower->counters.f_pre_pdcycles;
        s_pre_pdcycles_s = drampower->counters.s_pre_pdcycles;
        sref_cycles_s = drampower->counters.sref_cycles;
        pup_act_cycles_s = drampower->counters.pup_act_cycles;
        pup_pre_cycles_s = drampower->counters.pup_pre_cycles;
        spup_cycles_s = drampower->counters.spup_cycles;
        sref_ref_act_cycles_s = drampower->counters.sref_ref_act_cycles;
        sref_ref_pre_cycles_s = drampower->counters.sref_ref_pre_cycles;
        spup_ref_act_cycles_s = drampower->counters.spup_ref_act_cycles;
        spup_ref_pre_cycles_s = drampower->counters.spup_ref_pre_cycles;
	}
    // call finish function of each channel
    channel->finish(dram_cycles, clk);

    // print out the row_act_hist
    if(collect_row_act_histogram) {
        printf("Printing Row Activation Histogram\n");
        printf("Format: row_id, access_count\n");
        printf("=================================\n");
        for(int bank_id = 0; bank_id < 8; bank_id++) {
            for(int sa_id = 0; sa_id < num_SAs; sa_id++) {
                printf("----- Bank %d, Subarray %d\n", bank_id, sa_id);
                auto& cur_hist = row_act_hist[bank_id][sa_id];

                for(auto it = cur_hist.begin(); it != cur_hist.end(); it++) {
                    printf("%d, %d\n", it->first, it->second);
                }
            }

        }
    }
    #ifdef COLLECT_TDELAY
    if (collect_latencies){
        long n = tqueue.size();
        for (long i = 0; i < n ; i++){
            tdelay_distr_file << tqueue.front()
		        << "," << tdelay.front()
			    << "," << tservice.front()
			    << "," << hammerflags.front()
			    << endl;
            tqueue.pop_front();
            tdelay.pop_front();
            tservice.pop_front();
            hammerflags.pop_front();
        }
    }
    #endif
    }

    /* Member Functions */
    Queue& get_queue(Request::Type type)
    {
        switch (int(type)) {
            case int(Request::Type::READ):
            case int(Request::Type::PREFETCH): return readq;
            case int(Request::Type::WRITE): return writeq;
            default: return otherq;
        }
    }

    bool ok_to_enqueue(Request& req)
    {
        Queue& queue = get_queue(req.type);
        // cout << "The corresponding queue already has " << (int) queue.size() << " requests." << endl;

        float throttling_coeff = 1.0;
        if (req.type != Request::Type::REFRESH)
        {
            throttling_coeff = channel->get_throttling_coeff(req);
 	        throttling_coeffs_ptr->at(req.coreid) = throttling_coeff;
            if (throttling_coeff * queue.max <= queue.size()){
                // cout << "Throttling coefficient for this thread-bank pair is " << throttling_coeff << "  Max, allowed requests is " << queue.max *  throttling_coeff << endl;
                // cout << "There are already " << queue.size() << " requests in this queue." << endl;
                // assert(false);
                return false;
            }
        }
        else{
            if ( queue.max == queue.size() ){
                // assert(false);
                return false;
            }
        }
        return true;
    }



    bool enqueue(Request &req)
    {
        if (req.type == Request::Type::HAMMER || req.type == Request::Type::READ  || req.type == Request::Type::WRITE){
            int flat_bank_id = channel->get_flat_bank_id(req.addr_vec.data(), 0);
            req.flat_bank_id = flat_bank_id;
        }

        req.dropped = false;
        if (req.type == Request::Type::HAMMER){
            req.type = Request::Type::READ;
            req.is_real_hammer = true;
        }

        Queue &queue = get_queue(req.type);

        req.arrive = clk;
        queue.q.push_back(req);
        // shortcut for read requests, if a write to same addr exists
        // necessary for coherence
        if ((req.type == Request::Type::READ || req.type == Request::Type::PREFETCH) && find_if(writeq.q.begin(), writeq.q.end(),
            [req](Request& wreq){ return req.addr == wreq.addr;}) != writeq.q.end()){
            req.depart = clk + 1;
            pending.push_back(req);
            readq.q.pop_back();
        }

        if ((req.type == Request::Type::READ || req.type == Request::Type::PREFETCH)){
            int flat_bank_id = channel->get_flat_bank_id(req.addr_vec.data(), 0);
            bank_idle_status[flat_bank_id] = false;
            for (int i = 0; i < num_cores; i++){
                num_on_the_fly_reqs[i] = 0;
            }
            for (auto const &r : queue.q){
                num_on_the_fly_reqs[r.coreid]++;
            }
            cum_on_the_fly_reqs[req.coreid] += num_on_the_fly_reqs[req.coreid];
            #ifdef COLLECT_QSTATE
            queue_sample_cnts[req.coreid]++;
            if (line_cnt < 1000 && (clk - last_queue_state_dump_clk > 100000 || queue_sample_cnts[req.coreid] > 10000)){
                queue_state_file << clk << "," << (int)queue.size();
                for (int cid = 0; cid < num_cores; cid++){
                    queue_state_file << "," << ((double)cum_on_the_fly_reqs[cid]) / queue_sample_cnts[cid];
                }
                queue_state_file << endl;
                last_queue_state_dump_clk = clk;
                for (int i = 0; i < num_cores; i++){
                    cum_on_the_fly_reqs[i] = 0;
                    queue_sample_cnts[i] = 0;
                }
                line_cnt++;
            }
            #endif
        }
        return true;
    }

    bool upgrade_prefetch_req(const Request& req) {
        assert(req.type == Request::Type::READ);

        Queue& queue = get_queue(req.type);

        // the prefetch request could be in readq, actq, or pending
        if (upgrade_prefetch_req(queue, req))
            return true;

        if (upgrade_prefetch_req(actq, req))
            return true;

        if (upgrade_prefetch_req(pending, req))
            return true;

        return false;
    }

    void tick()
    {
        clk++;
        {
          int instant_queue_length = readq.size() + writeq.size() + pending.size();
          req_queue_length_sum += instant_queue_length;
          read_req_queue_length_sum += readq.size() + pending.size();
          write_req_queue_length_sum += writeq.size();

          if (req_queue_length_max.value() < instant_queue_length)
            req_queue_length_max = instant_queue_length;
        }
        /*** 1. Serve completed reads ***/
        if (pending.size()) {
            Request& req = pending[0];
            if (req.depart <= clk) {
                if (req.depart - req.arrive > 1) { // this request really accessed a row
                    read_latency_sum += req.depart - req.arrive;
                    read_latency_sum_per_core[req.coreid] += (req.depart - req.arrive);
                    if (!req.dropped)
                        channel->update_serving_requests(
                    req.addr_vec.data(), -1, clk);
                }
                #ifdef COLLECT_TDELAY
                if (collect_latencies && req.unblock != -1){
                    long tq, td, ts, ha;
                    if(req.is_ever_blocked){
                        tq = req.block - req.arrive;
                        td = req.unblock - req.block;
                        ts = req.depart - req.unblock;
                        ha = req.is_real_hammer;
                    }
                    else {
                        tq = req.unblock - req.arrive;
                        td = 0;
                        ts = req.depart-req.unblock;
                        ha = req.is_real_hammer;
                    }
                    if(tq < 0){
                        cerr << "tqueue is less than zero" << endl;
                        cerr << "request arrives @ " << req.arrive << endl;
                        cerr << "        is blocked @ " << req.block << endl;
                        cerr << "        is unblocked @ " << req.unblock << endl;
                        cerr << "        departs @ " << req.depart << endl;
                        cerr << "        hammer? @ " << req.is_real_hammer << endl;
                        cerr << "        dropped? @ " << req.dropped << endl;
                        assert(false);
                    }

                    tqueue.push_back(tq);
                    tdelay.push_back(td);
                    tservice.push_back(ts);
                    hammerflags.push_back(ha);
                }
                #endif

                if (req.callback)
                    req.callback(req);
                pending.pop_front();
            }
        }

        /*** 2. Refresh scheduler ***/
        if (!refresh_disabled)
            refresh->tick_ref();

        /*** 3. Should we schedule writes? ***/
        if (!write_mode) {
            // yes -- write queue is almost full or read queue is empty
            if (writeq.size() >= int(0.8 * writeq.max) /*|| readq.size() == 0*/){
                write_mode = true;
            }
        }
        else {
            // no -- write queue is almost empty and read queue is not empty
            if (writeq.size() <= int(0.2 * writeq.max) && readq.size() != 0) {
                write_mode = false;
            }
        }

        /*** 4. Find the best command to schedule, if any ***/

        // First check the actq (which has higher priority) to see if there
        // are requests available to service in this cycle
        // string str = "Checking the actq\n";
        Queue* queue = &actq;
        typename T::Command cmd;

        auto req = scheduler->get_head(queue->q);

        bool is_valid_req = (req != queue->q.end());

        if(is_valid_req) {
            cmd = get_first_cmd(req);
            is_valid_req = is_ready(req);
            // str += "There is a valid request of type " + to_string((int) cmd) + " in the actq.\n";
        }


        if (!is_valid_req) {

            queue = !write_mode ? &readq : &writeq;
            // if(!write_mode)
            //     str += "Checking the readq.\n";
            // else
            //     str += "Checking the writeq.\n";

            if (otherq.size())
                // str += "Wait! Let's prioritize the otherq.\n";
                queue = &otherq;  // "other" requests are rare, so we give them precedence over reads/writes

            req = scheduler->get_head(queue->q);
            
            is_valid_req = (req != queue->q.end());

            if(is_valid_req){

                cmd = get_first_cmd(req);
                is_valid_req = is_ready(req);
                // str += "There is a valid request of type " + to_string((int) cmd) + ".\n";
            }
        }

        if (!is_valid_req) {
            // we couldn't find a command to schedule -- let's try to be speculative
            auto cmd = T::Command::PRE;
            vector<int> victim = rowpolicy->get_victim(cmd);
            if (!victim.empty())
                issue_cmd(cmd, victim);
            // cout << "No valid cmds in any queues. Skipping this cycle." << endl;

            return;  // nothing more to be done this cycle
        }

        if (req->is_first_command) {
            req->is_first_command = false;
            int coreid = req->coreid;
            if (req->type == Request::Type::READ || req->type == Request::Type::WRITE || req->type == Request::Type::PREFETCH) {
              channel->update_serving_requests(req->addr_vec.data(), 1, clk);
            }
            int tx = (channel->spec->prefetch_size * channel->spec->channel_width / 8);
            if (req->type == Request::Type::READ || req->type == Request::Type::PREFETCH) {
                if (is_row_hit(req)) {
                    ++read_row_hits[coreid];
                    ++row_hits;
                } else if (is_row_open(req)) {
                    ++read_row_conflicts[coreid];
                    ++row_conflicts;
                } else {
                    ++read_row_misses[coreid];
                    ++row_misses;
                }
              read_transaction_bytes += tx;
            } else if (req->type == Request::Type::WRITE) {
              if (is_row_hit(req)) {
                  ++write_row_hits[coreid];
                  ++row_hits;
              } else if (is_row_open(req)) {
                  ++write_row_conflicts[coreid];
                  ++row_conflicts;
              } else {
                  ++write_row_misses[coreid];
                  ++row_misses;
              }
              write_transaction_bytes += tx;
            }
        }

        // issue command on behalf of request
        cmd = get_first_cmd(req);
        // str += "cmd type before crow is " + to_string((int) cmd) + ".\n";
        

        // CROW
        // if going to activate a new row which will discard an entry from
        // the CROW table, we may need to fully restore the discarding row
        // first

        bool make_crow_copy = true;
        if (enable_crow && channel->spec->is_opening(cmd)) {
            vector<int> target_addr_vec = get_addr_vec(cmd, req);
            if(!crow_table->is_hit(target_addr_vec) && crow_table->is_full(target_addr_vec)) {
                bool discard_next = true;

                if(prioritize_evict_fully_restored) {
                    assert(false && "Error: Unimplemented functionality!");
                }

                if(discard_next) {
                    CROWEntry* cur_entry = crow_table->get_LRU_entry(target_addr_vec, crow_evict_threshold);
                    
                    if(cur_entry == nullptr) {
                        assert(!enable_tl_dram && "Error: It should always be possible to discard an entry with TL-DRAM.");
                        make_crow_copy = false;
                        crow_bypass_copying++;
                        }
                    else {
                        if(cur_entry->FR && !enable_tl_dram) {
                            // We first need to activate the discarding row to fully
                            // restore it
                            
                            target_addr_vec[int(T::Level::Row)] = cur_entry->row_addr;


                            issue_cmd(cmd, target_addr_vec, true); 
                            crow_full_restore++;
                            return;
                        } else {
                            // move to LRU
                            target_addr_vec[int(T::Level::Row)] = cur_entry->row_addr;
                            crow_table->make_LRU(target_addr_vec, cur_entry);
                            crow_skip_full_restore++;
                        }
                    }
                }
            }
            else if (crow_table->is_hit(target_addr_vec) && enable_tl_dram) {
                if(req->type == Request::Type::WRITE){
                    crow_table->invalidate(target_addr_vec);
                    tl_dram_invalidate_due_to_write++;
                }
            }

        }

        if((enable_crow && enable_tl_dram) && ((cmd == T::Command::WR) || (cmd == T::Command::WRA))) {
            vector<int> target_addr_vec = get_addr_vec(cmd, req);
            target_addr_vec[int(T::Level::Row)] = rowtable->get_open_row(target_addr_vec);

            CROWEntry* cur_entry = crow_table->get_hit_entry(target_addr_vec);
            if(cur_entry != nullptr && cur_entry->total_hits != 0) {
                // convert the command to precharge
                cmd = T::Command::PRE;
                int flat_bank_id = channel->get_flat_bank_id(target_addr_vec.data(), 0);
                if(is_ready(T::Command::PRE, target_addr_vec, false, -1)){
                    issue_cmd(cmd, target_addr_vec, true);
                    tl_dram_precharge_cached_row_due_to_write++;
                } else {
                    tl_dram_precharge_failed_due_to_timing++;
                }
                return;
            }
        }
        // END - CROW
        // str += "cmd type after crow is " + to_string((int) cmd) + ".\n";
        // cout << str << endl;

        issue_cmd(cmd, get_addr_vec(cmd, req), false, make_crow_copy, req->is_real_hammer, req->coreid);
        int req_flat_bank_id = channel->get_flat_bank_id(req->addr_vec.data(), 0); 


        // check whether this is the last command (which finishes the request)
        if (cmd != channel->spec->translate[int(req->type)]){
            if(channel->spec->is_opening(cmd)) {
                // promote the request that caused issuing activation to actq
                actq.q.push_back(*req);
                queue->q.erase(req);
            }

            return;
        }

        // set a future completion time for read requests
        if (req->type == Request::Type::READ || req->type == Request::Type::PREFETCH) {
            req->depart = clk + channel->spec->read_latency;
            pending.push_back(*req);
        }

        if (req->type == Request::Type::WRITE) {
            channel->update_serving_requests(req->addr_vec.data(), -1, clk);
        }

        // remove request from queue
        queue->q.erase(req);
    }

    // CROW
    bool req_hold_due_trcd(list<Request>& q) {
        // go over all requests in 'q' and check if there is one that could
        // have been issued if tRCD was 0.

        for (auto it = q.begin(); it != q.end(); it++) {
            if(is_ready_no_trcd(it))
                return true;
        }

        return false;
    }

    bool req_hold_due_tras(list<Request>& q) {
        // go over all requests in 'q' and check if there is one that could
        // have been issued if tRAS was 0.

        for (auto it = q.begin(); it != q.end(); it++) {
            if(is_ready_no_tras(it))
                return true;
        }

        return false;
    }

    bool is_ready_no_trcd(list<Request>::iterator req) {
        typename T::Command cmd = get_first_cmd(req);

        if(!channel->spec->is_accessing(cmd))
            return false;

        return channel->check_no_trcd(cmd, req->addr_vec.data(), clk);
    }

    bool is_ready_no_tras(list<Request>::iterator req) {
        typename T::Command cmd = get_first_cmd(req);

        if(cmd != T::Command::PRE)
            return false;

        return channel->check_no_tras(cmd, req->addr_vec.data(), clk);
    }
    // END - CROW

    bool is_ready(list<Request>::iterator req)
    {
        if (req->test_clk == clk){
	        return req->ready;
        }
        req->test_clk = clk;
        typename T::Command cmd = get_first_cmd(req);
        if(cmd == T::Command::ACT && req->blocked_until > clk){
            if (is_verbose)
                cout << "[" << clk << "] Request is already blocked until " << req->blocked_until << endl;
            // assert(false);
            req->ready = false;
            return false;
        }

	    bool ready = channel->check_iteratively(cmd, req->addr_vec.data(), clk, is_verbose);

        if (is_verbose){
            cout << "[" << clk << "] ";
            cout << "Cmd " << int(cmd) << ":";
            for (auto addr : req->addr_vec)
            cout << " " << addr;
            if (ready) cout << " ready" << endl;
            else cout << " notready" << endl;
        }

        if ((cmd == T::Command::ACT) && ready){
            long blocked_until = this->is_rowhammer_safe(req);
            if(clk < blocked_until){
                // cout << "Request is blocked @ clk cycle " << clk << endl;
                req->is_ever_blocked = true;
                req->blocked_until = blocked_until;
                ready=false;
            }
        }
        if (ready){
            req->unblock = clk;
        }
        req->ready = ready;
        return ready;
    }

    bool is_ready(typename T::Command cmd, const vector<int>& addr_vec, bool is_real_hammer, int coreid)
    {
        bool ready = channel->check_iteratively(cmd, addr_vec.data(), clk, is_verbose);
        if (is_verbose){
          cout << "[" << clk << "] ";
          cout << "Cmd " << int(cmd) << ":";
          for (auto addr : addr_vec)
            cout << " " << addr;
          if (ready) cout << " ready" << endl;
          else cout << " notready" << endl;
        }
        if ((cmd == T::Command::ACT) && ready){
            long blocked_until = this->is_rowhammer_safe(addr_vec, is_real_hammer, coreid);
            if (clk < blocked_until) {
                // cout << "Request is blocked @ clk cycle " << clk << endl;
                ready=false;
            }
        }
        return ready;
    }

    bool is_row_hit(list<Request>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        typename T::Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_hit(cmd, req->addr_vec.data());
    }

    bool is_row_hit(typename T::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_hit(cmd, addr_vec.data());
    }

    bool is_row_open(list<Request>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        typename T::Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_open(cmd, req->addr_vec.data());
    }

    bool is_row_open(typename T::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_open(cmd, addr_vec.data());
    }

    long is_rowhammer_safe(const vector<int>& addr_vec, bool is_real_hammer, int coreid)
    {
        return channel->is_rowhammer_safe(addr_vec.data(), clk, is_real_hammer, coreid);
    }

    long is_rowhammer_safe(list<Request>::iterator req)
    {
        bool is_real_hammer = (req->type == Request::Type::HAMMER);
        return channel->is_rowhammer_safe(req->addr_vec.data(), clk, is_real_hammer, req->coreid);
    }

    void update_temp(ALDRAM::Temp current_temperature)
    {
    }

    // For telling whether this channel is busying in processing read or write
    bool is_active() {
      return (channel->cur_serving_requests > 0);
    }

    // For telling whether this channel is under refresh
    bool is_refresh() {
      return clk <= channel->end_of_refreshing;
    }

    void record_core(int coreid) {
#ifndef INTEGRATED_WITH_GEM5
      record_read_hits[coreid] = read_row_hits[coreid];
      record_read_misses[coreid] = read_row_misses[coreid];
      record_read_conflicts[coreid] = read_row_conflicts[coreid];
      record_write_hits[coreid] = write_row_hits[coreid];
      record_write_misses[coreid] = write_row_misses[coreid];
      record_write_conflicts[coreid] = write_row_conflicts[coreid];
      record_read_latency_avg_per_core[coreid] = read_latency_sum_per_core[coreid].value()/
                                        (float)(read_row_hits[coreid].value() + read_row_misses[coreid].value() +
                                                read_row_conflicts[coreid].value());
#endif
    }



    void reload_options(const Config& configs) {
        cout << "Reload options starts here" << endl;
        channel->update_rowhammer_observer_options();
        channel->update_timings(configs);

        if((channel->spec->standard_name == "SALP-MASA") || (channel->spec->standard_name == "SALP-1") || 
                (channel->spec->standard_name == "SALP-2"))
            channel->update_num_subarrays(configs.get_int("subarrays"));

        prioritize_evict_fully_restored = configs.get_bool("crow_evict_fully_restored");
        collect_row_act_histogram = configs.get_bool("collect_row_activation_histogram");
        copy_rows_per_SA = configs.get_int("copy_rows_per_SA");
        weak_rows_per_SA = configs.get_int("weak_rows_per_SA");
        refresh_mult = configs.get_float("refresh_mult");
        crow_evict_threshold = configs.get_int("crow_entry_evict_hit_threshold");
        crow_half_life = configs.get_int("crow_half_life");
        crow_to_mru_frac = configs.get_float("crow_to_mru_frac");
        crow_table_grouped_SAs = configs.get_int("crow_table_grouped_SAs");
        
        refresh_disabled = configs.get_bool("disable_refresh"); 

        if (copy_rows_per_SA > 0)
            enable_crow = true;

        if (configs.get_bool("enable_crow_upperbound")) {
            enable_crow_upperbound = true;
            enable_crow = false;
        }

        enable_tl_dram = configs.get_bool("enable_tl_dram");

        if(enable_crow || enable_crow_upperbound) {

            // Adjust tREFI
            channel->spec->speed_entry.nREFI *= refresh_mult;
            
            trcd_crow_partial_hit = configs.get_float("trcd_crow_partial_hit");
            trcd_crow_full_hit = configs.get_float("trcd_crow_full_hit");

            tras_crow_partial_hit_partial_restore = configs.get_float("tras_crow_partial_hit_partial_restore");
            tras_crow_partial_hit_full_restore = configs.get_float("tras_crow_partial_hit_full_restore");
            tras_crow_full_hit_partial_restore = configs.get_float("tras_crow_full_hit_partial_restore");
            tras_crow_full_hit_full_restore = configs.get_float("tras_crow_full_hit_full_restore");
            tras_crow_copy_partial_restore = configs.get_float("tras_crow_copy_partial_restore");
            tras_crow_copy_full_restore = configs.get_float("tras_crow_copy_full_restore");

            twr_partial_restore = configs.get_float("twr_partial_restore");
            twr_full_restore = configs.get_float("twr_full_restore");

            initialize_crow();

        }

        if(collect_row_act_histogram)
            assert(num_SAs <= 128);
    }


private:
    typename T::Command get_first_cmd(list<Request>::iterator req)
    {
        typename T::Command cmd = channel->spec->translate[int(req->type)];
        //return channel->decode(cmd, req->addr_vec.data());
        cmd = channel->decode_iteratively(cmd, req->addr_vec.data());
        return cmd;
    }

    unsigned long last_clk = 0; // DEBUG
    unsigned long num_cas_cmds = 0;
    void issue_cmd(typename T::Command cmd, const vector<int>& addr_vec, bool do_full_restore = false, bool make_crow_copy = true, bool is_real_hammer = false, int coreid=-1)
    {
        if(!is_ready(cmd, addr_vec, false, channel->get_flat_bank_id(addr_vec.data(), 0))){
            // ===== Print the requests in all request queues =====
            // if(warmup_complete){ // && target_row_to_refresh == addr_vec[int(T::Level::Row)]){
                is_verbose = true;
                printf("Is write mode? : %s \n", write_mode ? "Yes" : "No");
                printf("=== ActQ === \n");
                for(auto req : actq.q) {
                    printf("bg:%d b:%d row:%d \n", req.addr_vec[2], req.addr_vec[3], req.addr_vec[4]);
                }
                printf("=== ===\n");
            
                printf("=== ReadQ === \n");
                for(auto req : readq.q) {
                    printf("bg:%d b:%d row:%d \n", req.addr_vec[2], req.addr_vec[3], req.addr_vec[4]);
                }
                printf("=== ===\n");
            
            
                printf("=== WriteQ === \n");
                for(auto req : writeq.q) {
                    printf("bg:%d b:%d row:%d \n", req.addr_vec[2], req.addr_vec[3], req.addr_vec[4]);
                }
                printf("=== ===\n");
                // ===================================
            
                if(cmd == T::Command::ACT)
                printf("Controller (%lu): Cmd: %s, addr: %d %d %d \n", clk, channel->spec->command_name[int(cmd)].c_str(),
                    addr_vec[2], addr_vec[3], addr_vec[4]);
            
                if(cmd == T::Command::ACT) {
                    int SA_size = channel->spec->org_entry.count[int(T::Level::Row)]/num_SAs;
                    printf("Controller %d: clk: %lu, ACT to c: %d r:%d bg:%d b:%d sa:%d row:%d \n",
                            channel->id, clk, addr_vec[0], addr_vec[1], addr_vec[2], addr_vec[3],
                                addr_vec[4]/SA_size, addr_vec[4]);
            
            
                }
            // }

            // ===== CMD Issue Timeline ===== //
            // assumes a single-channel, single-rank configuration
            // and DDR4 with bank-groups
            // if(warmup_complete) {
            printf("%lu \t%lu:\t %lu %d\t|", clk, clk - last_clk, num_cas_cmds, (int)read_row_conflicts[0].value());
            last_clk = clk;
            
            if(channel->spec->is_closing(cmd) && cmd != T::Command::PRE){
                // rank-level precharge
                //for(int bg = 0; bg < channel->spec->org_entry.count[int(T::Level::Bank) - 1]; bg++) {
                    for(int b = 0; b < channel->spec->org_entry.count[int(T::Level::Bank)]; b++) {
                        printf(" P ");
                    }
                    printf("|");
                //}
            }
            else {
                //for(int bg = 0; bg < channel->spec->org_entry.count[int(T::Level::Bank) - 1]; bg++) {
                    for(int b = 0; b < channel->spec->org_entry.count[int(T::Level::Bank)]; b++) {
                    if(//(bg == addr_vec[int(T::Level::Bank) - 1]) &&
                            b == addr_vec[int(T::Level::Bank)]) {
                            switch(cmd) {
                                case T::Command::ACT:
                                    if(do_full_restore)
                                        printf(" A ");
                                    else {
                                        if(enable_crow && crow_table->is_hit(addr_vec))
                                            printf(" H ");
                                        else
                                            printf(" a ");
                                    }
                                    break;
                                case T::Command::PRE:
                                    printf(" p ");
                                    break;
                                case T::Command::RD:
                                    printf(" r ");
                                    num_cas_cmds++;
                                    break;
                                case T::Command::WR:
                                    num_cas_cmds++;
                                    printf(" w ");
                                    break;
                                case T::Command::REF:
                                    printf(" R ");
                                    break;
                                default:
                                    printf(" %d ", int(cmd));
            
                            }
                    } else {
                            printf(" - ");
                    }
                    }
                    printf("|");
                //}
            }
            printf("\n");
            cerr << "Assert fail at issue_cmd" << endl;
            // } //warmup_complete

            // ===== END - CMD Issue Timeline ===== //
            assert(false);
        }

        if(warmup_complete && collect_row_act_histogram) {
            if(channel->spec->is_opening(cmd)) {
                int row_id = addr_vec[int(T::Level::Row)];
                int SA_size = channel->spec->org_entry.count[int(T::Level::Row)]/num_SAs;
                int sa_id = row_id/SA_size;

                auto& cur_hist = row_act_hist[addr_vec[int(T::Level::Bank)]][sa_id];

                int local_row_id = row_id % SA_size;
                if(cur_hist.find(local_row_id) == cur_hist.end())
                    cur_hist[local_row_id] = 1;
                else
                    cur_hist[local_row_id]++;
            }
        }

    	int flat_bank_id = channel->get_flat_bank_id(addr_vec.data(), 0);
        const string& cmd_name = channel->spec->command_name[int(cmd)];
        
        // CROW
        int crow_hit = 0; // 0 -> no hit, 1 -> hit to partially restored row, 2 -> hit to fully restored row
        bool crow_copy = false;

        if(enable_crow || enable_crow_upperbound) {
            if(channel->spec->is_opening(cmd)) {
                if(enable_crow_upperbound || crow_table->is_hit(addr_vec)) {
                    assert(make_crow_copy && "Error: A row activation without copying should not hit on CROWTable!");

                    bool require_full_restore = false;
                   
                    if(!enable_crow_upperbound)
                        require_full_restore = crow_table->get_hit_entry(addr_vec)->FR;

                    if(require_full_restore)
                        crow_hit = 1;
                    else
                        crow_hit = 2;


                    if(!do_full_restore) {
                        crow_num_hits++;
                        if(enable_crow)
                            crow_table->access(addr_vec); // we shouldn't access it if we do additional activation to fully restore
                    } else {
                        crow_table->access(addr_vec, true); // move the fully restored row to LRU position
                    }
                    
                    crow_num_all_hits++;

                    if(!enable_crow_upperbound && require_full_restore){
                        crow_num_hits_with_fr++;
                    }
                } else {
                    assert(!do_full_restore && "Full restoration should be issued only when the row is in the CROW table.");
                    assert((!enable_tl_dram || make_crow_copy) && "Error: ACT command should always copy when TL-DRAM is used.");
                    // crow miss
                    if(make_crow_copy) {
                        crow_copy = true;
                        crow_table->add_entry(addr_vec, false);
                        crow_num_copies++;
                    } else {
                        crow_table->access(addr_vec); // we know it is a miss but we access to update the hit_count of the LRU entry
                    }
                    
                    crow_num_misses++;
                }
            }

            if((cmd == T::Command::WR) || (cmd == T::Command::WRA)) {
               if(enable_crow_upperbound)
                  crow_hit = 2;
               else if(crow_table->is_hit(addr_vec)) {
                   if(crow_table->get_hit_entry(addr_vec)->FR)
                       crow_hit = 1;
                   else
                       crow_hit = 2;
               }
               else
                   // assert(false && "Can this happen?"); // Hasan:
                   // happens right after warmup when a row was opened
                   // before CROWTable initialization. Printing the message
                   // below to log how many times this happens to make sure
                   // it does not happen many times due to a different
                   // reason
                   //printf("Warning: Ramulator: Writing on a CROWTable miss!\n");
                   crow_hit = -1;
            }

            if((!enable_crow_upperbound && !enable_tl_dram) && channel->spec->is_closing(cmd)) {
                crow_set_FR_on_PRE(cmd, addr_vec);
            }

            
            if(!enable_crow_upperbound && channel->spec->is_refreshing(cmd)) {
                                
                int nREFI = channel->spec->speed_entry.nREFI;
                float tCK = channel->spec->speed_entry.tCK;
                int base_refw = is_LPDDR4 ? 32*refresh_mult : 64*refresh_mult;
                unsigned long ticks_in_ref_int = base_refw*1000000/tCK;
                int num_refs = ticks_in_ref_int/nREFI;

               
                int num_rows_refreshed_at_once;
                int num_ref_limit;
               
               if(is_DDR4 || is_LPDDR4) {
                  num_ref_limit = channel->spec->org_entry.count[int(T::Level::Row)];
                  num_rows_refreshed_at_once = ceil(channel->spec->org_entry
                                                    .count[int(T::Level::Row)]/(float)num_refs);
               }
               else { // For SALP
                   num_ref_limit = channel->spec->org_entry.count[int(T::Level::Row)] * 
                                    channel->spec->org_entry.count[int(T::Level::Row) - 1];
                   num_rows_refreshed_at_once = ceil(channel->spec->org_entry
                                                    .count[int(T::Level::Row)] * channel->spec->org_entry.count[int(T::Level::Row) - 1]/(float)num_refs);
               }

                ref_counters[addr_vec[int(T::Level::Rank)]] += num_rows_refreshed_at_once;

                if(ref_counters[addr_vec[int(T::Level::Rank)]] >= num_ref_limit)
                    ref_counters[addr_vec[int(T::Level::Rank)]] = 0;
            }

            switch(crow_hit) {
                case 0:
                    if(crow_copy){
                        assert(make_crow_copy && "Error: Using copy timing parameters for regular access!");
                        load_timing(channel, crow_copy_timing);
                    }
                    break;
                case 1: // partial hit
                    assert(!crow_copy && "Error: A row should not be copied if is already duplicated!");

                    if(do_full_restore){
                        assert(cmd == T::Command::ACT);
                        assert(!enable_crow_upperbound);
                        load_timing(channel, partial_crow_hit_full_restore_timing); 
                    }
                    else
                        load_timing(channel, partial_crow_hit_partial_restore_timing);
                    break;
                case 2: // hit to a fully restored row
                    assert(!crow_copy && "Error: A row should not be copied if is already duplicated!");

                    if(do_full_restore){
                        assert(cmd == T::Command::ACT);
                        assert(!enable_crow_upperbound);
                        load_timing(channel, full_crow_hit_full_restore_timing);
                    }
                    else
                        load_timing(channel, full_crow_hit_partial_restore_timing);
            }
            
        }

        // END - CROW

        channel->update(cmd, addr_vec.data(), clk);

        if(enable_crow && do_full_restore && (cmd == T::Command::ACT)) {
            // clean just_opened state
            if(is_LPDDR4) {
                channel->children[addr_vec[int(T::Level::Rank)]]->
                    children[addr_vec[int(T::Level::Bank)]]->just_opened = false;
            }
            else {
                assert(false && "Not implemented for the current DRAM standard.");
            }
        }

        // CROW
        load_default_timing(channel);
        // END - CROW

        //if(channel->spec->is_closing(cmd) && !channel->spec->is_accessing(cmd)){
        if(cmd == T::Command::PRE){
            if(rowtable->get_hits(addr_vec, true) == 0){
                useless_activates++;
                //printf("Useless Activate \n"); // DEBUG

                //if(warmup_complete)
                //    assert(false); // DEBUG
            }
        }

        if(cmd == T::Command::ACT){
            // cout << "ACT Bank " << channel->get_flat_bank_id(addr_vec.data(), 0) << " Row " << dec << addr_vec[int(T::Level::Row)];
            vector<int> act_addr_to_consider = {-1, -1, -1, -1, -1, -1, -1, -1};
            for (int i = 0 ; i <= int(T::Level::Column) ; i++) {
              act_addr_to_consider[i] = addr_vec[i];
            }
            int adj_row_refresh_cnt = 0;
            // cout << "Get Row to Refresh" << endl;
            int row_to_refresh = channel->get_row_to_refresh(
              act_addr_to_consider.data(),
              clk,
              adj_row_refresh_cnt,
              is_real_hammer,
              coreid
            );
            // cout << " Row to refresh is " << dec << (int) row_to_refresh << endl;
            while(row_to_refresh >= 0){
                vector<int> row_to_refresh_addr_vec = {-1, -1, -1, -1, -1, -1, -1, -1};
                for (int i = 0 ; i <= int(T::Level::Column) ; i++) {
                    row_to_refresh_addr_vec[i] = addr_vec[i];
                }
                // printf("\n  Need to change the row id from %d to %d.\n",addr_vec[int(T::Level::Row)], row_to_refresh); fflush(stdout);
                row_to_refresh_addr_vec[int(T::Level::Row)] = row_to_refresh;
                //printf("  Target row is %d.\n",row_to_refresh_addr_vec[int(T::Level::Row)]); fflush(stdout);
                Request newreq(row_to_refresh_addr_vec, Request::Type::ACT);
  	            newreq.coreid = coreid;
                assert(this->ok_to_enqueue(newreq));
                this->enqueue(newreq);
                // assert(res);
                // target_row_to_refresh = row_to_refresh;
                adj_row_refresh_cnt ++;
                row_to_refresh = channel->get_row_to_refresh(addr_vec.data(), clk, adj_row_refresh_cnt, is_real_hammer, coreid);
            }
        }

        if (cmd == T::Command::RD){
            issued_read++;
        }

        // Let the channel know that it is a refresh tick (mainly for RowHammer defense mechanisms and stats collection)
        int num_trr = (channel->spec->is_refreshing(cmd)) ? channel->issue_refresh(addr_vec.data(), clk) : 0;
        // Sending issued command to DRAMPower. 
        if (this->collect_energy){
            drampower->doCommand(DRAMPower::MemCommand::getTypeFromName(cmd_name), flat_bank_id, clk, num_trr);
            update_drampower_counters ++;
            if (update_drampower_counters == 1000){
                drampower->updateCounters(false, clk); //not the last update
                update_drampower_counters = 0;
            } 
        }

        rowtable->update(cmd, addr_vec, clk);

        if (record_cmd_trace){
            // select rank
            auto& file = cmd_trace_files[addr_vec[1]];
            string& cmd_name = channel->spec->command_name[int(cmd)];

            if(cmd_name != "SASEL") {
                if((cmd_name == "ACT") && (crow_copy || (crow_hit > 0)) && (!enable_crow_upperbound))
                    file << clk << ',' << "ACTD";
                else
                    file << clk << ',' << cmd_name;
                // TODO bad coding here
                if (cmd_name == "PREA" || cmd_name == "REF")
                    file<<endl;
                else{
                    int bank_id = 0;
                    if(channel->spec->standard_name == "SALP-MASA")
                        bank_id = addr_vec[int(T::Level::Bank)]*channel->spec->org_entry.count[int(T::Level::Bank) + 1] + addr_vec[int(T::Level::Bank) + 1];
                    else
                        bank_id = addr_vec[int(T::Level::Bank)];
                    if (channel->spec->standard_name == "DDR4" || channel->spec->standard_name == "GDDR5")
                        bank_id += addr_vec[int(T::Level::Bank) - 1] * channel->spec->org_entry.count[int(T::Level::Bank)];
                    file<<','<<bank_id<<endl;
                }
            }
        }
        if (print_cmd_trace){
            printf("%5s %10ld:", channel->spec->command_name[int(cmd)].c_str(), clk);
            for (int lev = 0; lev < int(T::Level::MAX); lev++)
                printf(" %5d", addr_vec[lev]);
            printf("\n");
        }
    }
    vector<int> get_addr_vec(typename T::Command cmd, list<Request>::iterator req){
        return req->addr_vec;
    }

    void load_timing(DRAM<T>* node, vector<typename T::TimingEntry> timing[][int(T::Command::MAX)]) {
        node->set_timing(timing[int(node->level)]);

        for(auto child : node->children)
            load_timing(child, timing);
    }

    void load_default_timing(DRAM<T>* node) {
        node->set_timing(node->spec->timing[int(node->level)]);

        for(auto child : node->children)
            load_default_timing(child);
    }

   void initialize_crow() {

       // 1. timing for CROW table hit on partially restored row when
       // intended to partially restore
       initialize_crow_timing(partial_crow_hit_partial_restore_timing, trcd_crow_partial_hit,
               tras_crow_partial_hit_partial_restore, twr_partial_restore, 1.0f /*crow_hit_tfaw*/);

       // 2. timing for CROW table hit on partially restored row when
       // intended to fully restore
       initialize_crow_timing(partial_crow_hit_full_restore_timing, trcd_crow_partial_hit,
               tras_crow_partial_hit_full_restore, twr_full_restore, 1.0f);

       // 3. timing for CROW table hit on fully restored row when
       // intended to partially restore
       initialize_crow_timing(full_crow_hit_partial_restore_timing, trcd_crow_full_hit,
               tras_crow_full_hit_partial_restore, twr_partial_restore, 1.0f);

       // 4. timing for CROW table hit on fully restored row when
       // intended to fully restore
       initialize_crow_timing(full_crow_hit_full_restore_timing, trcd_crow_full_hit,
               tras_crow_full_hit_full_restore, twr_full_restore, 1.0f);

       // 5. timing for CROW copy
       initialize_crow_timing(crow_copy_timing, 1.0f, tras_crow_copy_full_restore, twr_full_restore, 1.0f);

       if(enable_crow_upperbound)
           return;

       if(crow_table != nullptr)
           delete crow_table;

       crow_table = new CROWTable<T>(channel->spec, channel->id, num_SAs, copy_rows_per_SA, 
                weak_rows_per_SA, crow_evict_threshold, crow_half_life, crow_to_mru_frac,
                crow_table_grouped_SAs);

       ref_counters = new int[channel->spec->org_entry.count[int(T::Level::Rank)]];
       for(int i = 0; i < channel->spec->org_entry.count[int(T::Level::Rank)]; i++)
           ref_counters[i] = 0;
   }

   void initialize_crow_timing(vector<typename T::TimingEntry> timing[][int(T::Command::MAX)], const float trcd_factor,
                   const float tras_factor, const float twr_factor, const float tfaw_factor) {


       // copy the default timing parameters
       for(unsigned int l = 0; l < int(T::Level::MAX); l++) {
           for(unsigned int c = 0; c < int(T::Command::MAX); c++) {
               timing[l][c] = channel->spec->timing[l][c];
           }
       }

       vector<typename T::TimingEntry>* t;
       int trcd = 0, tras = 0;

       // apply trcd_factor to the related timing params
       t = timing[int(T::Level::Bank)];

       for (auto& t : t[int(T::Command::ACT)]) {
           if((t.cmd == T::Command::RD) || (t.cmd == T::Command::RDA)){
            //printf("Default ACT-to-RD cycles: %d\n", t.val);
               t.val = (int)ceil(t.val * trcd_factor);
               trcd = t.val;
            //printf("New ACT-to-RD cycles: %d\n", t.val);
           }

           if((t.cmd == T::Command::WR) || (t.cmd == T::Command::WRA)) {
            //printf("Default ACT-to-WR cycles: %d\n", t.val);
               t.val = (int)ceil(t.val * trcd_factor);
            //printf("New ACT-to-WR cycles: %d\n", t.val);
           }
       }

       // apply tras_factor to the related timing parameters
       t = timing[int(T::Level::Rank)];

       for (auto& t : t[int(T::Command::ACT)]) {
           if(t.cmd == T::Command::PREA){
            //printf("Default ACT-to-PREA cycles: %d\n", t.val);
              t.val = (int)ceil(t.val * tras_factor);
              tras = t.val;
            //printf("New ACT-to-PREA cycles: %d\n", t.val);
           }
       }

       t = timing[int(T::Level::Bank)];

       for (auto& t : t[int(T::Command::ACT)]) {
           if(t.cmd == T::Command::PRE) {
            //printf("Default ACT-to-PRE cycles: %d\n", t.val);
               t.val = (int)ceil(t.val * tras_factor);
            //printf("New ACT-to-PRE cycles: %d\n", t.val);
           }
       }

       // apply both trcd_factor and tras_factor to tRC
       assert(trcd != 0 && tras !=0 && "tRCD or tRAS was not set.");
       t = timing[int(T::Level::Bank)];

       for (auto& t : t[int(T::Command::ACT)]) {
           if(t.cmd == T::Command::ACT) {
            //printf("Default ACT-to-ACT cycles: %d\n", t.val);
               t.val = trcd + tras;
            //printf("New ACT-to-ACT cycles: %d\n", t.val);
           }
       }

       // apply twr_factor to the related timing parameters
       t = timing[int(T::Level::Rank)];

       for (auto& t : t[int(T::Command::WR)]) {
           if(t.cmd == T::Command::PREA) {
            //printf("Default WR-to-PREA cycles: %d\n", t.val);
               t.val = channel->spec->speed_entry.nCWL + channel->spec->speed_entry.nBL +
                           (int)ceil(channel->spec->speed_entry.nWR*twr_factor);
            //printf("New WR-to-PREA cycles: %d\n", t.val);
           }
       }


       t = timing[int(T::Level::Bank)];

       for (auto& t : t[int(T::Command::WR)]) {
           if(t.cmd == T::Command::PRE) {
            //printf("Default WR-to-PRE cycles: %d\n", t.val);
               t.val = channel->spec->speed_entry.nCWL + channel->spec->speed_entry.nBL +
                           (int)ceil(channel->spec->speed_entry.nWR*twr_factor);
            //printf("New WR-to-PRE cycles: %d\n", t.val);
           }
       }

       for (auto& t : t[int(T::Command::WRA)]) {
           if(t.cmd == T::Command::ACT) {
               //printf("Default WRA-to-ACT cycles: %d\n", t.val);
               t.val = channel->spec->speed_entry.nCWL + channel->spec->speed_entry.nBL +
                           (int)ceil(channel->spec->speed_entry.nWR*twr_factor) +
                           channel->spec->speed_entry.nRP;
               //printf("New WRA-to-ACT cycles: %d\n", t.val);
           }

       }

       // apply tfaw_factor to the related timing parameters
       t = timing[int(T::Level::Rank)];

       for (auto& t : t[int(T::Command::ACT)]) {
           if(t.cmd == T::Command::ACT && (t.dist == 4)) {
               //printf("Default ACT-to-ACT (tFAW) cycles: %d\n", t.val);
               t.val = (int)ceil(t.val*tfaw_factor);
               //printf("New ACT-to-ACT (tFAW) cycles: %d\n", t.val);
           }

       }
    }

    void crow_set_FR_on_PRE(typename T::Command cmd, const vector<int>& addr_vec) {

        if(cmd != T::Command::PRE) {
            vector<int> cur_addr_vec = addr_vec;

            int bank_levels = int(T::Level::Bank) - int(T::Level::Rank);

            switch(bank_levels) {
                case 1:
                    for(int i = 0; i < channel->spec->org_entry.count[int(T::Level::Bank)]; i++) {
                        cur_addr_vec[int(T::Level::Bank)] = i;
                        crow_set_FR_on_PRE_single_bank(cur_addr_vec);
                    }
                    break;
                case 2:
                    for(int i = 0; i < channel->spec->org_entry.count[int(T::Level::Bank) - 1];
                            i++) {
                        cur_addr_vec[int(T::Level::Bank) - 1] = i;
                        for(int j = 0; j < channel->spec->org_entry.count[int(T::Level::Bank)];
                                j++) {
                            cur_addr_vec[int(T::Level::Bank)] = j;
                            crow_set_FR_on_PRE_single_bank(cur_addr_vec);
                        }
                    }

                    break;
                default:
                    assert(false && "Not implemented!");
            }
        } else {
            crow_set_FR_on_PRE_single_bank(addr_vec);
        }

    }

    void crow_set_FR_on_PRE_single_bank(const vector<int>& addr_vec) {

        // get the id of the row to be precharged
        int pre_row = rowtable->get_open_row(addr_vec);

        if(pre_row == -1)
            return;

        auto crow_addr_vec = addr_vec;
        crow_addr_vec[int(T::Level::Row)] = pre_row;

        if(!crow_table->is_hit(crow_addr_vec)) // An active row may not be in crow_table right after the warmup period finished
            return;


        // get the next SA to be refreshed
        const float ref_period_threshold = 0.4f;

        int nREFI = channel->spec->speed_entry.nREFI;
        float tCK = channel->spec->speed_entry.tCK;
        int SA_size;
        int next_SA_to_ref;


        if(is_DDR4 || is_LPDDR4) {
           SA_size = channel->spec->org_entry.count[int(T::Level::Row)]/num_SAs;
        }
        else { // for SALP
            SA_size = channel->spec->org_entry.count[int(T::Level::Row)];
        }

        next_SA_to_ref = ref_counters[addr_vec[int(T::Level::Rank)]]/SA_size;

        float trefi_in_ns = nREFI * tCK;

        int SA_id;
        if(is_DDR4 || is_LPDDR4)
            SA_id = pre_row/SA_size;
        else // for SALP
            SA_id = addr_vec[int(T::Level::Row) - 1];


        int base_refw = is_LPDDR4 ? 32*refresh_mult : 64*refresh_mult;
        unsigned long ticks_in_ref_int = (base_refw)*1000000/tCK;
        int num_refs = ticks_in_ref_int/nREFI;

        int rows_per_bank;

        if(is_DDR4 || is_LPDDR4)
            rows_per_bank = channel->spec->org_entry.count[int(T::Level::Row)];
        else // for SALP
            rows_per_bank = channel->spec->org_entry.count[int(T::Level::Row)] * channel->spec->org_entry.count[int(T::Level::Row) - 1];

        int num_rows_refreshed_at_once = ceil(rows_per_bank/(float)num_refs);

        int SA_diff = (next_SA_to_ref > SA_id) ? (num_SAs - (next_SA_to_ref - SA_id)) : SA_id - next_SA_to_ref;

        long time_diff = (long)(SA_diff * (trefi_in_ns*(SA_size/(float)num_rows_refreshed_at_once)));
        bool refresh_check = time_diff > ((base_refw*1000000*refresh_mult) * ref_period_threshold);


        // get the restoration time applied
        long applied_restoration = channel->cycles_since_last_act(addr_vec, clk);
        bool restore_check = (applied_restoration < channel->spec->speed_entry.nRAS);

        if(refresh_check && restore_check)
            crow_num_fr_set++;
        else
            crow_num_fr_notset++;

        if(refresh_check)
            crow_num_fr_ref++;

        if(restore_check)
            crow_num_fr_restore++;

        crow_table->set_FR(crow_addr_vec, refresh_check && restore_check);

    }

    bool upgrade_prefetch_req (Queue& q, const Request& req) {
        if(q.size() == 0)
            return false;

        auto pref_req = find_if(q.q.begin(), q.q.end(), [req](Request& preq) {
                                                return req.addr == preq.addr;});

        if (pref_req != q.q.end()) {
            pref_req->type = Request::Type::READ;
            pref_req->callback = pref_req->proc_callback; // FIXME: proc_callback is an ugly workaround
            return true;
        }

        return false;
    }

    // FIXME: ugly
    bool upgrade_prefetch_req (deque<Request>& p, const Request& req) {
        if (p.size() == 0)
            return false;

        auto pref_req = find_if(p.begin(), p.end(), [req](Request& preq) {
                                                return req.addr == preq.addr;});

        if (pref_req != p.end()) {
            pref_req->type = Request::Type::READ;
            pref_req->callback = pref_req->proc_callback; // FIXME: proc_callback is an ugly workaround
            return true;
        }

        return false;
    }

    vector<typename T::TimingEntry> partial_crow_hit_partial_restore_timing[int(T::Level::MAX)][int(T::Command::MAX)];
    vector<typename T::TimingEntry> partial_crow_hit_full_restore_timing[int(T::Level::MAX)][int(T::Command::MAX)];
    vector<typename T::TimingEntry> full_crow_hit_partial_restore_timing[int(T::Level::MAX)][int(T::Command::MAX)];
    vector<typename T::TimingEntry> full_crow_hit_full_restore_timing[int(T::Level::MAX)][int(T::Command::MAX)];
    vector<typename T::TimingEntry> crow_copy_timing[int(T::Level::MAX)][int(T::Command::MAX)];

  float trcd_crow_partial_hit = 1.0f, trcd_crow_full_hit = 1.0f;

  float tras_crow_partial_hit_partial_restore = 1.0f, tras_crow_partial_hit_full_restore = 1.0f;
  float tras_crow_full_hit_partial_restore = 1.0f, tras_crow_full_hit_full_restore = 1.0f;
  float tras_crow_copy_partial_restore = 1.0f, tras_crow_copy_full_restore = 1.0f;

  float twr_partial_restore = 1.0f, twr_full_restore = 1.0f;

    map<int, int> row_act_hist[8][128]; // hardcoded for 8 bank and 128 SAs per bank (assuming 512-row SAs and 64K rows per bank)

};

// template <>
// vector<int> Controller<SALP>::get_addr_vec(
//     SALP::Command cmd, list<Request>::iterator req);

// template <>
// bool Controller<SALP>::is_ready(list<Request>::iterator req);

// template <>
// void Controller<SALP>::initialize_crow_timing(vector<SALP::TimingEntry> timing[]
//         [int(SALP::Command::MAX)], const float trcd_factor, const float tras_factor,
//         const float twr_factor, const float tfaw_factor);

//template <>
//void Controller<SALP>::update_crow_table_inv_index();

template <>
void Controller<ALDRAM>::update_temp(ALDRAM::Temp current_temperature);

//template <>
//void Controller<TLDRAM>::tick();

} /*namespace ramulator*/

#endif /*__CONTROLLER_H*/
