#include "tcp_connection.hh"

#include <iostream>


using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return  outbound_stream().remaining_capacity();}

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    _time_since_last_segment_received = 0;
    TCPHeader header = seg.header();

    //! if the rst flag is set, sets both the inbound and outbound streams to the error state 
    //! and kills the connection permanently
    if(header.rst){
        inbound_stream().set_error();
        outbound_stream().set_error();
        _is_rst_set = true;
        return;
    }

    //! gives the segment to the TCPReceiver
    _receiver.segment_received(seg);

    //! if the ack flag is set, tells the TCPSender about 
    //! the fields it cares about on incoming segments: ackno and window size.
    if(header.ack){
        _sender.ack_received(header.ackno, header.win);
    }

    //! if the incoming segment occupied any sequence numbers, 
    //！the TCPConnection makes sure that at least one segment is sent in reply.
    if(seg.length_in_sequence_space()){
        _sender.fill_window();
        if(_sender.segments_out().empty()){
            _sender.send_empty_segment();
        }
    }
        
    //! If the inbound stream ends before the TCPConnection has reached EOF on its outbound stream, this variable needs to be set to false.
    if(inbound_stream().input_ended() && !outbound_stream().eof()) _linger_after_streams_finish = false;

    //! responding to a “keep-alive” segment.
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0)
    && header.seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment(); 
    }

    //!  reflect an update in the ackno and window size.
    transform_segments_out();
}

bool TCPConnection::active() const { 
    if(_is_rst_set) return false;

    //Prereq #1 The inbound stream has been fully assembled and has ended
    if(inbound_stream().input_ended() && _receiver.unassembled_bytes() == 0 
    //Prereq #2 The outbound stream has been ended by the local application and fully sent
    && outbound_stream().eof() && _sender.next_seqno_absolute() == outbound_stream().bytes_written() + 2
    //Prereq #3 The outbound stream has been fully acknowledged by the remote peer
    && bytes_in_flight() == 0
    // At any point where prerequisites #1 through #3 are satisfied, the connection is “done” (and
    // active() should return false) if linger after streams finish is false.
    && (!_linger_after_streams_finish
    // Otherwise you need to linger: the connection is only done after enough time (10 × cfg.rt timeout) has
    // elapsed since the last segment was received.
    || time_since_last_segment_received() >= 10 * _cfg.rt_timeout)) return false;

    return true;
 }

size_t TCPConnection::write(const string &data) {
    size_t bytes_written = outbound_stream().write(data);
    _sender.fill_window();
    transform_segments_out();
    return bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    if(_receiver.syn_received()) 
        _sender.fill_window(), transform_segments_out();
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS){
        send_rst_seg();
        return;
    }
    transform_segments_out();
 }

void TCPConnection::end_input_stream() { 
    outbound_stream().end_input();
    _sender.fill_window();
    transform_segments_out();    
}

void TCPConnection::connect() {
    _sender.fill_window();
    transform_segments_out();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            send_rst_seg();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::transform_segments_out(){
    while(!_sender.segments_out().empty()){
    _sender.segments_out().front().header().ack = _receiver.ackno().has_value();
    if (_receiver.ackno().has_value())
        _sender.segments_out().front().header().ackno = _receiver.ackno().value();
    _sender.segments_out().front().header().win = min(_receiver.window_size(), static_cast<size_t>((1 << 16) - 1));
    _segments_out.push(_sender.segments_out().front());
    _sender.segments_out().pop();
}
}
void TCPConnection::send_rst_seg(){
        TCPSegment rst_tcp_seg;
        rst_tcp_seg.header().rst = true;
        rst_tcp_seg.header().seqno = _sender.next_seqno();
        outbound_stream().set_error();
        inbound_stream().set_error();
        _segments_out.push(rst_tcp_seg);
        _is_rst_set = true;
}