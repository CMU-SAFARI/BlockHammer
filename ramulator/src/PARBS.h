#ifndef __PARBS_H
#define __PARBS_H

#include <vector>
#include <algorithm>
#include <list>
#include <functional>
#include "Request.h"
#include "Controller.h"
#include "SchedulerBase.h"

using namespace std;
namespace ramulator
{

template <typename T>
class Controller;

template <typename T>
class PARBS : public SchedulerBase<T>{
public:
    Controller<T>* ctrl;

    typedef list<Request>::iterator ReqIter;
    PARBS(Controller<T>* ctrl) : ctrl(ctrl) {
        numberCores = 1;
        _markedLoad = 0;
    }

    ReqIter better_req(ReqIter req1, ReqIter req2);
    void set_num_cores(int);
    void set_localBcount(unsigned bcount);
    void set_globalBcount(unsigned bcount);
    void Tick();
    void initialize();
    void dequeue_req(ReqIter req);
    ~PARBS(){}

    void form_batch();
    void assign_rank();
private:
    int batch_cap = 5;
    int prio_max = 11;

    unsigned LocalBcount;
    unsigned GlobalBcount;

    unsigned long prio_inv_thresh = 0;
    int use_weights = 0;
    double weights[128];

    int numberCores;
    vector<int> _rank;
    unsigned _markedLoad;
    vector<int> _markedMaxLoadPerProc;
    vector<int> _markedTotalLoadPerProc;
    vector<vector<list<Request> > > _markableQ;

    int indexof(vector<int> ptr, int tid, int size);
    static bool compare_nocase (Request req1, Request req2);
    bool sort_maxtot(int tid1, int tid2);
};


template <typename T>
void PARBS<T>::set_num_cores(int coreN){
    if(coreN > 0){
        numberCores = coreN;
        _rank.resize(numberCores);
        _markedMaxLoadPerProc.resize(numberCores);
        _markedTotalLoadPerProc.resize(numberCores);
    }
}

template <typename T>
void PARBS<T>::set_localBcount(unsigned bcount){
    LocalBcount = bcount;
}

template <typename T>
void PARBS<T>::set_globalBcount(unsigned bcount){
    GlobalBcount = bcount*LocalBcount;
}

template<typename T>
void PARBS<T>::initialize(){
   _markableQ.resize(numberCores);

    for(int i=0; i < numberCores; i++)
        _markableQ[i].resize(this->LocalBcount);

    for(int i=0; i < numberCores; i++)
        for(int j=0; j < this->LocalBcount; j++){
            list<Request> tmp(batch_cap);
            _markableQ[i][j] = tmp;
         }
}

template<typename T>
void PARBS<T>::dequeue_req(ReqIter req){
    if(!req->marked)
        return;

    if(_markedLoad > 0)
        _markedLoad--;

}

template <typename T>
typename PARBS<T>::ReqIter PARBS<T>::better_req(ReqIter req1, ReqIter req2){
    bool marked1 = req1->marked;
    bool marked2 = req2->marked;
    if (marked1 ^ marked2){
        if (marked1) return req1;
        else return req2;
    }


    bool ready1 = this->ctrl->is_ready(req1);
    bool ready2 = this->ctrl->is_ready(req2);

    if (ready1 ^ ready2) {
        if (ready1) return req1;
        return req2;
    }

    int rank1 = 0, rank2 = 0; // Hasan: these two were uninitialized, which led to warnings. Is zero-initialization is correct?
    if(req1->coreid < _rank.size())
        rank1 = _rank[req1->coreid];
     if(req2->coreid < _rank.size())
        rank2 = _rank[req2->coreid];

    if (rank1 != rank2) {
        if (rank1 > rank2) return req1;
        else return req2;
    }

    if (req1->arrive <= req2->arrive) return req1;
    return req2;
}

template <typename T>
void PARBS<T>::Tick(){
    if((_markedLoad > 0) || (ctrl->readq.size() < 3)){
        return;
    }
    form_batch();
    assign_rank();
}

template <typename T>
void PARBS<T>::form_batch(){
    for(int i=0; i < numberCores; i++){
        _markedMaxLoadPerProc[i] = 0;
        _markedTotalLoadPerProc[i] = 0;
        for(int j=0; j < LocalBcount; j++){
            _markableQ[i][j].clear();
        }
    }

    unsigned bcount = 0;
    for (list<Request>::iterator it=ctrl->readq.q.begin(); it != ctrl->readq.q.end(); ++it){
        if(!(it->marked)){
            int p = it->coreid;
            if((p < numberCores) && (bcount < LocalBcount))
                _markableQ[p][bcount].push_back(*it);
            else
                cerr << "Invalid operation \n";
        }
        bcount++;
    }

    for(int i=0; i < numberCores; i++){
        for(int j=0; j < LocalBcount; j++){
            _markableQ[i][j].sort(compare_nocase);
        }
    }

    for(int i=0; i < numberCores; i++){
        for(int j=0; j < LocalBcount; j++){
            list<Request> q = _markableQ[i][j];
            unsigned markedCnt = 0;
            for (list<Request>::iterator it=q.begin(); it != q.end(); ++it){
                if(markedCnt == batch_cap)
                    break;
                it->marked = true;
                markedCnt++;
            }
            _markedLoad += markedCnt;
            _markedMaxLoadPerProc[i] += markedCnt;
            if(markedCnt > _markedMaxLoadPerProc[i])
                _markedMaxLoadPerProc[i] = markedCnt;
        }
    }
}


template <typename T>
int PARBS<T>::indexof(vector<int> ptr, int tid, int size){
    for (int i = 0; i < size; i++){
        if(ptr[i] == tid)
            return i;
    }
    return -1;
}

template <typename T>
void PARBS<T>::assign_rank(){
   vector<int> tids(numberCores);
   for(int i=0; i < numberCores; i++) tids[i] = i;

   if(numberCores > 1){
       for(int i=1; i < numberCores;++i){
           for(int j=0; j <(numberCores - i); ++j){
            if(sort_maxtot(tids[j],tids[j+1])){
                int temp=tids[j];
                tids[j]=tids[j+1];
                tids[j+1]=temp;
             }
           }
        }
   }

   for(int i=0; i < numberCores; i++)
       _rank[i] = indexof(tids, i, numberCores);

}

template<typename T>
bool PARBS<T>::compare_nocase(Request req1, Request req2){
    if (req1.arrive <= req2.arrive) return true;
    return false;
}

template<typename T>
bool PARBS<T>::sort_maxtot(int tid1, int tid2){
    unsigned max1 = 0, max2 = 0, tot1 = 0, tot2 = 0; // Hasan: these were uninitialized. Check if is correct.
    if((tid1 < numberCores) && (tid2 < numberCores)){
         max1 = _markedMaxLoadPerProc[tid1];
         max2 = _markedMaxLoadPerProc[tid2];
         tot1 = _markedTotalLoadPerProc[tid1];
         tot2 = _markedTotalLoadPerProc[tid2];
    }
    if (max1 != max2) {
        if (max1 < max2) return true;
        else return false;
    }

    if (tot1 != tot2){
        if (tot1 < tot2) return true;
        else return false;
    }
    return false;
}

}/*namespace ramulator*/
#endif // PARBS_H
