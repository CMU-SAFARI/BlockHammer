#ifndef __DRAM_H
#define __DRAM_H

#include "Statistics.h"
#include "RowHammerDefense.h"
#include "Request.h"
#include <iostream>
#include <vector>
#include <deque>
#include <map>
#include <functional>
#include <algorithm>
#include <cassert>
#include <type_traits>

using namespace std;

namespace ramulator
{

template <typename T>
class DRAM
{
public:
    ScalarStat active_cycles;
    ScalarStat refresh_cycles;
    ScalarStat busy_cycles;
    ScalarStat active_refresh_overlap_cycles;

    ScalarStat serving_requests;
    ScalarStat average_serving_requests;

    RowhammerDefense * rhdef;
    int rhdef_lvl;
    // ScalarStat rowhammer_warnings;
    // DistributionStat rowhammer_probability;
    ScalarStat opened_cycles; // Hasan

    bool just_opened = false; // Hasan: this is supposed to be used for banks.
                              // Whenever a bank is opened, this will be true until the bank
                              // services a column command, i.e., read or write. I implemented this
                              // to prevent unwanted back to back ACT-PRE commands

    long cur_clk = 0;
    string identifier_str;
    // Constructor
    DRAM(T* spec, const Config& configs, typename T::Level level, int id, string identifier);
    ~DRAM();

    bool is_DDR4 = false, is_LPDDR4 = false; // Hasan

    // Specification (e.g., DDR3)
    T* spec;

    // Tree Organization (e.g., Channel->Rank->Bank->Row->Column)
    typename T::Level level;
    int id;
    long size;
    DRAM* parent;
    vector<DRAM*> children;

    // State (e.g., Opened, Closed)
    typename T::State state;

    // State of Rows:
    // There are too many rows for them to be instantiated individually
    // Instead, their bank (or an equivalent entity) tracks their state for them
    map<int, typename T::State> row_state;

    // Insert a node as one of my child nodes
    void insert(DRAM<T>* child);

    void update_num_subarrays(int num_sa); // Hasan

    // Decode a command into its "prerequisite" command (if any is needed)
    typename T::Command decode(typename T::Command cmd, const int* addr);
    typename T::Command decode_iteratively(typename T::Command cmd, const int* addr);

    // Check whether a command is ready to be scheduled
    bool check(typename T::Command cmd, const int* addr, long clk);
    bool check_iteratively(typename T::Command cmd, const int* addr, long clk, bool is_verbose);

    // CROW
    bool check_no_trcd(typename T::Command cmd, const int* addr, long clk);
    bool check_no_tras(typename T::Command cmd, const int* addr, long clk);
    // END - CROW

    // Check whether a command is a row hit
    bool check_row_hit(typename T::Command cmd, const int* addr);

    // Check whether a row is open
    bool check_row_open(typename T::Command cmd, const int* addr);

    // Return the earliest clock when a command is ready to be scheduled
    long get_next(typename T::Command cmd, const int* addr);

    // Update the timing/state of the tree, signifying that a command has been issued
    void update(typename T::Command cmd, const int* addr, long clk);
    // Update statistics:

    // Update the number of requests it serves currently
    void update_serving_requests(const int* addr, int delta, long clk);

    int get_num_banks();
    int get_flat_bank_id(const int * addr, int base);

    // CROW
    int cycles_since_last_act(const vector<int>& addr_vec, long clk) {

        return cycles_since_last_act(addr_vec.data(), clk);

    }

    int cycles_since_last_act(const int* addr, long clk) {
        if(level == spec->scope[int(T::Command::ACT)] || !children.size()){
            assert(prev[int(T::Command::ACT)].size() == 1);
            return (clk - prev[int(T::Command::ACT)][0]);
        } else
            return children[addr[int(level) + 1]]->cycles_since_last_act(addr, clk);
    }

    int cycles_since_last_act(long clk) {
        // Should be called only from the scope level
        assert(level == spec->scope[int(T::Command::ACT)] || !children.size());
        return cycles_since_last_act(nullptr, clk); // nullptr is okay since addr is not used when already on the scope level
    }

    void set_timing(vector<typename T::TimingEntry>* new_timing) {
        timing = new_timing;
    }
    // END - CROW

    // TIANSHI: current serving requests count
    int cur_serving_requests = 0;
    long begin_of_serving = -1;
    long end_of_serving = -1;
    long begin_of_cur_reqcnt = -1;
    long begin_of_refreshing = -1;
    long end_of_refreshing = -1;
    std::vector<std::pair<long, long>> refresh_intervals;

    // register statistics
    void regStats(const std::string& identifier);

    void collect_opened_cycles(long clk); // Hasan

    void finish(long dram_cycles, long clk);

    // rowhammer observer
    int get_row_to_refresh(const int* addr, long clk, int adj_row_refresh_cnt, bool is_real_hammer, int coreid);
    float get_throttling_coeff(Request& req);
    float get_throttling_coeff(const int* addr_vec, int core_id, bool is_real_hammer); //const int* addr, int coreid)
    // float get_rowhammer_probability(const int* addr, long clk);
    long is_rowhammer_safe(const int* addr, long clk, bool is_real_hammer, int coreid);
    void update_rowhammer_observer_options();
    void update_timings(const Config& config);
    int issue_refresh(const int* addr, long clk);
    int num_total_rows;

private:
    // Constructor
    DRAM(){}
    const Config& configs;
    // Timing
    long next[int(T::Command::MAX)]; // the earliest time in the future when a command could be ready
    deque<long> prev[int(T::Command::MAX)]; // the most recent history of when commands were issued

    // Lookup table for which commands must be preceded by which other commands (i.e., "prerequisite")
    // E.g., a read command to a closed bank must be preceded by an activate command
    function<typename T::Command(DRAM<T>*, typename T::Command cmd, int)>* prereq;

    // SAUGATA: added table for row hits
    // Lookup table for whether a command is a row hit
    // E.g., a read command to a closed bank must be preceded by an activate command
    function<bool(DRAM<T>*, typename T::Command cmd, int)>* rowhit;
    function<bool(DRAM<T>*, typename T::Command cmd, int)>* rowopen;

    // Lookup table between commands and the state transitions they trigger
    // E.g., an activate command to a closed bank opens both the bank and the row
    function<void(DRAM<T>*, int)>* lambda;

    // Lookup table for timing parameters
    // E.g., activate->precharge: tRAS@bank, activate->activate: tRC@bank
    vector<typename T::TimingEntry>* timing;

    // Helper Functions
    void update_state(typename T::Command cmd, const int* addr, long clk);
    void update_timing(typename T::Command cmd, const int* addr, long clk);

}; /* class DRAM */


// register statistics
template <typename T>
void DRAM<T>::regStats(const std::string& identifier) {
    active_cycles
        .name("active_cycles" + identifier + "_" + to_string(id))
        .desc("Total active cycles for level " + identifier + "_" + to_string(id))
        .precision(0)
        ;
    refresh_cycles
        .name("refresh_cycles" + identifier + "_" + to_string(id))
        .desc("(All-bank refresh only, only valid for rank level) The sum of cycles that is under refresh per memory cycle for level " + identifier + "_" + to_string(id))
        .precision(0)
        .flags(Stats::nozero)
        ;
    busy_cycles
        .name("busy_cycles" + identifier + "_" + to_string(id))
        .desc("(All-bank refresh only. busy cycles only include refresh time in rank level) The sum of cycles that the DRAM part is active or under refresh for level " + identifier + "_" + to_string(id))
        .precision(0)
        ;
    active_refresh_overlap_cycles
        .name("active_refresh_overlap_cycles" + identifier + "_" + to_string(id))
        .desc("(All-bank refresh only, only valid for rank level) The sum of cycles that are both active and under refresh per memory cycle for level " + identifier + "_" + to_string(id))
        .precision(0)
        .flags(Stats::nozero)
        ;
    serving_requests
        .name("serving_requests" + identifier + "_" + to_string(id))
        .desc("The sum of read and write requests that are served in this DRAM element per memory cycle for level " + identifier + "_" + to_string(id))
        .precision(0)
        ;
    average_serving_requests
        .name("average_serving_requests" + identifier + "_" + to_string(id))
        .desc("The average of read and write requests that are served in this DRAM element per memory cycle for level " + identifier + "_" + to_string(id))
        .precision(6)
        ;

    opened_cycles
        .name("opened_cycles" + identifier + "_" + to_string(id))
        .desc("Total cycles during which the level " + identifier + "_" + to_string(id) + " had an opened row.")
        .precision(0)
        ;

    if (!children.size()) {
      return;
    }

    // recursively register children statistics
    for (auto child : children) {
      child->regStats(identifier + "_" + to_string(id));
    }
}

template <typename T>
void DRAM<T>::finish(long dram_cycles, long clk) {
  // finalize busy cycles
  busy_cycles = active_cycles.value() + refresh_cycles.value() - active_refresh_overlap_cycles.value();

  // finalize average serving requests
  average_serving_requests = serving_requests.value() / dram_cycles;

  //finalize Rowhammer Observer stats
  //if (int(level) + 1 == int(T::Level::Row))
  if ( int(level) == rhdef_lvl)
    this->rhdef->finish(clk);

  if (!children.size()) {
    return;
  }

  for (auto child : children) {
    child->finish(dram_cycles, clk);
  }
}

// Constructor
template <typename T>
DRAM<T>::DRAM(T* spec, const Config& configs, typename T::Level level, int id, const string identifier) :
    spec(spec), level(level), id(0), parent(NULL), configs(configs)
{

    state = spec->start[(int)level];
    prereq = spec->prereq[int(level)];
    rowhit = spec->rowhit[int(level)];
    rowopen = spec->rowopen[int(level)];
    lambda = spec->lambda[int(level)];
    timing = spec->timing[int(level)];
    num_total_rows = spec->org_entry.count[int(T::Level::Row)];

    this->id = id;

    fill_n(next, int(T::Command::MAX), -1); // initialize future
    for (int cmd = 0; cmd < int(T::Command::MAX); cmd++) {
        int dist = 0;
        for (auto& t : timing[cmd])
            dist = max(dist, t.dist);

        if (dist)
            prev[cmd].resize(dist, -1); // initialize history
    }

    // initialize identifier string of the component
    if (identifier != "") this->identifier_str = identifier + "_" + to_string(id);
    else this->identifier_str = to_string(id);

    // cout << "Created " << this->identifier_str << " at level " << int(level) << endl;

    // Adjust the level at which RowHammer Observer works
    rhdef_lvl = configs.get_int("rowhammer_defense_level");
    if (rhdef_lvl == -1) rhdef_lvl = int(T::Level::Row)-1; 
    // try to recursively construct my children
    int child_level = int(level) + 1;
    if (int(level) == rhdef_lvl)
    {
    //   cout << "Creating a rowhammer defense in this component." << endl;
      rhdef = new RowhammerDefense(configs, spec->speed_entry.tCK, spec->speed_entry.nREFI, spec->speed_entry.nRC, this->identifier_str, this->num_total_rows);
    
    }
    if (child_level == int(T::Level::Row))
    {
      return; // stop recursion: rows are not instantiated as nodes
    }

    int child_max = spec->org_entry.count[child_level];
    if (!child_max)
        return; // stop recursion: the number of children is unspecified

    // recursively construct my children
    for (int i = 0; i < child_max; i++) {
        DRAM<T>* child = new DRAM<T>(spec, configs, typename T::Level(child_level), i, this->identifier_str);
        child->parent = this;
        children.push_back(child);
    }

    is_DDR4 = spec->standard_name == "DDR4";
    is_LPDDR4 = spec->standard_name == "LPDDR4";

}

template <typename T>
DRAM<T>::~DRAM()
{
    for (auto child: children)
        delete child;

    if ( int(level) == rhdef_lvl)
        delete rhdef;

}

template <typename T>
void DRAM<T>::update_num_subarrays(int num_sa) {

    if(level == T::Level::Bank) {
        while(children.size() < num_sa) {
            DRAM<T>* child = new DRAM<T>(spec, configs, typename T::Level(int(level) + 1), children.size(), this->identifier_str);
            //printf("DRAM: Created new child for bank %d\n", id);
            child->parent = this;
            // child->id = children.size();
            children.push_back(child);
        }

        assert(children.size() <= num_sa && "Error: Reducing the number of subarrays not yet supported!");
        return;
    }

    // recursively update my children
    for(int i = 0; i < children.size(); i++) {
        children[i]->update_num_subarrays(num_sa);
    }

}

// Insert
template <typename T>
void DRAM<T>::insert(DRAM<T>* child)
{
    child->parent = this;
    child->id = children.size();
    children.push_back(child);
}

// Decode
template <typename T>
typename T::Command DRAM<T>::decode(typename T::Command cmd, const int* addr)
{
    int child_id = addr[int(level)+1];
    if (prereq[int(cmd)]) {
        typename T::Command prereq_cmd = prereq[int(cmd)](this, cmd, child_id);
        if (prereq_cmd != T::Command::MAX)
            return prereq_cmd; // stop recursion: there is a prerequisite at this level
    }

    if (child_id < 0 || !children.size())
        return cmd; // stop recursion: there were no prequisites at any level

    // recursively decode at my child
    return children[child_id]->decode(cmd, addr);
}

// Decode iteratively, same functionality as decode()
// the aim is improving the performance
template <typename T>
typename T::Command DRAM<T>::decode_iteratively(typename T::Command cmd, const int* addr) {
    int cur_level = int(level);
    int child_id = addr[int(cur_level) + 1];
    auto cur_node = this;

    // Rockey's
    // cout << "This is for level: " << cur_level << "and command: " << int(cmd) << endl;
    if(spec->prereq[cur_level][int(cmd)]) {
        typename T::Command prereq_cmd = spec->prereq[cur_level][int(cmd)](cur_node, cmd, child_id);

        if(prereq_cmd != T::Command::MAX)
            return prereq_cmd;
    }

    while(!(child_id < 0 || (cur_level == (int(T::Level::Row) - 1)))) {
        // go to the child
        cur_node = cur_node->children[child_id];
        cur_level++;
        child_id = addr[int(cur_level) + 1];

        if(spec->prereq[cur_level][int(cmd)]) {
            typename T::Command prereq_cmd = spec->prereq[cur_level][int(cmd)](cur_node, cmd, child_id);
            // if (addr[int(T::Level::Row)] == 12760){
            //   cout << "This is for level: " << cur_level << " and command: " << int(cmd);
            //   cout << "Prereq cmd is " << int(prereq_cmd) << endl;
            // }
            if(prereq_cmd != T::Command::MAX)
                return prereq_cmd;
        }

    }

    return cmd;
}


// Get Number of Banks
template <typename T>
int DRAM<T>::get_num_banks()
{
    if(int(level) == int(T::Level::Bank)) return 1;
    else
    {
        assert(children.size() > 0);
        return children.size() * children[0]->get_num_banks();
    }
}

// Get Flat Bank ID
template <typename T>
int DRAM<T>::get_flat_bank_id(const int * addr, int base)
{
    // cerr << "  Level: " << int(level);
    // cerr << "  Base: " << base;
    // cerr << "  Addr: " << addr[int(level)] << endl << endl;
    
    if(int(level) == int(T::Level::Bank)) return base + addr[int(level)];
    else
    {
        if (children.size() > addr[int(level)+1] && addr[int(level)+1] >= 0)
        {
            return children[addr[int(level)+1]]->get_flat_bank_id(addr, (base + addr[int(level)]) * children.size());
        }
	else{
	    return -1;
	}
    }
}

// Check
template <typename T>
bool DRAM<T>::check(typename T::Command cmd, const int* addr, long clk)
{
    if (next[int(cmd)] != -1 && clk < next[int(cmd)])
        return false; // stop recursion: the check failed at this level

    int child_id = addr[int(level)+1];
    if (child_id < 0 || level == spec->scope[int(cmd)] || !children.size())
        return true; // stop recursion: the check passed at all levels

    // recursively check my child
    return children[child_id]->check(cmd, addr, clk);
}



// Check iteratively, same functionality as check()
// the aim is improving the performance
template <typename T>
bool DRAM<T>::check_iteratively(typename T::Command cmd, const int* addr, long clk, bool is_verbose) {
    int cur_level = int(level);
    int child_id = addr[int(cur_level) + 1];
    auto cur_node = this;

    if(cur_node->next[int(cmd)] != -1 && clk < cur_node->next[int(cmd)]){
      // if (is_verbose) cout << " tp@" << int(cur_level) << " ";
      return false;
    }

    while(!(child_id < 0 || cur_level == int(spec->scope[int(cmd)]) || (cur_level == (int(T::Level::Row) - 1)))){
        // go to the child
        cur_node = cur_node->children[child_id];
        cur_level++;
        child_id = addr[int(cur_level) + 1];

        if(cur_node->next[int(cmd)] != -1 && clk < cur_node->next[int(cmd)]){
          // if (is_verbose) cout << " tp@" << int(cur_level) << " ";
          return false;
        }
    }

    // @Giray: commented this out to enable adjacent row activations
    // if(spec->is_closing(cmd)){// && (cmd != T::Command::PREA)) { // Hasan: make an exception for all-rank PRE, which is currently only used before a refresh
    //                              // as this code below is causing a deadlock otherwise
    //     if(cur_level == int(T::Level::Bank)) {
    //
    //         if(cur_node->just_opened){
    //           if (is_verbose) cout << " jo@" << int(cur_level) << " ";
    //           return false;
    //         }
    //     }
    //     else if(cur_level == int(T::Level::Rank)) {
    //         // check all banks if there is any just opened
    //         for (auto bank : cur_node->children) {
    //             if(bank->just_opened){
    //               if (is_verbose) cout << " jo@" << int(cur_level) << " ";
    //               return false;
    //             }
    //         }
    //     }
    //     else if (cur_level == (int(T::Level::Bank) + 1)) { // for SALP-MASA
    //         if(cur_node->just_opened){
    //           if (is_verbose) cout << " jo@" << int(cur_level) << " ";
    //           return false;
    //         }
    //     }
    //     else
    //         assert(false && "Unimplemented closing command type. PRE and PREA should target bank and rank respectively.");
    // }


    return true;
}


// CROW

template <typename T>
bool DRAM<T>::check_no_trcd(typename T::Command cmd, const int* addr, long clk) {
    if(level == T::Level::Bank)
        return true; // only timing for accessing cmds (RD/WR) is tRCD in bank level.
                     // so always return true in bank level to pretend like
                     // there is no tRCD requirement
                     //
    if (next[int(cmd)] != -1 && clk < next[int(cmd)])
        return false; // stop recursion: the check failed at this level

    int child_id = addr[int(level)+1];
    if (child_id < 0 || level == spec->scope[int(cmd)] || !children.size())
        return true; // stop recursion: the check passed at all levels

    // recursively check my child
    return children[child_id]->check_no_trcd(cmd, addr, clk);
}


template <typename T>
bool DRAM<T>::check_no_tras(typename T::Command cmd, const int* addr, long clk) {

    if(((is_DDR4 || is_LPDDR4) && level == T::Level::Bank)) {
        assert(prev[int(T::Command::ACT)].size() == 1);
        unsigned long restoration = clk - prev[int(T::Command::ACT)][0];
        unsigned long recovery = 0;

        if(prev[int(T::Command::WR)].size() == 1)
            recovery = clk - prev[int(T::Command::WR)][0];

        unsigned long rtp = 0;

        if(prev[int(T::Command::RD)].size() == 1)
            rtp = clk - prev[int(T::Command::RD)][0];

        auto s = spec->speed_entry;

        if(restoration >= s.nRAS) // TODO_hasan: make sure it is >= but not >
            return false;

        if((recovery < (s.nCWL + s.nBL + s.nWR)) || (rtp < s.nRTP)) {
            // waiting to timing related to the previous WR/RD command
            // but not necessarily tRAS
            return false;
        }

        // waiting for tras
        return true;
    }
    if (next[int(cmd)] != -1 && clk < next[int(cmd)])
        return false; // stop recursion: the check failed at this level

    int child_id = addr[int(level)+1];
    if (child_id < 0 || level == spec->scope[int(cmd)] || !children.size())
        return true; // stop recursion: the check passed at all levels

    // recursively check my child
    return children[child_id]->check_no_tras(cmd, addr, clk);
}

// END - CROW

// SAUGATA: added function to check whether a command is a row hit
// Check row hits
template <typename T>
bool DRAM<T>::check_row_hit(typename T::Command cmd, const int* addr)
{
    int child_id = addr[int(level)+1];
    if (rowhit[int(cmd)]) {
        return rowhit[int(cmd)](this, cmd, child_id);  // stop recursion: there is a row hit at this level
    }

    if (child_id < 0 || !children.size())
        return false; // stop recursion: there were no row hits at any level

    // recursively check for row hits at my child
    return children[child_id]->check_row_hit(cmd, addr);
}

template <typename T>
bool DRAM<T>::check_row_open(typename T::Command cmd, const int* addr)
{
    int child_id = addr[int(level)+1];
    if (rowopen[int(cmd)]) {
        return rowopen[int(cmd)](this, cmd, child_id);  // stop recursion: there is a row hit at this level
    }

    if (child_id < 0 || !children.size())
        return false; // stop recursion: there were no row hits at any level

    // recursively check for row hits at my child
    return children[child_id]->check_row_open(cmd, addr);
}

template <typename T>
int DRAM<T>::get_row_to_refresh(const int* addr, long clk, int adj_row_refresh_cnt, bool is_real_hammer, int coreid)
{
    int child_level = int(level)+1;
    int child_id = addr[child_level];
    //if(child_level == int(T::Level::Row))
    if ( int(level) == rhdef_lvl)
    {
        // cout << " defense at level " << int(level);
        return this->rhdef->get_row_to_refresh(child_id, clk, adj_row_refresh_cnt, is_real_hammer, coreid);
    }
    else
    {
        return children[child_id]->get_row_to_refresh(addr, clk, adj_row_refresh_cnt, is_real_hammer, coreid);
    }
}

template <typename T>
float DRAM<T>::get_throttling_coeff(Request& req) //const int* addr, int coreid)
{
  int child_level = int(level)+1;
  int child_id = req.addr_vec.data()[child_level];
  //if(child_level == int(T::Level::Row))
  if ( int(level) == rhdef_lvl)
  {
      return this->rhdef->get_throttling_coeff(child_id, req.coreid, req.is_real_hammer);
  }
  else
  {
      return children[child_id]->get_throttling_coeff(req);
  }
}

template <typename T>
float DRAM<T>::get_throttling_coeff(const int* addr, int core_id, bool is_real_hammer) //const int* addr, int coreid)
{
  int child_level = int(level)+1;
  int child_id = addr[child_level];
  //if(child_level == int(T::Level::Row))
  if ( int(level) == rhdef_lvl)
  {
      return this->rhdef->get_throttling_coeff(child_id, core_id, is_real_hammer);
  }
  else
  {
      return children[child_id]->get_throttling_coeff(addr, core_id, is_real_hammer);
  }
}


// Atb: start breaking refresh
template <typename T>
int DRAM<T>::issue_refresh(const int* addr, long clk)
{
    // int child_level = int(level)+1;
    // int child_id = addr[child_level];
    //if(child_level == int(T::Level::Row)) {
    if ( int(level) == rhdef_lvl)
    {
        // printf("Are we inside of the function issue_refresh, and about to send the clock to rh observer?\n");
        return this->rhdef->refresh_tick(clk);
    }
    else {
        // printf("Are we inside of the function issue_refresh, not yet sending the clock to rh observer?, btw, child level is %d\n", child_level);
        int retval = 0;
	for (auto child : children )
          retval += child->issue_refresh(addr, clk);
	return retval;
    }
}

template <typename T>
long DRAM<T>::is_rowhammer_safe(const int* addr, long clk, bool is_real_hammer, int coreid)
{
    int child_level = int(level)+1;
    int child_id = addr[child_level];
    //if(child_level == int(T::Level::Row))
    if ( int(level) == rhdef_lvl)
    {
        return this->rhdef->is_rowhammer_safe(child_id, clk, is_real_hammer, coreid);
    }
    else
    {
        return children[child_id]->is_rowhammer_safe(addr, clk, is_real_hammer, coreid);
    }
}

template <typename T>
void DRAM<T>::update_rowhammer_observer_options()
{
  int child_level = int(level)+1;
  //if(child_level != int(T::Level::Row))
  if ( int(level) != rhdef_lvl)
    for (auto child : children )
      child->update_rowhammer_observer_options();
  else
    this->rhdef->reload_options();
}

template <typename T>
void DRAM<T>::update_timings(const Config& config)
{
//   if (spec->standard_name.substr(0, 4) == "DDR4"){
    spec->update_timings(config);
//   }
}

template <typename T>
long DRAM<T>::get_next(typename T::Command cmd, const int* addr)
{
    long next_clk = max(cur_clk, next[int(cmd)]);
    auto node = this;
    for (int l = int(level); l < int(spec->scope[int(cmd)]) && node->children.size() && addr[l + 1] >= 0; l++){
        node = node->children[addr[l + 1]];
        next_clk = max(next_clk, node->next[int(cmd)]);
    }
    return next_clk;
}

// Update
template <typename T>
void DRAM<T>::update(typename T::Command cmd, const int* addr, long clk)
{
    //cur_clk = clk;
    update_state(cmd, addr, clk);
    update_timing(cmd, addr, clk);
}


// Update (State)
template <typename T>
void DRAM<T>::update_state(typename T::Command cmd, const int* addr, long clk)
{
    cur_clk = clk;

    int child_id = addr[int(level)+1];
    if (lambda[int(cmd)])
        lambda[int(cmd)](this, child_id); // update this level

    if (level == spec->scope[int(cmd)] || !children.size()) {
        return; // stop recursion: updated all levels
    }

    // recursively update my child
    children[child_id]->update_state(cmd, addr, clk);
}

template <typename T>
void DRAM<T>::collect_opened_cycles(long clk) {
    opened_cycles += cycles_since_last_act(clk);
}


// Update (Timing)
template <typename T>
void DRAM<T>::update_timing(typename T::Command cmd, const int* addr, long clk)
{
    // I am not a target node: I am merely one of its siblings
    if (id != addr[int(level)]) {
        for (auto& t : timing[int(cmd)]) {
            if (!t.sibling)
                continue; // not an applicable timing parameter

            assert (t.dist == 1);

            long future = clk + t.val;
            next[int(t.cmd)] = max(next[int(t.cmd)], future); // update future
        }

        return; // stop recursion: only target nodes should be recursed
    }

    // I am a target node
    if (prev[int(cmd)].size()) {
        prev[int(cmd)].pop_back();  // FIXME TIANSHI why pop back?
        prev[int(cmd)].push_front(clk); // update history
    }

    for (auto& t : timing[int(cmd)]) {
        if (t.sibling)
            continue; // not an applicable timing parameter

        long past = prev[int(cmd)][t.dist-1];
        if (past < 0)
            continue; // not enough history

        long future = past + t.val;
        next[int(t.cmd)] = max(next[int(t.cmd)], future); // update future
        // TIANSHI: for refresh statistics
        if (spec->is_refreshing(cmd) && spec->is_opening(t.cmd)) {
          assert(past == clk);
          begin_of_refreshing = clk;
          end_of_refreshing = max(end_of_refreshing, next[int(t.cmd)]);
          refresh_cycles += end_of_refreshing - clk;
          if (cur_serving_requests > 0) {
            refresh_intervals.push_back(make_pair(begin_of_refreshing, end_of_refreshing));
          }
        }
    }

    // Some commands have timings that are higher that their scope levels, thus
    // we do not stop at the cmd's scope level
    if (!children.size())
        return; // stop recursion: updated all levels

    // recursively update *all* of my children
    for (auto child : children)
        child->update_timing(cmd, addr, clk);

}

template <typename T>
void DRAM<T>::update_serving_requests(const int* addr, int delta, long clk) {
  if(id != addr[int(level)]){
    cout << "update_serving_request was called for the component " << id
         << ", but address is " << addr[int(level)] << "" << endl;
    for (int i = 0 ; i <= int(level) ; i++) cout << int(addr[i]) << " ";
    cout << endl;
    assert(false);
  }
  assert(delta == 1 || delta == -1);
  // update total serving requests
  if (begin_of_cur_reqcnt != -1 && cur_serving_requests > 0) {
    serving_requests += (clk - begin_of_cur_reqcnt) * cur_serving_requests;
    active_cycles += clk - begin_of_cur_reqcnt;
  }
  // update begin of current request number
  begin_of_cur_reqcnt = clk;
  cur_serving_requests += delta;
  assert(cur_serving_requests >= 0);

  if (delta == 1 && cur_serving_requests == 1) {
    // transform from inactive to active
    begin_of_serving = clk;
    if (end_of_refreshing > begin_of_serving) {
      active_refresh_overlap_cycles += end_of_refreshing - begin_of_serving;
    }
  } else if (cur_serving_requests == 0) {
    // transform from active to inactive
    assert(begin_of_serving != -1);
    assert(delta == -1);
    active_cycles += clk - begin_of_cur_reqcnt;
    end_of_serving = clk;

    for (const auto& ref: refresh_intervals) {
      active_refresh_overlap_cycles += min(end_of_serving, ref.second) - ref.first;
    }
    refresh_intervals.clear();
  }

  int child_id = addr[int(level) + 1];
  // We only count the level bank or the level higher than bank
  if (child_id < 0 || !children.size() || (int(level) > int(T::Level::Bank)) ) {
    return;
  }
  children[child_id]->update_serving_requests(addr, delta, clk);
}

} /* namespace ramulator */

#endif /* __DRAM_H */
