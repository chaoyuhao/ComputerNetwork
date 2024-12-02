#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), init_isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), 
      last_FIN_ (false) , RTO_ms_( initial_RTO_ms ),
      initial_push ( false ),  new_RTO_alart_( false ),  alart_pause_( false ), RTO_alart_ms_( 0 ), 
      live_time_( 0 ), retransmissions_( 0 ), in_flight_( 0 ), checkpoint_( 0 ),
      window_size_( 1 ), buffer_msg_(), ack_bound_(0), zero_ws_( false )
  {
    //std::cout << "\n init tcp sender" << std::endl;
  }

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  Wrap32 init_isn_;
  uint64_t initial_RTO_ms_;

  // Variables updated by other functions
  bool last_FIN_;
  uint64_t RTO_ms_;
  bool initial_push;
  bool new_RTO_alart_;
  bool alart_pause_;
  uint64_t RTO_alart_ms_;
  uint64_t live_time_;
  uint64_t retransmissions_;
  uint64_t in_flight_;
  uint64_t checkpoint_;
  uint64_t window_size_;
  std::queue<TCPSenderMessage> buffer_msg_;
  uint64_t ack_bound_;
  bool zero_ws_;

  static uint64_t byteInBuffer (std::queue<TCPSenderMessage> q);
};
