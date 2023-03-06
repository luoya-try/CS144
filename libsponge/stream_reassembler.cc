#include "stream_reassembler.hh"
#include <map>
using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

void StreamReassembler::set_eof_index(size_t index, const bool eof) { if(eof) eof_index = index, eof_flag = true;}

void StreamReassembler::set_eof(size_t index) { if(eof_flag && index == eof_index) _output.end_input();}

void StreamReassembler::output_merge(){
    vector<size_t> delete_indexs;
    for(auto it: _unassembled_bytes){
        if(it.first <= expected_index){
            size_t len = it.second.length();
            if(it.first + len >= expected_index + 1)
            {
                _output.write(it.second.substr(expected_index - it.first));
                set_eof(it.first + len - 1);
                expected_index += len + it.first - expected_index;
            }
            delete_indexs.push_back(it.first);
        } else break;
    }
    for(auto it: delete_indexs) _unassembled_bytes.erase(it);
}

void StreamReassembler::unassemble_bytes_update(){
    map<size_t, string> mp;
    size_of_unassembled_bytes = 0;
    size_t end_index{}, start_index{}; 
    for(auto it: _unassembled_bytes){
        size_t len = it.second.length();
        if(it.first > end_index || start_index == 0){
            start_index = it.first;
            end_index = it.first + len - 1;
            size_of_unassembled_bytes += len;
            mp.insert(move(it));
        } else {
            if(it.first + len - 1 <= end_index) continue;
            mp[start_index] = mp[start_index] + it.second.substr(end_index - it.first + 1);
            size_of_unassembled_bytes += (len + it.first - end_index - 1);
            end_index = start_index + mp[start_index].length() - 1;
        }
    }
    _unassembled_bytes = move(mp);
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t len = data.length(), data_end_index = index + len - 1;
    max_end_index = _capacity + expected_index - _output.buffer_size() - 1;
    string effective_data = data;

    //! special judge1: empty data
    if(len == 0){ set_eof_index(index, eof); set_eof(index); return; }
    //! special judge2: data is out of bound or contained by _output
    if(index > max_end_index || data_end_index < expected_index) return;

    //! special judge3: portion of data out of bound
    if(data_end_index > max_end_index) {
        effective_data = data.substr(0, len - data_end_index + max_end_index);
        data_end_index = max_end_index;
    }else set_eof_index(index + data.length() - 1, eof);
    _unassembled_bytes[index] = effective_data;

    output_merge();
    unassemble_bytes_update();

}

size_t StreamReassembler::unassembled_bytes() const { return size_of_unassembled_bytes; }
bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
