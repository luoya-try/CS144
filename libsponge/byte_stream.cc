#include "byte_stream.hh"
using namespace std;
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity) : myStream({}), mxSize(capacity), size(0), totWritten(0), totPop(0) {
}
size_t ByteStream::write(const string &data) {
    size_t len = data.length();
    size_t acceptedNum = min(len, mxSize - size);
    
    for(size_t i = 0; i < acceptedNum; i ++) {
        myStream.push_back(data[i]);
    }
    totWritten += acceptedNum;
    size += acceptedNum;
    return acceptedNum;
}

//! \param[in] len bytes will be copied from the output size of the buffer
string ByteStream::peek_output(const size_t len) const {
    return string(myStream.begin(), myStream.begin() + len);
}

//! \param[in] len bytes will be removed from the output size of the buffer
void ByteStream::pop_output(const size_t len) { 
    totPop += len;
    size -= len;
    for(size_t i = 0; i < len; i ++) myStream.pop_front();
 }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t ac_len = min(len, buffer_size());
    string str = peek_output(ac_len);
    pop_output(ac_len);
    return str;
}

void ByteStream::end_input() { _input_ended = true;}

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return size; }

bool ByteStream::buffer_empty() const { return size == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty();}

size_t ByteStream::bytes_written() const { return totWritten; }

size_t ByteStream::bytes_read() const { return totPop; }

size_t ByteStream::remaining_capacity() const { return mxSize - size;}
