#pragma once

#include "reassembler.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <string>

class TCPReceiver
{
public:
  // Construct with given Reassembler
  explicit TCPReceiver( Reassembler&& reassembler ) : 
    reassembler_( std::move( reassembler ) ), ackno_(Wrap32(0)), zero_point(Wrap32(0)), last_seq_(0),
    window_size_(0), RST_(false), SYN_(false), FIN_(false)
  {}

  /*
   * The TCPReceiver receives TCPSenderMessages, inserting their payload into the Reassembler
   * at the correct stream index.
   */
  void receive( TCPSenderMessage message );

  // The TCPReceiver sends TCPReceiverMessages to the peer's TCPSender.
  TCPReceiverMessage send() const;

  // Access the output (only Reader is accessible non-const)
  const Reassembler& reassembler() const { return reassembler_; }
  Reader& reader() { return reassembler_.reader(); }
  const Reader& reader() const { return reassembler_.reader(); }
  const Writer& writer() const { return reassembler_.writer(); }

private:
  Reassembler reassembler_;
  Wrap32 ackno_;
  Wrap32 zero_point;
  uint64_t last_seq_;
  uint16_t window_size_;
  bool RST_;
  bool SYN_;
  bool FIN_;
};
