#include "Processor.h"
#include "StridePrefetcher.h"
#include <cassert>

using namespace std;
using namespace ramulator;

Processor::Processor(const Config& configs,
    vector<std::string> trace_list,
    function<bool(Request)> send_memory,
    function<bool(long)> upgrade_prefetch_req,
    MemoryBase& memory,
    vector<float> &throttling_coeffs)
    : ipcs(trace_list.size(), -1),
    early_exit(configs.get_bool("early_exit")),
    exit_at_cycle(configs.get_bool("exit_at_cycle")),
    no_core_caches(!configs.has_core_caches()),
    no_shared_cache(!configs.has_l3_cache()),
    l3_size(configs.get_int("l3_size")), 
    throttling_coeffs_ptr(&throttling_coeffs),
    cachesys(new CacheSystem(configs, send_memory, upgrade_prefetch_req)),
    llc(l3_size, l3_assoc, l3_blocksz,
         mshr_per_bank * trace_list.size(),
         Cache::Level::L3, cachesys, throttling_coeffs_ptr) {

  
  // for (int i = 0 ; i < 64 ; i++)
  //   cout << "Core " << i << " is allowed to use " << throttling_coeffs_ptr->at(i) << " of the queues." << endl;
	    
  assert(cachesys != nullptr);
  int tracenum = trace_list.size();
  assert(tracenum > 0);
  printf("tracenum: %d\n", tracenum);
  for (int i = 0 ; i < tracenum ; ++i) {
    printf("trace_list[%d]: %s\n", i, trace_list[i].c_str());
  }
  if (no_shared_cache) {
    for (int i = 0 ; i < tracenum ; ++i) {
      cores.emplace_back(new Core(
          configs, i, trace_list[i].c_str(), send_memory, nullptr,
          cachesys, memory, throttling_coeffs_ptr));
    }
  } else {
    for (int i = 0 ; i < tracenum ; ++i) {
      cores.emplace_back(new Core(configs, i, trace_list[i].c_str(),
          std::bind(&Cache::send, &llc, std::placeholders::_1),
          &llc, cachesys, memory, throttling_coeffs_ptr));
    }
  }
  for (int i = 0 ; i < tracenum ; ++i) {
    cores[i]->callback = std::bind(&Processor::receive, this,
        placeholders::_1);
  }

  // plug in a prefetcher
  // currently only supporting prefetches to llc, so shared_cache is
  // required
  if (configs.get_str("prefetcher") == "stride") {
      assert(!no_shared_cache && "ERROR: Currently, a shared LLC is required for the stride prefetcher!");

      // Stride prefetcher configuration
      int num_entries = configs.get_int("stride_pref_entries");
      int mode = configs.get_int("stride_pref_mode");
      int ss_thresh = configs.get_int("stride_pref_single_stride_tresh");
      int ms_thresh = configs.get_int("stride_pref_multi_stride_tresh");
      int start_dist = configs.get_int("stride_pref_stride_start_dist");
      int stride_degree = configs.get_int("stride_pref_stride_degree");
      int stride_dist = configs.get_int("stride_pref_stride_dist");

      StridePrefetcher * pref = new StridePrefetcher(num_entries,
                      (StridePrefetcher::StridePrefMode) mode,
                      ss_thresh, ms_thresh, start_dist, stride_degree, stride_dist,
                      std::bind(&Cache::send, &llc, placeholders::_1),
                      std::bind(&Cache::callback, &llc, placeholders::_1),
                      std::bind(&Processor::receive, this, placeholders::_1));

      llc.prefetcher = pref;
  }

  core_limit_instr_exempt_flags = configs.get_int("core_limit_instr_exempt_flags");
  expected_limit_cycles = configs.get_long("expected_limit_cycles");

  // regStats
  cpu_cycles.name("cpu_cycles")
            .desc("cpu cycle number")
            .precision(0)
            ;
  cpu_cycles = 0;
}

void Processor::tick() {
  cpu_cycles++;

  // if((int(cpu_cycles.value()) % 50000000) == 0){
  if((int(cpu_cycles.value()) % 500) == 0){
      printf("\rCPU heartbeat, cycles: %d instr:", (int(cpu_cycles.value())));
      for(auto& core : cores){
        printf(" %d", core->get_insts());
      }
      // printf("\n");
  }
  if (!(no_core_caches && no_shared_cache)) {
    cachesys->tick();
  }
  for (unsigned int i = 0 ; i < cores.size() ; ++i) {
    Core* core = cores[i].get();
    core->tick();
  }
}

void Processor::receive(Request& req) {
  if (!no_shared_cache) {
    llc.callback(req);
  } else if (!cores[0]->no_core_caches) {
    // Assume all cores have caches or don't have caches
    // at the same time.
    for (unsigned int i = 0 ; i < cores.size() ; ++i) {
      Core* core = cores[i].get();
      core->caches[0]->callback(req);
    }
  }
  for (unsigned int i = 0 ; i < cores.size() ; ++i) {
    Core* core = cores[i].get();
    core->receive(req);
  }
}

bool Processor::finished() {
  if (exit_at_cycle){
    if (cpu_cycles.value() >= expected_limit_cycles){
      return true;
    }
  }
  if (early_exit) {
    for (unsigned int i = 0 ; i < cores.size(); ++i) {
      if (cores[i]->finished()) {
        for (unsigned int j = 0 ; j < cores.size() ; ++j) {
          ipc += cores[j]->calc_ipc();
        }
        return true;
      }
    }
    return false;
  } else {
    for (unsigned int i = 0 ; i < cores.size(); ++i) {
      if (!cores[i]->finished()) {
        return false;
      }
      if (ipcs[i] < 0) {
        ipcs[i] = cores[i]->calc_ipc();
        ipc += ipcs[i];
      }
    }
    return true;
  }
}

bool Processor::has_reached_limit() {
  for (unsigned int i = 0 ; i < cores.size() ; ++i) {
    // cout << "Has core " << i << " reached its limit? " << (int) cores[i]->has_reached_limit() << endl;
    // cout << "Is core " << i << " exempted from limit? " << (int) is_core_exempted_from_limit_instr(i) << endl;
    if (!cores[i]->has_reached_limit() && !is_core_exempted_from_limit_instr(i)) {
      return false;
    }
  }
  return true;
}

bool Processor::is_core_exempted_from_limit_instr(int i){
  return (core_limit_instr_exempt_flags >> i) % 2;
}

long Processor::get_insts() {
    long insts_total = 0;
    for (unsigned int i = 0 ; i < cores.size(); i++) {
        insts_total += cores[i]->get_insts();
    }

    return insts_total;
}

void Processor::reset_stats() {
    for (unsigned int i = 0 ; i < cores.size(); i++) {
        cores[i]->reset_stats();
    }

    ipc = 0;

    for (unsigned int i = 0; i < ipcs.size(); i++)
        ipcs[i] = -1;
}

Core::Core(const Config& configs, int coreid,
    const char* trace_fname, function<bool(Request)> send_next,
    Cache* llc, std::shared_ptr<CacheSystem> cachesys, MemoryBase& memory, 
    vector<float> *throttling_coeffs_ptr)
    : id(coreid), no_core_caches(!configs.has_core_caches()),
    no_shared_cache(!configs.has_l3_cache()),
    rest_in_peace(configs.get_bool("rest_in_peace")),
    llc(llc), trace(trace_fname), memory(memory),
    throttling_coeffs_ptr(throttling_coeffs_ptr)
{
  // Build cache hierarchy
  if (no_core_caches) {
    send = send_next;
  } else {
    // L2 caches[0]
    caches.emplace_back(new Cache(
        l2_size, l2_assoc, l2_blocksz, l2_mshr_num,
        Cache::Level::L2, cachesys, throttling_coeffs_ptr));
    // L1 caches[1]
    caches.emplace_back(new Cache(
        l1_size, l1_assoc, l1_blocksz, l1_mshr_num,
        Cache::Level::L1, cachesys, throttling_coeffs_ptr));
    send = bind(&Cache::send, caches[1].get(), placeholders::_1);
    if (llc != nullptr) {
      caches[0]->concatlower(llc);
    }
    caches[1]->concatlower(caches[0].get());
  }
  if (no_core_caches) {
    more_reqs = trace.get_unfiltered_request(
        bubble_cnt, req_addr, req_type, payload);
    req_addr = memory.page_allocator(req_addr, id, req_type);
  } else {
    more_reqs = trace.get_unfiltered_request( //SPEC benchmark compatability
        bubble_cnt, req_addr, req_type, payload);
    req_addr = memory.page_allocator(req_addr, id, req_type);
  }

  cout << hex << req_addr << endl;
  // set expected limit instruction for calculating weighted speedup
  expected_limit_insts = configs.get_long("expected_limit_insts");

  // regStats
  record_cycs.name("record_cycs_core_" + to_string(id))
             .desc("Record cycle number for calculating weighted speedup. (Only valid when expected limit instruction number is non zero in config file.)")
             .precision(0)
             ;

  record_insts.name("record_insts_core_" + to_string(id))
              .desc("Retired instruction number when record cycle number. (Only valid when expected limit instruction number is non zero in config file.)")
              .precision(0)
              ;

  memory_access_cycles.name("memory_access_cycles_core_" + to_string(id))
                      .desc("memory access cycles in memory time domain")
                      .precision(0)
                      ;
  memory_access_cycles = 0;
  cpu_inst.name("cpu_instructions_core_" + to_string(id))
          .desc("cpu instruction number")
          .precision(0)
          ;
  cpu_inst = 0;
}


double Core::calc_ipc()
{
    printf("[%d]retired: %ld, clk, %ld\n", id, retired, clk);
    return (double) retired / clk;
}

void Core::tick()
{
    clk++;

    retired += window.retire();

    // cout << clk << " ---- " << "CPU Ticks, checkpoint 1, request type: " << (int)req_type << endl;

    if (expected_limit_insts == 0 && !more_reqs) return;

    if (reached_limit && rest_in_peace) return;
    // bubbles (non-memory operations)
    int inserted = 0;

    // cout << clk << " ---- " << "CPU Ticks, checkpoint 2, request type: " << (int)req_type << endl;

    while (bubble_cnt > 0) {
        if (inserted == window.ipc) return;
        // cout << clk << " ---- " << bubble_cnt << " ---- " << "CPU Ticks, checkpoint 2.1, request type: " << (int)req_type << endl;
        if (window.is_full()) return;
        // cout << clk << " ---- " << bubble_cnt << " ---- " << "CPU Ticks, checkpoint 2.2, request type: " << (int)req_type << endl;

        window.insert(true, -1);
        // cout << clk << " ---- " << bubble_cnt << " ---- " << "CPU Ticks, checkpoint 2.3, request type: " << (int)req_type << endl;
        inserted++;
        bubble_cnt--;
        cpu_inst++;
        if (long(cpu_inst.value()) == expected_limit_insts && !reached_limit) {
          // cout << clk << " ---- " << bubble_cnt << " ---- " << "CPU Ticks, checkpoint 2.4, request type: " << (int)req_type << endl;
          record_cycs = clk;
          record_insts = long(cpu_inst.value());
          memory.record_core(id);
          // cout << clk << " ---- " << bubble_cnt << " ---- " << "CPU Ticks, checkpoint 2.5, request type: " << (int)req_type << endl;
          reached_limit = true;
        }
        // cout << clk << " ---- " << bubble_cnt << " ---- " << "CPU Ticks, checkpoint 2.6, request type: " << (int)req_type << endl;
    }

    // cout << clk << " ---- " << "CPU Ticks, checkpoint 3, request type: " << (int)req_type << endl;

    if (req_type == Request::Type::READ || req_type == Request::Type::HAMMER) {
        // read request
        if (inserted == window.ipc) return;
        if (window.is_full()) return;

        Request req(req_addr, req_type, callback, id);
        req.payload = payload;
        // cout << clk << " ---- " << "CPU Ticks, checkpoint 4, request type: " << (int)req_type << endl;
        if (!send(req)) return;

        window.insert(false, req_addr);
        cpu_inst++;
    }
    else {
        // write request
        assert(req_type == Request::Type::WRITE);
        Request req(req_addr, req_type, callback, id);
        req.payload = payload;
        if (!send(req)) return;
        cpu_inst++;
    }
    if (long(cpu_inst.value()) == expected_limit_insts && !reached_limit) {
      record_cycs = clk;
      record_insts = long(cpu_inst.value());
      memory.record_core(id);
      reached_limit = true;
    }

    if (no_core_caches) {
      more_reqs = trace.get_unfiltered_request(
          bubble_cnt, req_addr, req_type, payload);
      if (req_addr != -1) {
        req_addr = memory.page_allocator(req_addr, id, req_type);
      }
    } else {
      more_reqs = trace.get_unfiltered_request(
          bubble_cnt, req_addr, req_type, payload);
      if (req_addr != -1) {
        req_addr = memory.page_allocator(req_addr, id, req_type);
      }
    }
    if (!more_reqs) {
      if (!reached_limit) { // if the length of this trace is shorter than expected length, then record it when the whole trace finishes, and set reached_limit to true.
        // Hasan: overriding this behavior. We start the trace from the
        // beginning until the requested amount of instructions are
        // simulated. This should never be reached now.
        assert(false && "Shouldn't be reached since we start over the trace");
        record_cycs = clk;
        record_insts = long(cpu_inst.value());
        memory.record_core(id);
        reached_limit = true;
      }
    }
}

bool Core::finished()
{
    return !more_reqs && window.is_empty();
}

bool Core::has_reached_limit() {
  return reached_limit;
}

long Core::get_insts() {
    return long(cpu_inst.value());
}

void Core::receive(Request& req)
{
    window.set_ready(req.addr, ~(l1_blocksz - 1l));
    if (req.arrive != -1 && req.depart > last) {
      memory_access_cycles += (req.depart - max(last, req.arrive));
      last = req.depart;
    }
}

void Core::reset_stats() {
    clk = 0;
    retired = 0;
    cpu_inst = 0;
    reached_limit = false;
}

bool Window::is_full()
{
    return load == depth;
}

bool Window::is_empty()
{
    return load == 0;
}


void Window::insert(bool ready, long addr)
{
    assert(load <= depth);

    ready_list.at(head) = ready;
    addr_list.at(head) = addr;

    head = (head + 1) % depth;
    load++;
}


long Window::retire()
{
    assert(load <= depth);

    if (load == 0) return 0;

    int retired = 0;
    while (load > 0 && retired < ipc) {
        if (!ready_list.at(tail))
            break;

        tail = (tail + 1) % depth;
        load--;
        retired++;
    }

    return retired;
}


void Window::set_ready(long addr, int mask)
{
    if (load == 0) return;

    for (int i = 0; i < load; i++) {
        int index = (tail + i) % depth;
        if ((addr_list.at(index) & mask) != (addr & mask))
            continue;
        ready_list.at(index) = true;
    }
}



Trace::Trace(const char* trace_fname) : file(trace_fname), trace_name(trace_fname)
{
    if (!file.good()) {
        std::cerr << "Bad trace file: " << trace_fname << std::endl;
        exit(1);
    }
}

bool Trace::get_unfiltered_request(long& bubble_cnt, long& req_addr, Request::Type& req_type, string& payload)
{
    string line;
    getline(file, line);
    payload = "";
    // printf("Reading unfiltered line: %s --- ", line.c_str());
    if (file.eof()  || line.size() == 0) {
      file.clear();
      file.seekg(0, file.beg);
      getline(file, line); // Hasan: starting over the file
    }
    size_t pos, end;
    bubble_cnt = std::stoul(line, &pos, 10);
    pos = line.find_first_not_of(' ', pos+1);
    req_addr = std::stoul(line.substr(pos), &end, 0);

    pos = line.find_first_not_of(' ', pos+end);

    if (pos == string::npos || line.substr(pos)[0] == 'R')
        req_type = Request::Type::READ;
    else if (line.substr(pos)[0] == 'W')
        req_type = Request::Type::WRITE;
    else if (line.substr(pos)[0] == 'H')
        req_type = Request::Type::HAMMER;
    else assert(false);

    pos = line.find_first_not_of(' ', pos+1);
    if (pos !=  string::npos){
        payload = line.substr(pos);
    }

    // printf("Bubble: %lx, Addr: %lx, Type: %d, \n", bubble_cnt, req_addr, (int)req_type);
    return true;
}

bool Trace::get_filtered_request(long& bubble_cnt, long& req_addr, Request::Type& req_type)
{
    static bool has_write = false;
    static long write_addr;
    static int line_num = 0;
    if (has_write){
        bubble_cnt = 0;
        req_addr = write_addr;
        req_type = Request::Type::WRITE;
        has_write = false;
        return true;
    }
    string line;
    getline(file, line);
    line_num ++;
    if (file.eof() || line.size() == 0) {
        file.clear();
        file.seekg(0, file.beg);
        line_num = 0; //Hasan
        getline(file, line); // Hasan: starting over the file
        // printf("Reading filtered line: %s\n", line.c_str());
        //has_write = false;
        line_num++; //Hasan
        //return false;
    }

    size_t pos, end;
    bubble_cnt = std::stoul(line, &pos, 10);

    pos = line.find_first_not_of(' ', pos+1);
    req_addr = stoul(line.substr(pos), &end, 0);
    req_type = Request::Type::READ;

    pos = line.find_first_not_of(' ', pos+end);
    if (pos != string::npos){
        has_write = true;
        write_addr = stoul(line.substr(pos), NULL, 0);
    }
    return true;
}

bool Trace::get_dramtrace_request(long& req_addr, Request::Type& req_type)
{
    string line;
    getline(file, line);
    if (file.eof()) {
        return false;
    }
    size_t pos;
    req_addr = std::stoul(line, &pos, 16);

    pos = line.find_first_not_of(' ', pos+1);

    if (pos == string::npos || line.substr(pos)[0] == 'R')
        req_type = Request::Type::READ;
    else if (line.substr(pos)[0] == 'W')
        req_type = Request::Type::WRITE;
    else assert(false);
    return true;
}
