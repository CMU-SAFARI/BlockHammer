#ifndef __CONFIG_H
#define __CONFIG_H

#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <iostream>
#include <cassert>
#include <typeinfo>

namespace ramulator
{

class Config {

private:
    std::map<std::string, std::string> options;
    std::map<std::string, std::string> sim_options;
    std::vector<std::string> trace_files;
    bool use_sim_options = false;

    std::map<std::string, std::string> defaults = {
        // DRAM
        {"standard", "LPDDR4"},
        {"speed", "LPDDR4_3200"},
        {"org", "LPDDR4_8Gb_x16"},
        {"channels", "1"},
        {"ranks", "1"},
        {"subarrays", "128"},

        // Memory Controller
        {"row_policy", "opened"},
        {"timeout_row_policy_threshold", "120"},
        {"disable_refresh", "false"},
        {"mem_scheduler", "FRFCFS_PriorHit"},
        {"mapping", "default_mapping"},
        {"dump_mapping", "false"},

        // CPU
        {"cores", "1"},
        {"cpu_tick", "5"},
        {"mem_tick", "2"},
        {"early_exit", "off"},
        {"exit_at_cycle", "off"},
        {"rest_in_peace", "on"},
        {"expected_limit_insts", "200000000"},
        {"expected_limit_cycles", "200000000"},
        {"warmup_insts", "100000000"},
        {"core_limit_instr_exempt_flags", "0"},
        {"translation", "Random"},

        // Cache
        {"cache", "L3"},
        {"l3_size", "4194304"},
        {"prefetcher", "off"}, // "off" or "stride"

        // Stride Prefetcher
        {"stride_pref_entries", "1024"},
        {"stride_pref_mode", "1"}, // 0 -> single stride mode, 1 -> multi stride mode
        {"stride_pref_single_stride_tresh", "6"},
        {"stride_pref_multi_stride_tresh", "6"},
        {"stride_pref_stride_start_dist", "1"},
        {"stride_pref_stride_degree", "4"},
        {"stride_pref_stride_dist", "16"},

        // DRAMPower Integration
        {"drampower_specs_file", "./DRAMPower/memspecs/MICRON_4Gb_DDR4-1866_8bit_A.xml"},
        {"collect_energy", "false"},

        // Other
        {"record_cmd_trace", "off"},
        {"print_cmd_trace", "off"},
        {"collect_row_activation_histogram", "off"},
        {"collect_latencies", "off"},
        {"outdir", "./out/"},
        {"cmd_trace", "cmd_trace_"},
        {"stats","ramulator.stats"},

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

        // CROW
        {"crow_entry_evict_hit_threshold", "0"},
        {"crow_evict_fully_restored", "false"},
        {"crow_half_life", "1000"},
        {"crow_to_mru_frac", "0.0f"},
        {"enable_crow_upperbound", "false"},
        {"enable_tl_dram", "false"},
        {"copy_rows_per_SA", "0"},
        {"weak_rows_per_SA", "0"},
        {"refresh_mult", "1.0f"},
        {"crow_table_grouped_SAs", "1"}, // the number of SAs to be grouped in CROW table to share the entries. 
                                        // This is an optimization to reduce the storage requirement of the CROW table
        // CROW DRAM timing parameters
        {"trcd_crow_partial_hit", "0.79f"},
        {"trcd_crow_full_hit", "0.62f"},
        {"tras_crow_partial_hit_partial_restore", "0.75f"},
        {"tras_crow_full_hit_partial_restore", "0.67f"},
        {"tras_crow_partial_hit_full_restore", "1.01f"},
        {"tras_crow_full_hit_full_restore", "0.93f"},
        {"tras_crow_copy_partial_restore", "0.93f"},
        {"tras_crow_copy_full_restore", "1.18f"},
        {"twr_partial_restore", "0.87f"},
        {"twr_full_restore", "1.14f"},
    };

    template<typename T>
    T get(const std::string& param_name, T (*cast_func)(const std::string&)) const {

        std::string param = this->operator[](param_name);

        if(param == "") {
            param = defaults.at(param_name); // get the default param, if exists

            if(param == "") {
                std::cerr << "ERROR: All options should have their default values in Config.h!" << std::endl;
                std::cerr << "No default value found for: " << param_name << std::endl;
                exit(-1);
            }
        }

        try {
            return (*cast_func)(param); 
        } catch (const std::invalid_argument& ia) {
            std::cerr << "Invalid argument: " << ia.what() << std::endl;
            exit(-1);
        } catch (const std::out_of_range& oor) {
            std::cerr << "Out of Range error: " << oor.what() << std::endl;
            exit(-1);
        } catch (...) {
            std::cerr << "Error! Unhandled exception." << std::endl;
            std::exit(-1);
        }

        return T();
    }

    static bool param_to_bool(const std::string& s) {
        if(s == "true")
            return true;

        if(s == "on")
            return true;

        return false;
    }

    bool sim_contains(const std::string& name) const {
        if(sim_options.find(name) != sim_options.end()) {
            return true;
        } else {
            return false;
        }
    }

public:
    Config() {}
    Config(const std::string& fname);
    void parse(const std::string& fname);
    void parse_cmdline(const int argc, char** argv);
    std::string operator [] (const std::string& name) const {
       if(use_sim_options && sim_contains(name))
           return (sim_options.find(name))->second;

       if (contains(name)) {
         return (options.find(name))->second;
       } else {
         return "";
       }
    }

    int get_int(const std::string& param_name) const {
        // std::cerr << "Getting integer argument " << param_name << std::endl;
        return get<int>(param_name, [](const std::string& s){ return std::stoi(s); }); // Hasan: the lambda function trick helps ignoring the optional argument of stoi
    }
    
    long get_long(const std::string& param_name) const {
        
        return get<long>(param_name, [](const std::string& s){ return std::stol(s); });
    }
    
    float get_float(const std::string& param_name) const {

        return get<float>(param_name, [](const std::string& s){ return std::stof(s); });
    }

    bool get_bool(const std::string& param_name) const {
        return get<bool>(param_name, param_to_bool);
    }

    std::string get_str(const std::string& param_name) const {
        return get<std::string>(param_name, [](const std::string& s){ return s; });
    }

    bool contains(const std::string& name) const {

      if(use_sim_options && sim_contains(name))
          return true;

      if (options.find(name) != options.end()) {
        return true;
      } else {
        return false;
      }
    } 

    void add (const std::string& name, const std::string& value) {

      if(use_sim_options) {
        if(!sim_contains(name))
            sim_options.insert(make_pair(name, value));
        else
            printf("ramulator::Config::add options[%s] already set.\n", name.c_str());

        return;
      }

      if (!contains(name)) {
        options.insert(make_pair(name, value));
      } else {
        printf("ramulator::Config::add options[%s] already set.\n", name.c_str());
      }
    }

    template<typename T>
    void update (const std::string& name, const T& value) {
        if(use_sim_options)
            sim_options[name] = std::to_string(value);
        else
            options[name] = std::to_string(value);
    }

    void enable_sim_options () {
        use_sim_options = true;
    }

    void disable_sim_options () {
        use_sim_options = false;
    }

    void dump(){
      printf("Warmup options:\n");
      for(auto entry : options)
        printf("  %s: %s\n", entry.first.c_str(), entry.second.c_str());

      printf("Simulation options:\n");
      for(auto entry : sim_options)
        printf("  %s: %s\n", entry.first.c_str(), entry.second.c_str());

    }


    bool has_l3_cache() const {
      const std::string& cache_type = get_str("cache");   
      return (cache_type == "all") || (cache_type == "L3");
    }

    bool has_core_caches() const {
      const std::string& cache_type = get_str("cache");   
      return (cache_type == "all" || cache_type == "L1L2");
    }
    

    bool calc_weighted_speedup() const {
      return (get_long("expected_limit_insts") != 0);
    }

    const std::vector<std::string>& get_trace_files() const {
        return trace_files;
    }

    const int get_num_traces() const{return trace_files.size();}
};


} /* namespace ramulator */

#endif /* _CONFIG_H */

