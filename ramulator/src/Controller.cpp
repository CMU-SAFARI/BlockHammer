#include "Controller.h"
#include "SALP.h"
#include "ALDRAM.h"
//#include "TLDRAM.h"

//using namespace ramulator;

namespace ramulator
{

static vector<int> get_offending_subarray(DRAM<SALP>* channel, vector<int> & addr_vec){
    int sa_id = 0;
    auto rank = channel->children[addr_vec[int(SALP::Level::Rank)]];
    auto bank = rank->children[addr_vec[int(SALP::Level::Bank)]];
    auto sa = bank->children[addr_vec[int(SALP::Level::SubArray)]];
    for (auto sa_other : bank->children)
        if (sa != sa_other && sa_other->state == SALP::State::Opened){
            sa_id = sa_other->id;
            break;
        }
    vector<int> offending = addr_vec;
    offending[int(SALP::Level::SubArray)] = sa_id;
    offending[int(SALP::Level::Row)] = -1;
    return offending;
}


template <>
vector<int> Controller<SALP>::get_addr_vec(SALP::Command cmd, list<Request>::iterator req){
    if (cmd == SALP::Command::PRE_OTHER)
        return get_offending_subarray(channel, req->addr_vec);
    else
        return req->addr_vec;
}


template <>
bool Controller<SALP>::is_ready(list<Request>::iterator req){
    SALP::Command cmd = get_first_cmd(req);
    if (cmd == SALP::Command::PRE_OTHER){

        vector<int> addr_vec = get_offending_subarray(channel, req->addr_vec);
        return channel->check(cmd, addr_vec.data(), clk);
    }
    else return channel->check(cmd, req->addr_vec.data(), clk);
}

template <>
void Controller<SALP>::initialize_crow_timing(vector<typename SALP::TimingEntry> timing[]
                [int(SALP::Command::MAX)], const float trcd_factor, const float tras_factor,
                        const float twr_factor, const float tfaw_factor) {

    assert(channel->spec->type == SALP::Type::MASA && "I didn't check timing for non-MASA implementations.");

		if(timing == partial_crow_hit_partial_restore_timing)
            printf("Initializing Partial CROW Hit to Partial Restoration Timing... \n");
        else if(timing == partial_crow_hit_full_restore_timing)
            printf("Initializing Partial CROW Hit to Full Restoration Timing... \n");
        else if(timing == full_crow_hit_partial_restore_timing)
            printf("Initializing Full CROW Hit to Partial Restoration Timing... \n");
        else if(timing == full_crow_hit_full_restore_timing)
            printf("Initializing Full CROW Hit to Full Restoration Timing... \n");
        else if(timing == crow_copy_timing)
            printf("Initializing CROW Copy Timing... \n");
        else
            assert(false && "Initializing unknown CROW timing.");


        // copy the default timing parameters
        for(uint l = 0; l < int(SALP::Level::MAX); l++) {
            for(uint c = 0; c < int(SALP::Command::MAX); c++) {
                timing[l][c] = channel->spec->timing[l][c];
            }
        }

        vector<typename SALP::TimingEntry>* t;
        int trcd = 0, tras = 0;

        // apply trcd_factor to the related timing params
        t = timing[int(SALP::Level::SubArray)];

        for (auto& t : t[int(SALP::Command::ACT)]) {
            if((t.cmd == SALP::Command::RD) || (t.cmd == SALP::Command::RDA)){
                printf("Default ACT-to-RD cycles: %d\n", t.val);
                t.val = (int)ceil(t.val * trcd_factor);
                trcd = t.val;
                printf("New ACT-to-RD cycles: %d\n", t.val);
            }

            if((t.cmd == SALP::Command::WR) || (t.cmd == SALP::Command::WRA)) {
                printf("Default ACT-to-WR cycles: %d\n", t.val);
                t.val = (int)ceil(t.val * trcd_factor);
                printf("New ACT-to-WR cycles: %d\n", t.val);
            }
        }



        // apply tras_factor to the related timing parameters
        t = timing[int(SALP::Level::Rank)];

        for (auto& t : t[int(SALP::Command::ACT)]) {
            if(t.cmd == SALP::Command::PREA){
               printf("Default ACT-to-PREA cycles: %d\n", t.val);
               t.val = (int)ceil(t.val * tras_factor);
               tras = t.val;
               printf("New ACT-to-PREA cycles: %d\n", t.val);
            }
        }

        t = timing[int(SALP::Level::SubArray)];

        for (auto& t : t[int(SALP::Command::ACT)]) {
            if(t.cmd == SALP::Command::PRE) {
                printf("Default ACT-to-PRE cycles: %d\n", t.val);
                t.val = (int)ceil(t.val * tras_factor);
                printf("New ACT-to-PRE cycles: %d\n", t.val);
            }
        }

        // apply both trcd_factor and tras_factor to tRC
        assert(trcd != 0 && tras !=0 && "tRCD or tRAS was not set.");
        t = timing[int(SALP::Level::SubArray)];

        for (auto& t : t[int(SALP::Command::ACT)]) {
            if(t.cmd == SALP::Command::ACT) {
                printf("Default ACT-to-ACT cycles: %d\n", t.val);
                t.val = trcd + tras;
                printf("New ACT-to-ACT cycles: %d\n", t.val);
            }
        }

        // apply twr_factor to the related timing parameters
        t = timing[int(SALP::Level::Rank)];

        for (auto& t : t[int(SALP::Command::WR)]) {
            if(t.cmd == SALP::Command::PREA) {
                printf("Default WR-to-PREA cycles: %d\n", t.val);
                t.val = channel->spec->speed_entry.nCWL + channel->spec->speed_entry.nBL +
                            (int)ceil(channel->spec->speed_entry.nWR*twr_factor);
                printf("New WR-to-PREA cycles: %d\n", t.val);
            }
        }


        t = timing[int(SALP::Level::SubArray)];

        int new_nWA = channel->spec->speed_entry.nCWL + channel->spec->speed_entry.nBL +
                                (int)ceil((channel->spec->speed_entry.nWR*twr_factor)/2);

        for (auto& t : t[int(SALP::Command::WR)]) {
            if(t.cmd == SALP::Command::PRE) {
                printf("Default WR-to-PRE cycles: %d\n", t.val);
                t.val = channel->spec->speed_entry.nCWL + channel->spec->speed_entry.nBL +
                            (int)ceil(channel->spec->speed_entry.nWR*twr_factor);
                printf("New WR-to-PRE cycles: %d\n", t.val);
            }

            if(((t.cmd == SALP::Command::ACT) || (t.cmd == SALP::Command::SASEL) || (t.cmd == SALP::Command::WR)) && (t.sibling)) {
                t.val = new_nWA;
            }
        }

        for (auto& t : t[int(SALP::Command::WRA)]) {
            if(t.cmd == SALP::Command::ACT) {
                printf("Default WRA-to-ACT cycles: %d\n", t.val);
                t.val = channel->spec->speed_entry.nCWL + channel->spec->speed_entry.nBL +
                            (int)ceil(channel->spec->speed_entry.nWR*twr_factor) +
                            channel->spec->speed_entry.nRP;
                printf("New WRA-to-ACT cycles: %d\n", t.val);
            }

            if(((t.cmd == SALP::Command::ACT) || (t.cmd == SALP::Command::SASEL) || (t.cmd == SALP::Command::WRA)) && (t.sibling)) {
                t.val = new_nWA;
            }

        }

        // apply tfaw_factor to the related timing parameters
        t = timing[int(SALP::Level::Rank)];

        for (auto& t : t[int(SALP::Command::ACT)]) {
            if(t.cmd == SALP::Command::ACT && (t.dist == 4)) {
                printf("Default ACT-to-ACT (tFAW) cycles: %d\n", t.val);
                t.val = (int)ceil(t.val*tfaw_factor);
                printf("New ACT-to-ACT (tFAW) cycles: %d\n", t.val);
            }

        }
}

template <>
void Controller<ALDRAM>::update_temp(ALDRAM::Temp current_temperature){
    channel->spec->aldram_timing(current_temperature);
}


//template <>
//void Controller<TLDRAM>::tick(){
//    clk++;
//    req_queue_length_sum += readq.size() + writeq.size();
//    read_req_queue_length_sum += readq.size();
//    write_req_queue_length_sum += writeq.size();
//
//    /*** 1. Serve completed reads ***/
//    if (pending.size()) {
//        Request& req = pending[0];
//        if (req.depart <= clk) {
//          if (req.depart - req.arrive > 1) {
//                  read_latency_sum += req.depart - req.arrive;
//                  channel->update_serving_requests(
//                      req.addr_vec.data(), -1, clk);
//          }
//            req.callback(req);
//            pending.pop_front();
//        }
//    }
//
//    /*** 2. Should we schedule refreshes? ***/
//    refresh->tick_ref();
//
//    /*** 3. Should we schedule writes? ***/
//    if (!write_mode) {
//        // yes -- write queue is almost full or read queue is empty
//        if (writeq.size() >= int(0.8 * writeq.max) || readq.size() == 0)
//            write_mode = true;
//    }
//    else {
//        // no -- write queue is almost empty and read queue is not empty
//        if (writeq.size() <= int(0.2 * writeq.max) && readq.size() != 0)
//            write_mode = false;
//    }
//
//    /*** 4. Find the best command to schedule, if any ***/
//    Queue* queue = !write_mode ? &readq : &writeq;
//    if (otherq.size())
//        queue = &otherq;  // "other" requests are rare, so we give them precedence over reads/writes
//
//    auto req = scheduler->get_head(queue->q);
//    if (req == queue->q.end() || !is_ready(req)) {
//        // we couldn't find a command to schedule -- let's try to be speculative
//        auto cmd = TLDRAM::Command::PRE;
//        vector<int> victim = rowpolicy->get_victim(cmd);
//        if (!victim.empty()){
//            issue_cmd(cmd, victim);
//        }
//        return;  // nothing more to be done this cycle
//    }
//
//    if (req->is_first_command) {
//        int coreid = req->coreid;
//        req->is_first_command = false;
//        if (req->type == Request::Type::READ || req->type == Request::Type::WRITE) {
//          channel->update_serving_requests(req->addr_vec.data(), 1, clk);
//        }
//        int tx = (channel->spec->prefetch_size * channel->spec->channel_width / 8);
//        if (req->type == Request::Type::READ) {
//            if (is_row_hit(req)) {
//                ++read_row_hits[coreid];
//                ++row_hits;
//            } else if (is_row_open(req)) {
//                ++read_row_conflicts[coreid];
//                ++row_conflicts;
//            } else {
//                ++read_row_misses[coreid];
//                ++row_misses;
//            }
//          read_transaction_bytes += tx;
//        } else if (req->type == Request::Type::WRITE) {
//          if (is_row_hit(req)) {
//              ++write_row_hits[coreid];
//              ++row_hits;
//          } else if (is_row_open(req)) {
//              ++write_row_conflicts[coreid];
//              ++row_conflicts;
//          } else {
//              ++write_row_misses[coreid];
//              ++row_misses;
//          }
//          write_transaction_bytes += tx;
//        }
//    }
//
//    /*** 5. Change a read request to a migration request ***/
//    if (req->type == Request::Type::READ) {
//        req->type = Request::Type::EXTENSION;
//    }
//
//    // issue command on behalf of request
//    auto cmd = get_first_cmd(req);
//    issue_cmd(cmd, get_addr_vec(cmd, req));
//
//    // check whether this is the last command (which finishes the request)
//    if (cmd != channel->spec->translate[int(req->type)])
//        return;
//
//    // set a future completion time for read requests
//    if (req->type == Request::Type::READ || req->type == Request::Type::EXTENSION) {
//        req->depart = clk + channel->spec->read_latency;
//        pending.push_back(*req);
//    }
//    if (req->type == Request::Type::WRITE) {
//        channel->update_serving_requests(req->addr_vec.data(), -1, clk);
//    }
//
//    // remove request from queue
//    queue->q.erase(req);
//}

} /* namespace ramulator */
