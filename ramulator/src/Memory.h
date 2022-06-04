#ifndef __MEMORY_H
#define __MEMORY_H

#include "Config.h"
#include "Cache.h"
#include "DRAM.h"
#include "Request.h"
#include "Controller.h"
#include "SpeedyController.h"
#include "Statistics.h"
#include "GDDR5.h"
#include "HBM.h"
//#include "LPDDR3.h"
#include "LPDDR4.h"
//#include "WideIO2.h"
//#include "DSARP.h"
#include <vector>
#include <functional>
#include <cmath>
#include <cassert>
#include <tuple>

// #define DEBUG_MEMORY
#ifndef DEBUG_MEMORY
#define debug(...)
#else
#define debug(...) do { \
          printf("\033[36m[DEBUG] %s ", __FUNCTION__); \
          printf(__VA_ARGS__); \
          printf("\033[0m\n"); \
      } while (0)
#endif



using namespace std;

typedef vector<unsigned int> MapSrcVector;
typedef map<unsigned int, MapSrcVector > MapSchemeEntry;
typedef map<unsigned int, MapSchemeEntry> MapScheme;

namespace ramulator
{

class MemoryBase{
public:
    MemoryBase() {}
    virtual ~MemoryBase() {}
    virtual double clk_ns() = 0;
    virtual void tick() = 0;
    virtual bool send(Request req) = 0;
    virtual bool upgrade_prefetch_req(long addr) = 0;
    virtual int pending_requests() = 0;
    virtual void finish(void) = 0;
    virtual long page_allocator(long addr, int coreid, Request::Type req_type) = 0;
    virtual void record_core(int coreid) = 0;
};

template <class T, template<typename> class Controller = Controller >
class Memory : public MemoryBase
{
protected:
  ScalarStat dram_capacity;
  ScalarStat num_dram_cycles;
  ScalarStat num_incoming_requests;
  VectorStat num_read_requests;
  VectorStat num_write_requests;
  ScalarStat ramulator_active_cycles;
  VectorStat incoming_requests_per_channel;
  VectorStat incoming_read_reqs_per_channel;

  ScalarStat physical_page_replacement;
  ScalarStat maximum_bandwidth;
  ScalarStat in_queue_req_num_sum;
  ScalarStat in_queue_read_req_num_sum;
  ScalarStat in_queue_write_req_num_sum;
  ScalarStat in_queue_req_num_avg;
  ScalarStat in_queue_read_req_num_avg;
  ScalarStat in_queue_write_req_num_avg;

#ifndef INTEGRATED_WITH_GEM5
  VectorStat record_read_requests;
  VectorStat record_write_requests;
#endif

  long max_address;
  MapScheme mapping_scheme;
  string outdir;

public:
    enum class Type {
        ChRaBaRoCo,
        RoBaRaCoCh,
        MOP4CLXOR,
        MOP1CLXOR,
        MAX,
    } type = Type::MOP4CLXOR;

    enum class Translation {
      None,
      Random,
      MAX,
    } translation = Translation::None;

    std::map<string, Translation> name_to_translation = {
      {"None", Translation::None},
      {"Random", Translation::Random},
    };

    vector<int> free_physical_pages;
    long free_physical_pages_remaining;
    map<pair<int, long>, long> page_translation;
    unsigned int addr_translation_seed;

    vector<Controller<T>*> ctrls;
    T * spec;
    vector<int> addr_bits;
    string mapping_file;
    bool use_mapping_file;
    bool dump_mapping;

    int tx_bits;

    int aggrow_indices [16]; // an index per unique bank 
    int aggressor_rows [16][16]; // supports at most 16 aggressor rows per bank in this mode.
    int aggrow_cnts [16];

    Memory(const Config& configs, vector<Controller<T>*> ctrls)
        : ctrls(ctrls),
          spec(ctrls[0]->channel->spec),
          addr_bits(int(T::Level::MAX))
    {
        // make sure 2^N channels/ranks
        // TODO support channel number that is not powers of 2
        int *sz = spec->org_entry.count;
        assert((sz[0] & (sz[0] - 1)) == 0);
        assert((sz[1] & (sz[1] - 1)) == 0);
        // validate size of one transaction
        int tx = (spec->prefetch_size * spec->channel_width / 8);
        tx_bits = calc_log2(tx);
        assert((1<<tx_bits) == tx);

        // Parsing mapping file and initialize mapping table
        use_mapping_file = false;
        dump_mapping = false;
        printf("Mapping: %s\n",configs["mapping"].c_str());
        if (spec->standard_name.substr(0, 4) == "DDR3" || true){
            if (configs["mapping"] != "default_mapping"){
              init_mapping_with_file(configs.get_str("mapping"));
              // dump_mapping = true;
              use_mapping_file = true;
            }
        }

        outdir = configs.get_str("outdir");
        // If hi address bits will not be assigned to Rows
        // then the chips must not be LPDDRx 6Gb, 12Gb etc.
        if (type != Type::RoBaRaCoCh && spec->standard_name.substr(0, 5) == "LPDDR")
            assert((sz[int(T::Level::Row)] & (sz[int(T::Level::Row)] - 1)) == 0);

        max_address = spec->channel_width / 8;

        for (unsigned int lev = 0; lev < addr_bits.size(); lev++) {
          addr_bits[lev] = calc_log2(sz[lev]);
            max_address *= sz[lev];
        }

        addr_bits[int(T::Level::MAX) - 1] -= calc_log2(spec->prefetch_size);

        // Initiating translation
        if (configs.contains("translation")) {
          translation = name_to_translation[configs.get_str("translation")];
        }
        if (translation != Translation::None) {
          // construct a list of available pages
          // TODO: this should not assume a 4KB page!
          free_physical_pages_remaining = max_address >> 12;

          free_physical_pages.resize(free_physical_pages_remaining, -1);
        }
        addr_translation_seed = 21312; // some constant seed value to make the results reproducible

        // Init aggressor row entries
        for (int i = 0 ; i < 16 ; i++){
        aggrow_indices[i] = 0;
        aggrow_cnts[i] = 0;
        for (int j = 0 ; j < 16 ; j++){
            aggressor_rows[i][j] = -1;
        }
        }

        // Fill aggressor rows with command line arguments
        // -p aggrow0=x_y --> unique bank addr: x, row addr: y
        for(int i = 0 ; i < 16 ; i++){
            string argstr = "aggrow"+to_string(i);
            string new_aggr_row = configs.get_str(argstr);
            //cerr << argstr << ": " << new_aggr_row << endl; 
            if (new_aggr_row != "-1"){
                string delimiter = "_";
                size_t pos = 0;
                pos = new_aggr_row.find(delimiter);
                assert(pos != std::string::npos);
                string unique_bank_id_str = new_aggr_row.substr(0, pos);
                //cerr << unique_bank_id_str << endl;
                int unique_bank_id = stoi(unique_bank_id_str);
                //cerr << unique_bank_id << endl;
                string row_id_str = new_aggr_row.substr(pos+1);
                //cerr << row_id_str << endl;
                int row_id = stoi(row_id_str);
                //cerr << row_id << endl;
                /*
                int token_index = 0;
                string tokens[2];
                while ((pos = new_aggr_row.find(delimiter)) != std::string::npos) {
                        tokens[token_index] = new_aggr_row.substr(0, pos);
                        cerr << token_index << ": " << tokens[token_index] << endl;
                    new_aggr_row.erase(0, pos + delimiter.length());
                    token_index += 1;
                }
                debug("%s", new_aggr_row);
                cerr << "Checkpoint" << endl;
                    aggressor_rows[stoi(tokens[0])][aggrow_cnts[stoi(tokens[0])]] = stoi(tokens[1]);
                aggrow_cnts[stoi(tokens[0])] ++;
                */

                // Record the aggressor row into the array
                aggressor_rows[unique_bank_id][aggrow_cnts[unique_bank_id]] = row_id;
                aggrow_cnts[unique_bank_id] ++;
            }
            else{
                debug("not defined!");
            }
        }
        // for (int i = 0 ; i < 16 ; i++){
        //     cout << "Bank " << i << ": ";
        //     for (int j = 0 ; j < 16 ; j++){
        //         cout << aggressor_rows[i][j] << " ";
        //     }
        //     cout << endl;
        // }

        dram_capacity
            .name("dram_capacity")
            .desc("Number of bytes in simulated DRAM")
            .precision(0)
            ;
        dram_capacity = max_address;

        num_dram_cycles
            .name("dram_cycles")
            .desc("Number of DRAM cycles simulated")
            .precision(0)
            ;
        num_incoming_requests
            .name("incoming_requests")
            .desc("Number of incoming requests to DRAM")
            .precision(0)
            ;
        num_read_requests
            .init(configs.get_int("cores"))
            .name("read_requests")
            .desc("Number of incoming read requests to DRAM per core")
            .precision(0)
            ;
        num_write_requests
            .init(configs.get_int("cores"))
            .name("write_requests")
            .desc("Number of incoming write requests to DRAM per core")
            .precision(0)
            ;
        incoming_requests_per_channel
            .init(sz[int(T::Level::Channel)])
            .name("incoming_requests_per_channel")
            .desc("Number of incoming requests to each DRAM channel")
            ;
        incoming_read_reqs_per_channel
            .init(sz[int(T::Level::Channel)])
            .name("incoming_read_reqs_per_channel")
            .desc("Number of incoming read requests to each DRAM channel")
            ;

        ramulator_active_cycles
            .name("ramulator_active_cycles")
            .desc("The total number of cycles that the DRAM part is active (serving R/W)")
            .precision(0)
            ;
        physical_page_replacement
            .name("physical_page_replacement")
            .desc("The number of times that physical page replacement happens.")
            .precision(0)
            ;
        maximum_bandwidth
            .name("maximum_bandwidth")
            .desc("The theoretical maximum bandwidth (Bps)")
            .precision(0)
            ;
        in_queue_req_num_sum
            .name("in_queue_req_num_sum")
            .desc("Sum of read/write queue length")
            .precision(0)
            ;
        in_queue_read_req_num_sum
            .name("in_queue_read_req_num_sum")
            .desc("Sum of read queue length")
            .precision(0)
            ;
        in_queue_write_req_num_sum
            .name("in_queue_write_req_num_sum")
            .desc("Sum of write queue length")
            .precision(0)
            ;
        in_queue_req_num_avg
            .name("in_queue_req_num_avg")
            .desc("Average of read/write queue length per memory cycle")
            .precision(6)
            ;
        in_queue_read_req_num_avg
            .name("in_queue_read_req_num_avg")
            .desc("Average of read queue length per memory cycle")
            .precision(6)
            ;
        in_queue_write_req_num_avg
            .name("in_queue_write_req_num_avg")
            .desc("Average of write queue length per memory cycle")
            .precision(6)
            ;
#ifndef INTEGRATED_WITH_GEM5
        record_read_requests
            .init(configs.get_int("cores"))
            .name("record_read_requests")
            .desc("record read requests for this core when it reaches request limit or to the end")
            ;

        record_write_requests
            .init(configs.get_int("cores"))
            .name("record_write_requests")
            .desc("record write requests for this core when it reaches request limit or to the end")
            ;
#endif
    }

    ~Memory()
    {
        for (auto ctrl: ctrls)
            delete ctrl;
        delete spec;
    }

    void set_DRAM_sizes(const Config& configs) {
        // make sure 2^N channels/ranks
        // TODO support channel number that is not powers of 2
        int *sz = spec->org_entry.count;
        assert((sz[0] & (sz[0] - 1)) == 0);
        assert((sz[1] & (sz[1] - 1)) == 0);
        // validate size of one transaction
        int tx = (spec->prefetch_size * spec->channel_width / 8);
        tx_bits = calc_log2(tx);
        assert((1<<tx_bits) == tx);
        // If hi address bits will not be assigned to Rows
        // then the chips must not be LPDDRx 6Gb, 12Gb etc.
        if (type != Type::RoBaRaCoCh && spec->standard_name.substr(0, 5) == "LPDDR")
            assert((sz[int(T::Level::Row)] & (sz[int(T::Level::Row)] - 1)) == 0);

        for (unsigned int lev = 0; lev < addr_bits.size(); lev++) {
          addr_bits[lev] = calc_log2(sz[lev]);
        }

        addr_bits[int(T::Level::MAX) - 1] -= calc_log2(spec->prefetch_size);
    }

    double clk_ns()
    {
        return spec->speed_entry.tCK;
    }

    void record_core(int coreid) {
#ifndef INTEGRATED_WITH_GEM5
      record_read_requests[coreid] = num_read_requests[coreid];
      record_write_requests[coreid] = num_write_requests[coreid];
#endif
      for (auto ctrl : ctrls) {
        ctrl->record_core(coreid);
      }
    }

    void tick()
    {
        ++num_dram_cycles;
        int cur_que_req_num = 0;
        int cur_que_readreq_num = 0;
        int cur_que_writereq_num = 0;
        for (auto ctrl : ctrls) {
          cur_que_req_num += ctrl->readq.size() + ctrl->writeq.size() + ctrl->pending.size();
          cur_que_readreq_num += ctrl->readq.size() + ctrl->pending.size();
          cur_que_writereq_num += ctrl->writeq.size();
        }
        in_queue_req_num_sum += cur_que_req_num;
        in_queue_read_req_num_sum += cur_que_readreq_num;
        in_queue_write_req_num_sum += cur_que_writereq_num;

        bool is_active = false;
        for (auto ctrl : ctrls) {
          is_active = is_active || ctrl->is_active();
          ctrl->tick();
        }
        if (is_active) {
          ramulator_active_cycles++;
        }
    }

    bool send(Request req)
    {
        int coreid = req.coreid;
        req.is_resent = true;
        update_addr_vec(req);


        if (req.type == Request::Type::HAMMER){
            int *sz = spec->org_entry.count;
            int unique_bank_id = 0;
            int lvl = int(T::Level::Row)-1;
            // debug("Calculating unique bank id.");
            unique_bank_id += req.addr_vec[lvl] % sz[lvl];
            // debug("Level: %d Address: %d Unique Bank ID: %d", lvl, req.addr_vec[lvl], unique_bank_id); 
            for (lvl=lvl-1 ; lvl >= 0 ; lvl--){
                unique_bank_id += (req.addr_vec[lvl] % sz[lvl]) * sz[lvl+1]; 
                // debug("Level: %d Address: %d Unique Bank ID: %d", lvl, req.addr_vec[lvl], unique_bank_id);
            }
	  
            // debug("%d aggressor rows are defined for bank %d", aggrow_cnts[unique_bank_id], unique_bank_id);
            // for (int i = 0 ; i < aggrow_cnts[unique_bank_id] ; i++){
            //   debug("  aggressor row %d: %d", i, aggressor_rows[unique_bank_id][i]);
            // } 
            // debug("it is turn for index %d", aggrow_indices[unique_bank_id]);
            if (aggrow_cnts[unique_bank_id] > 0){
                //cerr << "aggrow_cnt: " << aggrow_cnt << endl;
                //cerr << "aggrow_index: " << aggrow_index << endl;
                    //cerr << "aggressor_rows[aggrow_index]: " << aggressor_rows[aggrow_index] << endl;
                    
                    //req.addr_vec.resize(addr_bits.size());
                req.addr_vec[int(T::Level::Row)] = aggressor_rows[unique_bank_id][aggrow_indices[unique_bank_id]];
                    // debug("Changing aggressor row to %d", req.addr_vec[int(T::Level::Row)]);
                //req.addr_vec[int(T::Level::Column)] = aggrow_colcnt; 
                //aggrow_colcnt = (aggrow_colcnt + 1) % 128;
                aggrow_indices[unique_bank_id] = (aggrow_indices[unique_bank_id] + 1) % aggrow_cnts[unique_bank_id];
            }

            //debug("New aggressor row here: ");
            //for (int lvl = 0; lvl <= int(T::Level::Row) ; lvl++){
            //  debug("    %d", req.addr_vec[lvl]);
            //}

            // clone the request for each bank and attack
            /*
            bool can_issue = false;
            int *sz = spec->org_entry.count;
            int num_banks = 1;
            for (int lvl = int(T::Level::Row) - 1 ; lvl >= 0; lvl--){
            num_banks *= sz[lvl];
            }
          
            for (int glob_bank_id = 1 ; glob_bank_id < num_banks ; glob_bank_id++){
                vector<int> new_addr_vec = {-1, -1, -1, -1, -1, -1, -1, -1};
                // cout << "glob_bank_id: " << glob_bank_id << " --> addr_vec: ";
                int nb = 1;
                for (int i = 0; i < int(T::Level::Row); i++){
                    new_addr_vec[i] = (glob_bank_id / nb) % sz[i];
                    nb = nb * sz[i];
                    // cout << new_addr_vec[i] << " ";
                }
                new_addr_vec[int(T::Level::Row)] = req.addr_vec[int(T::Level::Row)];
                // cout << new_addr_vec[int(T::Level::Row)] << "." << endl;
                Request newreq(new_addr_vec, Request::Type::READ);
	            // newreq.is_clone = true;
	            newreq.coreid = req.coreid;
                if (ctrls[newreq.addr_vec[0]]->enqueue(newreq)){
                    // cerr << "Successfully enqueued a request to " << newreq.addr_vec[0] << endl;
                    //for (int i = 0; i <= int(T::Level::Row); i++){
                    //    cerr << newreq.addr_vec[i] << " ";
                    //}
                    ++num_incoming_requests;
                    ++incoming_requests_per_channel[newreq.addr_vec[0]];
                    // cerr << ", and incremented the stats counters." << endl;
                    can_issue = true;
                }
            }
	        // */
            //return can_issue;
	    }
        // cerr << "This is not a Hammer type request." << endl; 
        //req.addr_vec.resize(addr_bits.size());
        //long addr = req.addr;

        //// Each transaction size is 2^tx_bits, so first clear the lowest tx_bits bits
        //clear_lower_bits(addr, tx_bits);

        //switch(int(type)){
        //    case int(Type::ChRaBaRoCo):
        //        for (int i = addr_bits.size() - 1; i >= 0; i--)
        //            req.addr_vec[i] = slice_lower_bits(addr, addr_bits[i]);
        //        break;
        //    case int(Type::RoBaRaCoCh):
        //        req.addr_vec[0] = slice_lower_bits(addr, addr_bits[0]);
        //        req.addr_vec[addr_bits.size() - 1] = slice_lower_bits(addr, addr_bits[addr_bits.size() - 1]);
        //        for (int i = 1; i <= int(T::Level::Row); i++)
        //            req.addr_vec[i] = slice_lower_bits(addr, addr_bits[i]);
        //        break;
        //    default:
        //        assert(false);
        //}

        if(!ctrls[req.addr_vec[0]]->ok_to_enqueue(req)) {
        // @Giray: bad coding here, fix later.
            return false;
        }
      if(ctrls[req.addr_vec[0]]->enqueue(req)) {
            // tally stats here to avoid double counting for requests that aren't enqueued
            ++num_incoming_requests;
            if (req.type == Request::Type::READ) {
              ++num_read_requests[coreid];
              ++incoming_read_reqs_per_channel[req.addr_vec[int(T::Level::Channel)]];
            }
            if (req.type == Request::Type::WRITE) {
              ++num_write_requests[coreid];
            }
            ++incoming_requests_per_channel[req.addr_vec[int(T::Level::Channel)]];
            return true;
      }

      return false;
    }

    bool upgrade_prefetch_req(long addr) {
        Request tmp_req;
        tmp_req.addr = addr;
        tmp_req.type = Request::Type::READ;
        update_addr_vec(tmp_req);

        return ctrls[tmp_req.addr_vec[0]]->upgrade_prefetch_req(tmp_req);
    }

    void update_addr_vec(Request& req) {
        req.addr_vec.resize(addr_bits.size());
        long addr = req.addr;
        debug("Byte Address: 0x%x", addr);
        // Each transaction size is 2^tx_bits, so first clear the lowest tx_bits bits
        // cout << "Complete address: 0x" << hex << addr;
        clear_lower_bits(addr, tx_bits);
	    debug("Column Address: 0x%x", addr);
        // cout << ", Address after clearing the lower " << tx_bits << " bits: 0x" << hex << addr << endl;
        if (use_mapping_file){
            apply_mapping(addr, req.addr_vec);
        }
        else {
            switch(int(type)){
                case int(Type::ChRaBaRoCo):
                    for (int i = addr_bits.size() - 1; i >= 0; i--)
                        req.addr_vec[i] = slice_lower_bits(addr, addr_bits[i]);
                    break;
                case int(Type::RoBaRaCoCh):
                    req.addr_vec[0] = slice_lower_bits(addr, addr_bits[0]);
                    req.addr_vec[addr_bits.size() - 1] = slice_lower_bits(addr, addr_bits[addr_bits.size() - 1]);

                    for (int i = 1; i <= int(T::Level::Row); i++)
                        req.addr_vec[i] = slice_lower_bits(addr, addr_bits[i]);
                    break;
                case int(Type::MOP4CLXOR):
                    {
                        //splitting bits into addresses of individual levels 
                        req.addr_vec[int(T::Level::Column)] = slice_lower_bits(addr, 2); // MOP4CL
                        for (int lvl = 0 ; lvl < int(T::Level::Row) ; lvl++){
                            req.addr_vec[lvl] = slice_lower_bits(addr, addr_bits[lvl]);
                            debug("lvl %d: addr:%x", lvl, req.addr_vec[lvl]);
                        }
                        req.addr_vec[int(T::Level::Column)] += slice_lower_bits(addr, addr_bits[int(T::Level::Column)]-2) << 2;
                        req.addr_vec[int(T::Level::Row)] = (int) addr;
                        debug("col addr:%x", req.addr_vec[int(T::Level::Column)]);
                        debug("row addr:%x", req.addr_vec[int(T::Level::Row)]);
                        // Co 1:0 = 1:0
                        // Bg 1:0 = 3:2 12:11
                        // Ba 1:0 = 5:4 14:13
                        // Co 6:2 = 10:6
                        // Ro 31:0 = 42:11
		    
                        // now we need to xor some bits
                        int row_xor_index = 0; 
                        for (int lvl = 0 ; lvl < int(T::Level::Row) ; lvl++){
                            if (addr_bits[lvl] > 0){
                        int mask = (req.addr_vec[int(T::Level::Row)] >> row_xor_index) & ((1<<addr_bits[lvl])-1);
                            req.addr_vec[lvl] = req.addr_vec[lvl] xor mask;
                        row_xor_index += addr_bits[lvl];
                            }
                            debug("lvl %d: addr:%x", lvl, req.addr_vec[lvl]);
                        }
                                
                        break;
	                }
                case int(Type::MOP1CLXOR):
                    {
                        for (int lvl = 0 ; lvl < int(T::Level::Row) ; lvl++){
                            req.addr_vec[lvl] = slice_lower_bits(addr, addr_bits[lvl]);
                        }
                        req.addr_vec[int(T::Level::Column)] = slice_lower_bits(addr, addr_bits[int(T::Level::Column)]);
                        req.addr_vec[int(T::Level::Row)] = (int) addr;
                        debug("col addr:%x", req.addr_vec[int(T::Level::Column)]);
                        debug("row addr:%x", req.addr_vec[int(T::Level::Row)]);
                        // Bg 1:0 = 1:0 12:11
                        // Ba 1:0 = 3:2 14:13
                        // Co 6:0 = 10:4
                        // Ro 31:0 = 42:11

                        // now we need to xor some bits
                        int row_xor_index = 0; 
                        for (int lvl = 0 ; lvl < int(T::Level::Row) ; lvl++){
                            if (addr_bits[lvl] > 0){
                        int mask = (req.addr_vec[int(T::Level::Row)] >> row_xor_index) & ((1<<addr_bits[lvl])-1);
                            req.addr_vec[lvl] = req.addr_vec[lvl] xor mask;
                        row_xor_index += addr_bits[lvl];
                            }
                            debug("lvl %d: addr:%x", lvl, req.addr_vec[lvl]);
                        }
                                
                        break;
                    }
                default:
                    assert(false);
            }
        }
        req.flat_bank_id = ctrls[req.addr_vec[0]]->channel->get_flat_bank_id(req.addr_vec.data(), 0);
    }

    void init_mapping_with_file(string filename){
        ifstream file(filename);
        assert(file.good() && "Bad mapping file");
        // possible line types are:
        // 0. Empty line
        // 1. Direct bit assignment   : component N   = x
        // 2. Direct range assignment : component N:M = x:y
        // 3. XOR bit assignment      : component N   = x y z ...
        // 4. Comment line            : # comment here
        string line;
        char delim[] = " \t";
        while (getline(file, line)) {
            short capture_flags = 0;
            int level = -1;
            int target_bit = -1, target_bit2 = -1;
            int source_bit = -1, source_bit2 = -1;
            // cout << "Processing: " << line << endl;
            bool is_range = false;
            while (true) { // process next word
                size_t start = line.find_first_not_of(delim);
                if (start == string::npos) // no more words
                    break;
                size_t end = line.find_first_of(delim, start);
                string word = line.substr(start, end - start);

                if (word.at(0) == '#')// starting a comment
                    break;

                size_t col_index;
                int source_min, target_min, target_max;
                switch (capture_flags){
                    case 0: // capturing the component name
                        // fetch component level from channel spec
                        for (int i = 0; i < int(T::Level::MAX); i++)
                            if (word.find(T::level_str[i]) != string::npos) {
                                level = i;
                                capture_flags ++;
                            }
                        break;

                    case 1: // capturing target bit(s)
                        col_index = word.find(":");
                        if ( col_index != string::npos ){
                            target_bit2 = stoi(word.substr(col_index+1));
                            //cout << "targe"
                            word = word.substr(0,col_index);
                            is_range = true;
                        }
                        target_bit = stoi(word);
                        capture_flags ++;
                        break;

                    case 2: //this should be the delimiter
                        assert(word.find("=") != string::npos);
                        capture_flags ++;
                        break;

                    case 3:
                        if (is_range){
                            col_index = word.find(":");
                            source_bit  = stoi(word.substr(0,col_index));
                            source_bit2 = stoi(word.substr(col_index+1));
                            if (source_bit2 - source_bit != target_bit2 - target_bit)
                            {
                              cout << source_bit << " " << source_bit2 << " " << target_bit2 << " " << target_bit << endl;
                              assert(false && "bit-widths are not the same.");
                            }
                            source_min = min(source_bit, source_bit2);
                            target_min = min(target_bit, target_bit2);
                            target_max = max(target_bit, target_bit2);
                            while (target_min <= target_max){
                                mapping_scheme[level][target_min].push_back(source_min);
                                // cout << target_min << " <- " << source_min << endl;
                                source_min ++;
                                target_min ++;
                            }
                        }
                        else {
                            source_bit = stoi(word);
                            mapping_scheme[level][target_bit].push_back(source_bit);
                        }
                }
                if (end == string::npos) { // this is the last word
                    break;
                }
                line = line.substr(end);
            }
        }
        if (dump_mapping)
            dump_mapping_scheme();
    }

    void dump_mapping_scheme(){
        cout << "Mapping Scheme: " << endl;
        for (MapScheme::iterator mapit = mapping_scheme.begin(); mapit != mapping_scheme.end(); mapit++)
        {
            int level = mapit->first;
            for (MapSchemeEntry::iterator entit = mapit->second.begin(); entit != mapit->second.end(); entit++){
                cout << T::level_str[level] << "[" << entit->first << "] := ";
                cout << "PhysicalAddress[" << *(entit->second.begin()) << "]";
                entit->second.erase(entit->second.begin());
                for (MapSrcVector::iterator it = entit->second.begin() ; it != entit->second.end(); it ++)
                    cout << " xor PhysicalAddress[" << *it << "]";
                cout << endl;
            }
        }
    }

    void apply_mapping(long addr, std::vector<int>& addr_vec){
        int *sz = spec->org_entry.count;
        int addr_total_bits = sizeof(addr_vec)*8;
        int addr_bits [int(T::Level::MAX)];
        for (int i = 0 ; i < int(T::Level::MAX) ; i ++)
        {
            if ( i != int(T::Level::Row))
            {
                addr_bits[i] = calc_log2(sz[i]);
                addr_total_bits -= addr_bits[i];
            }
        }
        // Row address is an integer.
        addr_bits[int(T::Level::Row)] = min((int)sizeof(int)*8, max(addr_total_bits, calc_log2(sz[int(T::Level::Row)])));

        // printf("Address: %lx => ",addr);
        for (unsigned int lvl = 0; lvl < int(T::Level::MAX); lvl++)
        {
            unsigned int lvl_bits = addr_bits[lvl];
            addr_vec[lvl] = 0;
            for (unsigned int bitindex = 0 ; bitindex < lvl_bits ; bitindex++){
                bool bitvalue = false;
                for (MapSrcVector::iterator it = mapping_scheme[lvl][bitindex].begin() ;
                    it != mapping_scheme[lvl][bitindex].end(); it ++)
                {
                    bitvalue = bitvalue xor get_bit_at(addr, *it);
                }
                addr_vec[lvl] |= (bitvalue << bitindex);
            }
        }
    }

    int pending_requests()
    {
        int reqs = 0;
        for (auto ctrl: ctrls)
            reqs += ctrl->readq.size() + ctrl->writeq.size() + ctrl->otherq.size() + ctrl->pending.size();
        return reqs;
    }

    void finish(void) {
      { // Dump the physical page numbers
        ofstream outfile;
        outfile.open(outdir+"/phys_page_nums.txt", ios::out | ios::trunc);
        for(auto& pt : page_translation){
          outfile << dec << pt.second << endl;
        }
        outfile.close();
      }

      dram_capacity = max_address;
      int *sz = spec->org_entry.count;
      maximum_bandwidth = spec->speed_entry.rate * 1e6 * spec->channel_width * sz[int(T::Level::Channel)] / 8;
      long dram_cycles = num_dram_cycles.value();
      for (auto ctrl : ctrls) {
        long read_req = long(incoming_read_reqs_per_channel[ctrl->channel->id].value());
        ctrl->finish(read_req, dram_cycles);
      }

      // finalize average queueing requests
      in_queue_req_num_avg = in_queue_req_num_sum.value() / dram_cycles;
      in_queue_read_req_num_avg = in_queue_read_req_num_sum.value() / dram_cycles;
      in_queue_write_req_num_avg = in_queue_write_req_num_sum.value() / dram_cycles;
    }

    long page_allocator(long addr, int coreid, Request::Type req_type) {
        long virtual_page_number = addr >> 12;

        //exception for RowHammer traces
      	//this is a bad hack to allocate consecutive pages for RowHammer traces
      	//to ensure hammering different locations, we use an offset based on the coreid
      	//then insert this mapping into the page translation table, so that no other
      	//virt. page is addressed to there.

      	if(req_type == Request::Type::HAMMER){
            auto target = make_pair(coreid, virtual_page_number);
            if(page_translation.find(target) == page_translation.end() ) {
              long phys_page_to_read = virtual_page_number + (coreid << 10);
              // assert(free_physical_pages[phys_page_to_read] != -1);
              page_translation[target] = phys_page_to_read;
              --free_physical_pages_remaining;
            }
            return (page_translation[target] << 12) | (addr & ((1 << 12) - 1));
      	}

        switch(int(translation)) {
            case int(Translation::None): {
              return addr;
            }
            case int(Translation::Random): {
                auto target = make_pair(coreid, virtual_page_number);
                if(page_translation.find(target) == page_translation.end()) {
                    // page doesn't exist, so assign a new page
                    // make sure there are physical pages left to be assigned

                    // if physical page doesn't remain, replace a previous assigned
                    // physical page.
                    if (!free_physical_pages_remaining) {
                      physical_page_replacement++;
                      long phys_page_to_read = lrand() % free_physical_pages.size();
                      assert(free_physical_pages[phys_page_to_read] != -1);
                      page_translation[target] = phys_page_to_read;
                    } else {
                        // assign a new page
                        long phys_page_to_read = lrand() % free_physical_pages.size();
                        // if the randomly-selected page was already assigned
                        if(free_physical_pages[phys_page_to_read] != -1) {
                            long starting_page_of_search = phys_page_to_read;

                            do {
                                // iterate through the list until we find a free page
                                // TODO: does this introduce serious non-randomness?
                                ++phys_page_to_read;
                                phys_page_to_read %= free_physical_pages.size();
                            }
                            while((phys_page_to_read != starting_page_of_search) && free_physical_pages[phys_page_to_read] != -1);
                        }

                        assert(free_physical_pages[phys_page_to_read] == -1);

                        page_translation[target] = phys_page_to_read;
                        free_physical_pages[phys_page_to_read] = coreid;
                        --free_physical_pages_remaining;
                    }
                }

                // SAUGATA TODO: page size should not always be fixed to 4KB
                return (page_translation[target] << 12) | (addr & ((1 << 12) - 1));
            }
            default:
                assert(false);
        }

    }

    void reload_options(const Config& configs) {
        spec->set_subarray_number(configs.get_int("subarrays"));
        set_DRAM_sizes(configs);

        for (auto ctrl : ctrls)
            ctrl->reload_options(configs);
    }

private:

    int calc_log2(int val){
        int n = 0;
        while ((val >>= 1))
            n ++;
        return n;
    }
    int slice_lower_bits(long& addr, int bits)
    {
        int lbits = addr & ((1<<bits) - 1);
        addr >>= bits;
        return lbits;
    }
    bool get_bit_at(long addr, int bit)
    {
        return (((addr >> bit) & 1) == 1);
    }
    void clear_lower_bits(long& addr, int bits)
    {
	debug("Clearing lower %d bits of %ld", bits, addr);
        addr >>= bits;
	debug("Result is %ld", addr);
    }
    long lrand(void) {
        if(sizeof(int) < sizeof(long)) {
            unsigned int rand1 = rand_r(&addr_translation_seed);
            unsigned int rand2 = rand_r(&addr_translation_seed);
            return static_cast<long>(rand1) << (sizeof(int) * 8) | rand2;
        }
        unsigned int rand = rand_r(&addr_translation_seed);
        return rand;
    }
};

} /*namespace ramulator*/

#endif /*__MEMORY_H*/
