#include "byte_stream.hh"
using namespace std;
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity) : mxSize(capacity){
}
size_t ByteStream::write(const string &data) {
    size_t len = data.length();
    size_t acceptedNum = min(len, mxSize - size);
    myStream.append(Buffer(data.substr(0, acceptedNum)));
    size += acceptedNum;
    totWritten += acceptedNum;
    return acceptedNum;
}

//! \param[in] len bytes will be copied from the output size of the buffer
string ByteStream::peek_output(const size_t len) const {
    return myStream.peak_out(min(myStream.size(), len));
}

//! \param[in] len bytes will be removed from the output size of the buffer
void ByteStream::pop_output(const size_t len) { 
    size_t acceptedNum = min(len, size);
    totPop += acceptedNum;
    size -= acceptedNum;
    myStream.remove_prefix(len);
 }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t acceptedNum = min(len, size);
    string str = myStream.peak_out(len);
    myStream.remove_prefix(len);
    size -= acceptedNum;
    return move(str);
}

void ByteStream::end_input() { _input_ended = true;}

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return size; }

bool ByteStream::buffer_empty() const { return size == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty();}

size_t ByteStream::bytes_written() const { return totWritten; }

size_t ByteStream::bytes_read() const { return totPop; }

size_t ByteStream::remaining_capacity() const { return mxSize - size;}
