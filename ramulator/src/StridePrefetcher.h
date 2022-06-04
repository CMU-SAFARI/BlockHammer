#ifndef __STRIDE_PREFETCHER
#define __STRIDE_PREFETCHER

// A Stride Prefetcher implementation based on "Effective Stream-Based and
// Execution-Based Data Prefetching", ICS'04


#define PREF_STRIDE_REGION_BITS 16 // TODO: make configurable??? 
#define STRIDE_REGION(x) ( x >> (PREF_STRIDE_REGION_BITS) )

#include <functional>
#include "Request.h"

namespace ramulator
{
    class StridePrefetcher {
        protected:
            struct StrideRegionTableEntry {
                long tag;
                bool valid;
                unsigned int last_access;
            };

            struct StrideIndexTableEntry {
                bool trained;
                bool train_count_mode;

                unsigned int num_states;
                unsigned int curr_state;
                long last_index;

                int stride[2];
                int s_cnt[2];
                int strans[2]; //== stride12, stride21

                int recnt;
                int count;

                int pref_count;
                unsigned int pref_curr_state;
                long pref_last_index;

                unsigned long pref_sent;
            };

            unsigned int num_entries;
            StrideRegionTableEntry* region_table;
            StrideIndexTableEntry* index_table;
            function<bool(Request)> send;
            function<void(Request&)> callback;
            function<void(Request&)> proc_callback;

        private:
            const int CACHE_LINE_SIZE_BITS = 6; // FIXME: cache line size is hardcoded in ramulator
                                                // to 64 bytes. Change this when making 
                                                // cache line size configurable

            bool issue_pref_req(long line_index);

        public:

            // config. params
            enum class StridePrefMode {
                SINGLE_STRIDE,
                MAX
            } mode;

            int single_stride_threshold;
            int multi_stride_threshold;
            int stride_start_dist;
            int stride_degree;
            int stride_dist;
            // END - config params

            StridePrefetcher(unsigned int num_stride_table_entries, StridePrefMode _mode, 
                    int _single_stride_threshold, int _multi_stride_threshold, 
                    int _stride_start_dist, int _stride_degree, int _stride_dist,
                    function<bool(Request)> _send, function<void(Request&)> _callback,
                    function<void(Request&)> _proc_callback);
            ~StridePrefetcher();

            void train(long line_addr, bool ul1_hit, long cur_clk);
            void miss(long line_addr, long cur_clk);
            void hit(long line_addr, long cur_clk);

            void create_new_entry(int idx, long line_addr, long region_tag, long cur_clk);

    };

} // namespace ramulator


#endif // __STRIDE_PREFETCHER
