#ifndef __REQUEST_H
#define __REQUEST_H

#include <vector>
#include <string>
#include <functional>

using namespace std;

namespace ramulator
{

class Request
{
public:
    bool is_first_command;
    bool dropped;
    long addr;
    // long addr_row;
    vector<int> addr_vec;
    // specify which core this request sent from, for virtual address translation
    int coreid;
    int flat_bank_id = -1;
    bool marked; // a flag that is used by some schedulers, e.g., PARBS
                 // TODO: find a better name for this flag, or implement
                 // the functionality in a better way
    bool is_real_hammer; // to carry the info about the ground truth
    bool is_ever_blocked = false; // BlockHammer blocked this request at least once.
    long blocked_until = 0; // BlockHammer blocked thhis request until this clk cycle.
    std::string payload; // carries any additional info found in the rest of the line.

    bool is_resent = false;
    bool ready = false;
    long test_clk = 0;

    long deadline = 0;

    enum class Type
    {
        READ,
        WRITE,
        REFRESH,
        POWERDOWN,
        SELFREFRESH,
        HAMMER,
        ACT,
        EXTENSION,
        PREFETCH,
        MAX
    } type;

    long arrive = -1;
    long block = -1; //this is the time that the request is blocked by blockhammer for the first time.
    long unblock = -1; //this is the time that the request is blocked by blockhammer for the first time.
    long depart;
    function<void(Request&)> callback; // call back with more info
    function<void(Request&)> proc_callback; // FIXME: ugly workaround

    Request(long addr, Type type, int coreid = 0)
        : is_first_command(true), addr(addr), coreid(coreid), marked(false), is_real_hammer(false), type(type), 
      callback([](Request& req){}) {}

    Request(long addr, Type type, function<void(Request&)> callback, int coreid = 0)
        : is_first_command(true), addr(addr), coreid(coreid), marked(false), is_real_hammer(false), type(type), callback(callback) {}

    Request(vector<int>& addr_vec, Type type, function<void(Request&)> callback, int coreid = 0)
        : is_first_command(true), addr_vec(addr_vec), coreid(coreid), marked(false), is_real_hammer(false), type(type), callback(callback) {}

    Request(vector<int>& addr_vec, Type type)
        : is_first_command(true), addr_vec(addr_vec), is_real_hammer(false), type(type){}

    Request()
        : is_first_command(true), coreid(0), is_real_hammer(false), marked(false) {}
};

} /*namespace ramulator*/

#endif /*__REQUEST_H*/

