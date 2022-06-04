#ifndef __BLISS_H
#define __BLISS_H

#include <vector>
#include <list>
#include "Request.h"
#include "Controller.h"
#include "SchedulerBase.h"

using namespace std;
namespace ramulator
{

template <typename T>
class Controller;

template <typename T>
class BLISS : public SchedulerBase<T>{
public:
    Controller<T>* ctrl;

    typedef list<Request>::iterator ReqIter;
    BLISS(Controller<T>* ctrl) : ctrl(ctrl) {
        bliss_shuffle_cycles = 10000;
        bliss_row_hit_cap = 4;
        frfcfs_qos_threshold = 2000;
        numberCores = 1;
    }

    ReqIter better_req(ReqIter req1, ReqIter req2);
    void set_num_cores(int);
    void Tick();
    void clear_marking();
    void issue_req(Request *req);

private:
    int bliss_shuffle_cycles;
    int bliss_row_hit_cap;
    int frfcfs_qos_threshold;

    int _shuffleCyclesLeft;
    int _lastReqPid;
    int _oldestStreakGlobal;

    int numberCores;
    vector<int> _mark;
};


template <typename T>
void BLISS<T>::set_num_cores(int coreN){
    if(coreN > 0){
        numberCores = coreN;
        _mark.resize(numberCores);
    }
}

template <typename T>
typename BLISS<T>::ReqIter BLISS<T>::better_req(ReqIter req1, ReqIter req2){
    if((_mark[req1->coreid]!= 1) ^ (_mark[req2->coreid] !=1)){
        if(_mark[req1->coreid] != 1)
            return req1;
        else
            return req2; 
    }

    bool ready1 = this->ctrl->is_ready(req1);
    bool ready2 = this->ctrl->is_ready(req2);

    if (ready1 ^ ready2) {
        if (ready1) return req1;
        return req2;
    }

    if (req1->arrive <= req2->arrive) return req1;
    return req2;
}

template <typename T>
void BLISS<T>::Tick(){
    if (_shuffleCyclesLeft > 0){
        _shuffleCyclesLeft--;
    }
    else{
        _shuffleCyclesLeft = bliss_shuffle_cycles;
        clear_marking();
    }
}

template <typename T>
void BLISS<T>::clear_marking(){
    for(int i=0; i < numberCores; i++)
        _mark[i] = 0;
}

template <typename T>
void BLISS<T>::issue_req(Request *req){
    if (req == NULL) return;

    if (req->coreid == _lastReqPid && _oldestStreakGlobal < bliss_row_hit_cap){
        _oldestStreakGlobal += 1;
    }
    else if (req->coreid == _lastReqPid && _oldestStreakGlobal == bliss_row_hit_cap){
        _mark[req->coreid] = 1;
        _oldestStreakGlobal = 1;
    }
    else{
        _oldestStreakGlobal = 1;
    }
    _lastReqPid = req->coreid;
}

}/*namespace ramulator*/
#endif // BLISS_H
