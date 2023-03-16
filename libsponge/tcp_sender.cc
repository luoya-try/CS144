#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>


using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {

    // ! state: "FIN_SENT" --> stream finished (FIN sent) but not fully acknowledged
    // ! state: "FIN_ACKED" --> stream finished and fully acknowledged
    if(stream_in().eof() && next_seqno_absolute() == stream_in().bytes_written() + 2){ return;}
    //! state: "SYN_SENT" --> stream started but nothing acknowledged(noting to do but wait for SYN_ACKED)
    if(next_seqno_absolute() > 0 && next_seqno_absolute() == bytes_in_flight()){return;}


    //! state: CLOSED --> waiting for stream to begin (no SYN sent)
    if(next_seqno_absolute() == 0){
        TCPSegment tcp_seg;
        tcp_seg.header().syn = true;
        tcp_seg.header().seqno = _isn;
        _next_seqno ++;
        segment_sending(tcp_seg);  
        outstanding_push(tcp_seg);
    }

    //! state: "SYN_ACKED" --> stream ongoing
    else if(next_seqno_absolute() > bytes_in_flight()){
        while(_upper_bound > next_seqno_absolute()){
            TCPSegment tcp_seg;
            tcp_seg.header().seqno = next_seqno();
            tcp_seg.payload() = Buffer(stream_in().read(min(_upper_bound - next_seqno_absolute(), TCPConfig::MAX_PAYLOAD_SIZE)));
            _next_seqno += tcp_seg.length_in_sequence_space();
            if(stream_in().eof() && _upper_bound > next_seqno_absolute()){
                tcp_seg.header().fin = true;
                _next_seqno ++;
            }
            if(tcp_seg.length_in_sequence_space() != 0) {
                segment_sending(tcp_seg);
                outstanding_push(tcp_seg);
            }
            
            if(stream_in().buffer_size() == 0) break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    // check whether the receiver gives the sender a new ackno
    bool is_ackno_effective{}; 
    uint64_t abs_ackno = unwrap(ackno, _isn, next_seqno_absolute());
    if(abs_ackno > next_seqno_absolute()) return;
    
    _window_size = window_size;

    // remove the acknowledged segments from the _outgoing segment
    _upper_bound = (window_size == 0) ? abs_ackno + 1 : abs_ackno + window_size;
    while(!_outstanding_segments.empty()){
        uint64_t abs_seqno = unwrap(_outstanding_segments.front().header().seqno, _isn, next_seqno_absolute());
        TCPSegment seg = _outstanding_segments.front();
        if(abs_seqno + _outstanding_segments.front().length_in_sequence_space() - 1 < abs_ackno){
            is_ackno_effective = true;
            outstanding_pop();
        } else break;
    }

    if(is_ackno_effective){
        // (a) Set the RTO back to its “initial value.”
        timer.set_RTO_initial();
        // (b) If any outstanding data, restart the retransmission timer
        // cout << "1 timer start\n";
        timer.start();
        // (c) Reset the count of “consecutive retransmissions” back to zero.
        _consecutive_retransmissions = 0;
    }
    //When all outstanding data has been acknowledged, stop the retransmission timer.
    if(_outstanding_segments.empty()) {
        // cout << "timer stop\n";
        timer.stop();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if(timer.is_on()){
        timer.passtime(ms_since_last_tick);
        // cout << "ms since last tick: " << ms_since_last_tick << endl;
        //if the retransmission timer has expired
        if(timer.is_expired()){
            // cout << "is expired!\n";
            //(a) Retransmit the earliest outgoing segment
            if(!_outstanding_segments.empty()){
                segment_sending(_outstanding_segments.front());
            }
            // (b) If the window size is nonzero: increment the number of consecutive retransmissions and exponential backoff
            if(_window_size != 0){
                _consecutive_retransmissions ++;
                // cout << "double the rto\n";
                timer.double_RTO();
            }
            // (c) Reset the retransmission timer
            // cout << "2 timer start\n";
            timer.start();
        }
    }
 }

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    // cout << "send empty" << endl;
    TCPSegment tcp_segment;
    tcp_segment.header().seqno = next_seqno();
    _segments_out.push(tcp_segment);
}
