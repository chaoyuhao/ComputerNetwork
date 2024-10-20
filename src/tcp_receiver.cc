#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  //cout << "TCP Receive" << "->" << message.payload << " rst->" << message.RST << " syn->" << message.SYN << endl;
  if(message.SYN) { // start message
    ackno_ = message.seqno + 1;
    SYN_   = message.SYN;
    FIN_   = false;
    last_seq_ = 0;
    zero_point = message.seqno;
    //cout << "init TCP ";
    ackno_.print();
  }
  if(message.RST) {
    RST_ = true;
    reassembler_.reader().set_error();
    //cout << "HAS ERROR RST" << endl;
    //return;
  }
  if(!SYN_) return;
  if(message.FIN) {
    FIN_ = true;
  }
  uint64_t idx = message.seqno.unwrap(zero_point, last_seq_);
  uint64_t s1 = reassembler_.reassembler_SYN();
  if(!message.SYN) idx--;
  //cout << "rea insert idx->" << idx << " payload->" << message.payload << " FIN->" << message.FIN << endl;
  reassembler_.insert(idx, message.payload, message.FIN);
  uint64_t s2 = reassembler_.reassembler_SYN();
  //cout << "check syn " << s1 << " ==> " << s2 << endl;
  ackno_ = ackno_ + uint16_t(s2-s1);
  last_seq_ = idx;
  if(reassembler_.bytes_pending() == 0 && FIN_) {
    ackno_ = ackno_ + 1;
  }
  // cout << "ackno_ ";
  // ackno_.print();
}

TCPReceiverMessage TCPReceiver::send() const
{
  //cout << "read TCP receiver" << endl;
  TCPReceiverMessage ret {};
  if (SYN_) ret.ackno = ackno_;
  ret.window_size = (uint16_t)min( reassembler_.writer().available_capacity(), (uint64_t)UINT16_MAX);
  if (reassembler_.reader().has_error() || RST_) ret.RST = true;
  return ret;
}
