#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    
    //Ignore the illegal segment
    if((!is_syn_received && !header.syn) || 
    (is_syn_received && header.syn)) return;

    if(header.fin) is_fin_received = true;
    //Set the Initial Sequence Number if necessary.
    if(header.syn){
        is_syn_received = true;
        isn = header.seqno;
    }
    //Push any data, or end-of-stream marker, to the StreamReassembler.
    //Use the index of the last reassembled byte as the checkpoint.
    uint64_t checkpoint = stream_out().bytes_written();
    uint64_t abs_seqno = unwrap(header.seqno + header.syn, isn, checkpoint);
    uint64_t index = abs_seqno - 1;
    string data = seg.payload().copy();
    _reassembler.push_substring(data, index, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(!is_syn_received) return nullopt;

    uint64_t abs_ackno = stream_out().bytes_written() + 1;
    if(stream_out().input_ended()) return wrap(abs_ackno + 1, isn);
    return wrap(abs_ackno, isn);
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity();}
