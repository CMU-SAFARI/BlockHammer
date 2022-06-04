#ifndef __H_CROW_TABLE_H__
#define __H_CROW_TABLE_H__

#include <unordered_map>
#include <list>
#include <cstdlib>
#include <ctime>
#include "Statistics.h"

using namespace std;

namespace ramulator {

    class CROWEntry {
        public:
            unsigned long row_addr;
            bool valid;
            bool FR; //Force Restoration
            int hit_count;
            long total_hits; // used only for statistic collection
            bool is_to_remap_weak_row;

            CROWEntry() {
                row_addr = 0;
                valid = false;
                FR = false;
                hit_count = 0;
                total_hits = 0;
                is_to_remap_weak_row = false;
            }
    };

    template <typename T>
    class CROWTable {


    protected:
        ScalarStat crow_evict_with_zero_hits;
        ScalarStat crow_evict_with_one_hit;
        ScalarStat crow_evict_with_two_hits;
        ScalarStat crow_evict_with_5_or_less_hits;
        ScalarStat crow_evict_with_6_or_more_hits;
         
    public:

        bool is_DDR4 = false, is_LPDDR4 = false;

        CROWTable(const T* spec, const int crow_id, const uint num_SAs, 
                    const uint num_copy_rows, const uint num_weak_rows, 
                    const uint crow_evict_hit_thresh,
                    const uint crow_half_life, const float to_mru_frac,
                    const uint num_grouped_SAs) : 
                        spec(spec), crow_id(crow_id), num_SAs(num_SAs), num_copy_rows(num_copy_rows),
                        num_weak_rows(num_weak_rows), to_mru_frac(to_mru_frac), num_grouped_SAs(num_grouped_SAs) {
                    // num_grouped_SAs indicates how many consecutively
                    // addressed subarrays (SAs) will share CROW table entries. This is
                    // implemented to optimize the storage requirement of
                    // the CROW table. num_grouped_SAs is 1 by default,
                    // which means each SA will have its own num_copy_rows number
                    // of entries.

            assert((num_weak_rows <= num_copy_rows) && "Cannot have more weak rows than copy rows! Need to fallback to the default refresh rate in case there are too many weak rows.");


            int num_levels = int(T::Level::Bank) - int(T::Level::Rank) + 1;

            sizes = new int[num_levels + 1]; // +1 for subarrays


            num_entries = num_SAs * num_copy_rows;
            for(int i = 0; i < num_levels; i++) {
                sizes[i] = spec->org_entry.count[1 + i]; // +1 to skip channel
                num_entries *= spec->org_entry.count[1 + i]; 
            }

            num_entries /= num_grouped_SAs;


            entries = new CROWEntry[num_entries];
            lru_lists = new list<CROWEntry*>[num_entries/num_copy_rows];
            pointer_maps = new unordered_map<CROWEntry*, 
                         list<CROWEntry*>::iterator>[num_entries/num_copy_rows];

            cur_accesses = new int[num_entries/num_copy_rows];
            fill(cur_accesses, cur_accesses + (num_entries/num_copy_rows), 0);
            hit_count_half_life = crow_half_life;

            srand(static_cast<unsigned>(0));

            next_copy_row_id = new uint[num_entries/num_copy_rows];
            for(int i = 0; i < (num_entries/num_copy_rows); i++) {
                next_copy_row_id[i] = 0;
            }

            is_DDR4 = spec->standard_name == "DDR4";
            is_LPDDR4 = spec->standard_name == "LPDDR4";

            if(is_DDR4 || is_LPDDR4)
                SA_size = spec->org_entry.count[int(T::Level::Row)]/num_SAs;
            else // for SALP
                SA_size = spec->org_entry.count[int(T::Level::Row) - 1];

            sizes[num_levels] = num_copy_rows;
            sizes[num_levels-1] = sizes[num_levels] * (num_SAs/num_grouped_SAs);
            for(int i = (num_levels - 2); i >= 0; i--) {
                sizes[i] = spec->org_entry.count[i + 2] * sizes[i + 1];
            }

            do_weak_row_mapping();

            //for(int i = 0; i <= num_levels; i++)
                //printf("sizes[%d] = %d \n", i, sizes[i]);

            crow_evict_with_zero_hits
                .name("crow_evict_with_zero_hits_"+to_string(crow_id))
                .desc("Number of copy rows evicted without being hit")
                .precision(0)
                ;

            crow_evict_with_one_hit
                .name("crow_evict_with_one_hit_"+to_string(crow_id))
                .desc("Number of copy rows evicted with a single hit")
                .precision(0)
                ;
            crow_evict_with_two_hits
                .name("crow_evict_with_two_hits_"+to_string(crow_id))
                .desc("Number of copy rows evicted with two hits")
                .precision(0)
                ;
            crow_evict_with_5_or_less_hits
                .name("crow_evict_with_5_or_less_hits_"+to_string(crow_id))
                .desc("Number of copy rows evicted with 5 hits or less")
                .precision(0)
                ;
            crow_evict_with_6_or_more_hits
                .name("crow_evict_with_6_or_more_hits_"+to_string(crow_id))
                .desc("Number of copy rows evicted with 6 or more hits")
                .precision(0)
                ;


        }

        virtual ~CROWTable() {
            delete[] entries;
            delete[] next_copy_row_id;
            delete[] sizes;

            delete[] lru_lists;
            delete[] pointer_maps;
            delete[] cur_accesses;
        }

        bool access(const vector<int>& addr_vec, const bool move_to_LRU = false) {

            int ind = (calc_entries_offset(addr_vec)/num_copy_rows);
            int& cur_access = cur_accesses[ind];
            cur_access++;

            if(cur_access == hit_count_half_life) {
                cur_access = 0;

                // traverse all corresponding entries and halve their hit counts
                auto& cur_lru_list = lru_lists[ind];

                for(auto it = cur_lru_list.begin(); it != cur_lru_list.end(); it++) {
                    (*it)->hit_count >>= 1;
                } 
            }

            if(is_hit(addr_vec)) {
                // promote to most recently accessed
                CROWEntry* cur_entry = get_hit_entry(addr_vec);

                cur_entry->total_hits++;
                if(cur_entry->hit_count < MAX_HIT_COUNT)
                    cur_entry->hit_count++;

                auto& cur_lru_list = lru_lists[ind];
                auto& cur_pointer_map = pointer_maps[ind];

                cur_lru_list.erase(cur_pointer_map[cur_entry]);
                if(!move_to_LRU) {
                    cur_lru_list.push_front(cur_entry);
                    cur_pointer_map[cur_entry] = cur_lru_list.begin();
                } else {
                    cur_lru_list.push_back(cur_entry);
                    cur_pointer_map[cur_entry] = prev(cur_lru_list.end());
                }

                return true;
            }
            else {
                assert(!move_to_LRU && "Error: Move to LRU should not be set on a miss!");
                // do not insert to lru_list on miss
                // insert only when add_entry() is called
                
                // decrease the hit counter of the LRU entry
                //CROWEntry* LRU_entry = get_LRU_entry(addr_vec);
                //if(LRU_entry->hit_count > 0)
                //    LRU_entry->hit_count--;
            }

            return false;

        }

        void make_LRU(const vector<int>& addr_vec, CROWEntry* crow_entry) {
            int ind = (calc_entries_offset(addr_vec)/num_copy_rows);
            auto& cur_lru_list = lru_lists[ind];
            auto& cur_pointer_map = pointer_maps[ind];

            cur_lru_list.erase(cur_pointer_map[crow_entry]);
            cur_lru_list.push_back(crow_entry);
            cur_pointer_map[crow_entry] = prev(cur_lru_list.end());
        }

        // this function does not update the LRU state
		bool is_hit(const vector<int>& addr_vec) {
            if(get_hit_entry(addr_vec) != nullptr){
                if(num_weak_rows == num_copy_rows)
                    assert(false && "WARNING: We should not get hit when all copy rows are allocated for weak rows.");
                return true;
            }

            return false;
        }

		CROWEntry* add_entry(const vector<int>& addr_vec, const bool FR) {
            int lru_ind = calc_entries_offset(addr_vec)/num_copy_rows;
            auto& cur_lru_list = lru_lists[lru_ind];
            auto& cur_pointer_map = pointer_maps[lru_ind];
            CROWEntry* cur_entry = nullptr;

        	int freeLoc = free_loc(addr_vec);
            if (freeLoc == -1) {
		        cur_entry = get_LRU_entry(addr_vec);
                                
                assert(!cur_entry->FR && "The discarded entry should not require full restoration!");

                if(cur_entry->total_hits == 0){
                    crow_evict_with_zero_hits++;
                    crow_evict_with_5_or_less_hits++;
                }
                else if(cur_entry->total_hits == 1){
                    crow_evict_with_one_hit++;
                    crow_evict_with_5_or_less_hits++;
                }
                else if(cur_entry->total_hits == 2){
                    crow_evict_with_two_hits++;
                    crow_evict_with_5_or_less_hits++;
                }
                else if(cur_entry->total_hits <= 5)
                    crow_evict_with_5_or_less_hits++;
                else
                    crow_evict_with_6_or_more_hits++;


            	cur_entry->row_addr = addr_vec[int(T::Level::Row)];
                cur_entry->valid = true;
                cur_entry->FR = FR;
                cur_entry->hit_count = 0;
                cur_entry->total_hits = 0;

                update_next_copy_row_id(addr_vec);
                
                // remove the LRU entry from the list
                assert(cur_lru_list.size() <= num_copy_rows);

                                
                cur_lru_list.pop_back();
                cur_pointer_map.erase(cur_entry);
            } 
            else {
        		cur_entry = get_entry(addr_vec, uint(freeLoc));
                cur_entry->row_addr = addr_vec[int(T::Level::Row)];
                cur_entry->valid = true;
                cur_entry->FR = FR;
                cur_entry->hit_count = 0;
                cur_entry->total_hits = 0;
                
                // As there is a free location, cur_lru_list should not be
                // full
                assert(cur_lru_list.size() < num_copy_rows && 
                        "There is a free copy row, the LRU table should not be full!"); 
            }

            float r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);

            if(r >= to_mru_frac){
                cur_lru_list.push_back(cur_entry);
                cur_pointer_map[cur_entry] = prev(cur_lru_list.end());
            } else {
                cur_lru_list.push_front(cur_entry);
                cur_pointer_map[cur_entry] = cur_lru_list.begin();
            }

            return cur_entry;
        }

        CROWEntry* get_entry(const vector<int>& addr_vec, const uint copy_row_id) {
            int offset = calc_entries_offset(addr_vec);    
        	return &entries[offset + copy_row_id];
        }

        CROWEntry* get_hit_entry(const vector<int>& addr_vec){
            int offset = calc_entries_offset(addr_vec);

            int row_addr = addr_vec[int(T::Level::Row)];
	        for (uint i = 0; i < num_copy_rows; i++) {
            	if (entries[offset + i].row_addr == row_addr && 
	        			entries[offset + i].valid && !entries[offset + i].is_to_remap_weak_row)
                	return &entries[offset + i];
            }

            return nullptr;
        }

		bool set_FR(const vector<int>& addr_vec, const bool FR) {
        	CROWEntry* cur_entry = get_hit_entry(addr_vec);
        
        	if(cur_entry == nullptr){
                assert(false && "We should always get a hit.");
        		return false;
            }
        
        	cur_entry->FR = FR;
        
        	return true;
        }

        CROWEntry* get_LRU_entry(const vector<int>& addr_vec, int crow_evict_threshold = 0){
            int lru_ind = calc_entries_offset(addr_vec)/num_copy_rows;
            auto& cur_lru_list = lru_lists[lru_ind];

            if(crow_evict_threshold == 0)
                return cur_lru_list.back();

            auto it = cur_lru_list.end();
            do {
                it--;
                if((*it)->hit_count <= crow_evict_threshold)
                    return *it;
            } while(it != cur_lru_list.begin());

            return nullptr;
        }

        CROWEntry* get_discarding_entry(const vector<int>& addr_vec){
            return get_entry(addr_vec, get_next_copy_row_id(addr_vec));
        }

		bool get_discarding_FR(const vector<int>& addr_vec) {
            return get_entry(addr_vec, get_next_copy_row_id(addr_vec))->FR;
        }

		unsigned long get_discarding_row_addr(const vector<int>& addr_vec) {
            return get_entry(addr_vec, get_next_copy_row_id(addr_vec))->row_addr;
        }

        int get_discarding_copy_row_id(const vector<int>& addr_vec) {
            return get_next_copy_row_id(addr_vec);
        }

		bool is_full(const vector<int>& addr_vec) {
           return (free_loc(addr_vec) == -1); 
        }

		void invalidate(const vector<int>& addr_vec) {
            CROWEntry* entry = get_hit_entry(addr_vec);
            assert(!entry->is_to_remap_weak_row && "A remapped weak row should not be invalidated!");
            int lru_ind = calc_entries_offset(addr_vec)/num_copy_rows;
            auto& cur_lru_list = lru_lists[lru_ind];
            auto& cur_pointer_map = pointer_maps[lru_ind];

            cur_lru_list.erase(cur_pointer_map[entry]);
            cur_pointer_map.erase(entry);

            entry->valid = false;

        }
        
        int find_not_FR(const vector<int>& addr_vec) {
            int offset = calc_entries_offset(addr_vec);

            for(int i = 0; i < num_copy_rows; i++) {
                if(!entries[offset + i].FR)
                    return i;
            }

            return -1;
        }
        
        void print() {
        
            for(int i = 0; i < num_entries; i++) {
                CROWEntry& ce = entries[i];
                printf("CROWTable: %lu %d %d \n", ce.row_addr, ce.valid, ce.FR);
            }
        }

    private:
        const T* spec;
        int crow_id;
        CROWEntry* entries;
        int num_entries = 0;
        int* sizes;
        uint* next_copy_row_id;
        uint num_SAs = 1;
        uint num_copy_rows;
        uint num_weak_rows;
		int SA_size = 0;

        float to_mru_frac = 0.0f; //by default, always insert a new entry to LRU position
        uint num_grouped_SAs = 1;

        int hit_count_half_life = 1000; // in number of CROWTable accesses
        const int MAX_HIT_COUNT = 32;
        int* cur_accesses = nullptr;

        // LRU replacement policy
        list<CROWEntry*>* lru_lists; //one for each subarray
        unordered_map<CROWEntry*, list<CROWEntry*>::iterator>* pointer_maps;


		void update_next_copy_row_id(const vector<int>& addr_vec) {
            uint cur = get_next_copy_row_id(addr_vec);

            int offset = calc_next_copy_row_id_offset(addr_vec);
        	next_copy_row_id[offset] = (cur + 1) % num_copy_rows;
        }

		uint get_next_copy_row_id(const vector<int>& addr_vec) {
            int offset = calc_next_copy_row_id_offset(addr_vec);
            return next_copy_row_id[offset]; 
        }

		int free_loc(const vector<int>& addr_vec) {
            for(uint i = 0; i < num_copy_rows; i++) {
        		if(!(get_entry(addr_vec, i)->valid))
	        		return i;
        	}

        	return -1;
        }

        int calc_entries_offset(const vector<int>& addr_vec) {
            int offset = 0;

            // skipping channel
            for(int l = 1; l <= int(T::Level::Bank); l++) {
                offset += sizes[l-1]*addr_vec[l];
            }

            int SA_id;
            if(is_DDR4 || is_LPDDR4)
                SA_id = addr_vec[int(T::Level::Row)]/SA_size;
            else // for SALP
                SA_id = addr_vec[int(T::Level::Row) - 1];
            offset += sizes[int(T::Level::Bank)]*(SA_id/num_grouped_SAs);

            return offset;       
        }


        int calc_next_copy_row_id_offset(const vector<int>& addr_vec) {
           int offset = 0;
           // skipping channel
           for(int l = 1; l <= int(T::Level::Bank); l++) {
               offset += (sizes[l-1]*addr_vec[l])/num_copy_rows;
           }
       
           int SA_id;
           if(is_DDR4 || is_LPDDR4)
               SA_id = addr_vec[int(T::Level::Row)]/SA_size;
           else // for SALP
               SA_id = addr_vec[int(T::Level::Row) - 1];
           offset += (SA_id/num_grouped_SAs);
       
           return offset;
        }

        void do_weak_row_mapping() {
            assert(is_LPDDR4 && "Error: This functionality is not"
                    "currently supported for the selected DRAM standard.");

            if(num_grouped_SAs > 1)
                assert(num_weak_rows == 0 && "Error: Weak row remapping is not yet implemented to work with SA grouping");

            vector<int> addr_vec(int(T::Level::MAX), 0);


            for(int r = 0; r < spec->org_entry.count[int(T::Level::Rank)]; r++){
                addr_vec[int(T::Level::Rank)] = r;

                for(int b = 0; b < spec->org_entry.count[int(T::Level::Bank)]; b++) {
                    addr_vec[int(T::Level::Bank)] = b;

                    for(int i = 0; i < num_SAs; i++) {
                        addr_vec[int(T::Level::Row)] = i*SA_size;
                        int offset = calc_entries_offset(addr_vec);
                        for(int j = 0; j < num_weak_rows; j++) {
                            CROWEntry* entry = entries + offset + j;
                            entry->is_to_remap_weak_row = true;
                            entry->valid = true;
                        }
                    }
                }
            }
        }
 
};


} // namespace ramulator

#endif //__H_CROW_TABLE_H__
