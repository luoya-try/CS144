#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>
#include <iostream>

//! \brief  An alarm that can be started at a certain
//! time, and goes off (or “expires”) once the RTO has elapsed.
class  RetransmissionTimer{
  private:
    unsigned int _retransmission_timeout{};
    unsigned int _initial_retransmission_timeout{};
    unsigned int _RTO{};
    bool _on{};
    bool _expired{};
  public:
    RetransmissionTimer(const unsigned int initial_retransmission_timeout)
    :_initial_retransmission_timeout(initial_retransmission_timeout){set_RTO_initial();}

    void set_RTO_initial() {_RTO = _initial_retransmission_timeout;}
    void half_RTO() {_RTO /= 2;}
    void double_RTO() {_RTO *= 2;}
    bool is_on() {return _on;}
    bool is_expired() {return _expired;}
    void start(){_retransmission_timeout = _RTO; _on = true; _expired = false;}
    void stop() {_on = false;}
    void passtime(const unsigned int _passtime){
      if(_retransmission_timeout > _passtime) _retransmission_timeout -= _passtime;
      else _expired = true;
    }
};
//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! queue of outstanding tcp segments
    std::queue<TCPSegment> _outstanding_segments{};
    //! the window size of the receiver, that the length of the TCP segment cannot exceed
    uint16_t _window_size{1};
    //! the max abs_seqno the receiver can receive
    uint64_t _upper_bound{};
    //! count how many times the _consecutive_retransmissions happened.
    unsigned int _consecutive_retransmissions{};
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;
    //! number of bytes that are sent but not acknowleged
    size_t _bytes_in_flight{};
    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};
    //! timer
    RetransmissionTimer timer;

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief everytime sending a segment, the timer should be started
    void segment_sending(TCPSegment& tcp_seg){ 
      _segments_out.push(tcp_seg); 
      if(!timer.is_on()) timer.start();
    }

    //! \brief everytime pushing or poping an outstanding segment, the number of bytes in flight should be updated
    void outstanding_push(TCPSegment& tcp_seg){ _outstanding_segments.push(tcp_seg); _bytes_in_flight += tcp_seg.length_in_sequence_space();}
    void outstanding_pop() { if(!_outstanding_segments.empty()){ _bytes_in_flight -= _outstanding_segments.front().length_in_sequence_space(); _outstanding_segments.pop();}}
    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
