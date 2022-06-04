#include "StridePrefetcher.h"

namespace ramulator
{


    StridePrefetcher::StridePrefetcher(unsigned int num_stride_table_entries, StridePrefMode _mode, 
            int _single_stride_threshold, int _multi_stride_threshold, int _stride_start_dist,
            int _stride_degree, int _stride_dist, function<bool(Request)> _send,
            function<void(Request&)> _callback, function<void(Request&)> _proc_callback) {
        mode = _mode;
        single_stride_threshold = _single_stride_threshold;
        multi_stride_threshold = _multi_stride_threshold;
        stride_start_dist = _stride_start_dist;
        stride_degree = _stride_degree;
        stride_dist = _stride_dist;
        send = _send;
        callback = _callback;
        proc_callback = _proc_callback;

        num_entries = num_stride_table_entries;
        region_table = new StrideRegionTableEntry[num_entries];
        index_table = new StrideIndexTableEntry[num_entries];
    }

    StridePrefetcher::~StridePrefetcher() {
        delete[] region_table;
        delete[] index_table;
    }

    void StridePrefetcher::train(long line_addr, bool ul1_hit, long cur_clk) {
       
       int region_idx = -1;
       long line_index = line_addr >> CACHE_LINE_SIZE_BITS;
       long index_tag = STRIDE_REGION(line_addr);

       for (int ii = 0; ii < num_entries; ii++) {
           if (index_tag == region_table[ii].tag && region_table[ii].valid) {
               region_idx = ii;
               break;
           }
       }

       if (region_idx == -1) {
           if (ul1_hit) // do not create an entry on hit
               return;
           // not present in the region table
           // make a new region
           // search for an unused entry
           for (int ii = 0; ii < num_entries; ii++) {
               if (!region_table[ii].valid) {
                   region_idx = ii;
                   break;
               }
               if (region_idx == -1 || (region_table[region_idx].last_access < 
                                                region_table[ii].last_access)) {
                   region_idx = ii;
               }
           }
           create_new_entry(region_idx, line_addr, index_tag, cur_clk);
           return;
       }

       StrideIndexTableEntry* entry = &index_table[region_idx];
       int stride = line_index - index_table[region_idx].last_index;
       index_table[region_idx].last_index = line_index;
       region_table[region_idx].last_access = cur_clk;

       if (!entry->trained) {
           // train the entry

           if (!entry->train_count_mode) {
               if (entry->stride[entry->curr_state] == 0) {
                   entry->stride[entry->curr_state] = stride;
                   entry->s_cnt[entry->curr_state] = 1;
               } else if (entry->stride[entry->curr_state] == stride) {
                   entry->s_cnt[entry->curr_state]++;
               } else {
                   if (mode == StridePrefMode::SINGLE_STRIDE) {
                       entry->stride[entry->curr_state] = stride;
                       entry->s_cnt[entry->curr_state] = 1;
                   } else {
                       // in multi-stride mode
                       entry->strans[entry->curr_state] = stride;
                       if (entry->num_states == 1) {
                           entry->num_states = 2;
                       }
                       entry->curr_state = (1 - entry->curr_state); // change 0 to 1 or 1 to 0
                       if (entry->curr_state == 0) {
                           entry->train_count_mode = true; // move into a checking mode
                           entry->count = 0;
                           entry->recnt = 0;
                       }
                   }
               }
           } else {
               // in train_count_mode
               if ((stride == entry->stride[entry->curr_state]) && 
                       (entry->count < entry->s_cnt[entry->curr_state])) {
                   entry->recnt++;
                   entry->count++;
               } else if ((stride == entry->strans[entry->curr_state]) && 
                       (entry->count == entry->s_cnt[entry->curr_state])) {
                   entry->recnt++;
                   entry->count = 0;
                   entry->curr_state = (1 - entry->curr_state);
               } else {
                   // does not match... lets reset.
                   create_new_entry(region_idx, line_addr, index_tag, cur_clk);
               }
           }
           if (entry->s_cnt[entry->curr_state] >= single_stride_threshold) {
               // single stride stream
               entry->trained = true;
               entry->num_states = 1;
               entry->curr_state = 0;
               entry->pref_last_index = entry->last_index + (entry->stride[0] * stride_start_dist);
           }
           if (entry->recnt >= multi_stride_threshold) {
               long pref_index;
               entry->trained = true;
               entry->pref_count = entry->count;
               entry->pref_curr_state = entry->curr_state;
               entry->pref_last_index = entry->last_index;
               for (int ii = 0; ii < stride_start_dist; ii++) {
                   if (entry->pref_count == entry->s_cnt[entry->pref_curr_state]) {
                       pref_index = entry->pref_last_index + entry->strans[entry->pref_curr_state];
                       entry->pref_count = 0;
                       entry->pref_curr_state = (1 - entry->pref_curr_state);
                   } else {
                       pref_index = entry->pref_last_index + entry->stride[entry->pref_curr_state];
                       entry->pref_count++;
                   }
                   entry->pref_last_index = pref_index;
               }
           }
       } else {
           long pref_index;
           // entry is trained
           if (entry->pref_sent)
               entry->pref_sent--;
           if (entry->num_states == 1 && stride == entry->stride[0]) {
               // single stride case
               for (int ii = 0; (ii < stride_degree && entry->pref_sent < stride_dist); ii++,
                                                    entry->pref_sent++) {
                   pref_index = entry->pref_last_index + entry->stride[0];
                   if (!issue_pref_req(pref_index))
                       break; // q is full
                   entry->pref_last_index = pref_index;
               }
           } else if ((stride == entry->stride[entry->curr_state] && 
                       entry->count < entry->s_cnt[entry->curr_state]) || 
                   (stride == entry->strans[entry->curr_state] && 
                    entry->count == entry->s_cnt[entry->curr_state])) {
               // first update verification info
               if (entry->count == entry->s_cnt[entry->curr_state]) {
                   entry->count = 0;
                   entry->curr_state = (1 - entry->curr_state);
               } else {
                   entry->count++;
               }
               // now send out prefetches
               for (int ii = 0; (ii < stride_degree && entry->pref_sent < stride_dist); 
                                                                    ii++, entry->pref_sent++) {
                   if (entry->pref_count == entry->s_cnt[entry->pref_curr_state]) {
                       pref_index = entry->pref_last_index + entry->strans[entry->pref_curr_state];
                       if (!issue_pref_req(pref_index))
                           break; // q is full
                       entry->pref_count = 0;
                       entry->pref_curr_state = (1 - entry->pref_curr_state);
                   } else {
                       pref_index = entry->pref_last_index + entry->stride[entry->pref_curr_state];
                       if (!issue_pref_req(pref_index))
                           break; // q is full
                       entry->pref_count++;
                   }
                   entry->pref_last_index = pref_index;
               }
           } else {
               // does not match
               entry->trained = false;
               entry->train_count_mode = false;
               entry->num_states = 1;
               entry->curr_state = 0;
               entry->stride[0] = 0;
               entry->stride[1] = 0;
               entry->s_cnt[0] = 0;
               entry->s_cnt[1] = 0;
               entry->strans[0] = 0;
               entry->strans[1] = 0;

               entry->count = 0;
               entry->recnt = 0;
               entry->pref_sent = 0;
           }
       }
    }

    void StridePrefetcher::miss(long line_addr, long cur_clk) {
        train(line_addr, false, cur_clk);
    }

    void StridePrefetcher::hit(long line_addr, long cur_clk) {
        train(line_addr, true, cur_clk);
    }

    void StridePrefetcher::create_new_entry(int idx, long line_addr, long region_tag, long cur_clk) {
        region_table[idx].tag = region_tag;
        region_table[idx].valid = true;
        region_table[idx].last_access = cur_clk;

        index_table[idx].trained = false;
        index_table[idx].num_states = 1;
        index_table[idx].curr_state = 0;
        index_table[idx].last_index = line_addr >> CACHE_LINE_SIZE_BITS;
        index_table[idx].stride[0] = 0;
        index_table[idx].s_cnt[0] = 0;
        index_table[idx].stride[1] = 0;
        index_table[idx].s_cnt[1] = 0;

        index_table[idx].strans[0] = 0;
        index_table[idx].strans[1] = 0;

        index_table[idx].recnt = 0;
        index_table[idx].count = 0;
        index_table[idx].train_count_mode = false;
        index_table[idx].pref_sent = 0;
    }

    bool StridePrefetcher::issue_pref_req(long line_index) {
        long line_addr = line_index << CACHE_LINE_SIZE_BITS;

        Request req(line_addr, Request::Type::PREFETCH, callback, 0);
        req.proc_callback = proc_callback;
        return send(req);
    }

} // namespace ramulator
