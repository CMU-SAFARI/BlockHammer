#ifndef SCHEDULERBASE_H
#define SCHEDULERBASE_H

#include "Request.h"
#include <list>
namespace ramulator{

template <typename T>
class SchedulerBase{
public:
    typedef std::list<Request>::iterator ReqIter;
    virtual ReqIter better_req(ReqIter req1, ReqIter req2) = 0;
    virtual void set_num_cores(int) = 0;
    virtual void Tick() = 0;

    virtual void issue_req(Request *req){}
    virtual void set_localBcount(unsigned bcount){}
    virtual void set_globalBcount(unsigned bcount){}
    virtual void set_Bmax(unsigned b_max){}
    virtual void set_Rmax(unsigned r_max){}
    virtual void initialize(){}
    virtual void dequeue_req(ReqIter req){}
    virtual void increment_rd_req_count(int core){}
    virtual void increment_wr_req_count(int core){}
    virtual void increment_instruction_count(int core){}


};
}

#endif // SCHEDULERBASE_H
