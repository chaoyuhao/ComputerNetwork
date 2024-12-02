#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retransmissions_;
}

uint64_t TCPSender::byteInBuffer(std::queue<TCPSenderMessage> q) {
  uint64_t ret = 0;
  while (!q.empty())
  {
    ret += q.front().sequence_length();
    q.pop();
  }
  return ret;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  //cout << "tcp push" << endl;
  if(!initial_push) {
    TCPSenderMessage msg = make_empty_message();
    msg.SYN = true;
    initial_push = true;
    in_flight_ = 1;
    isn_ = isn_ + 1;
    window_size_ -= 1;
    checkpoint_ = isn_.unwrap(init_isn_, 0);
    ////cout << "checkpoint_:" << checkpoint_ << endl;
    ack_bound_ = msg.seqno.unwrap(init_isn_, checkpoint_) + 1;
    if(reader().is_finished()) {
      msg.FIN = true;
      in_flight_++;
      isn_ = isn_ + 1;
    }
    buffer_msg_.push(msg);
    transmit(msg);
    return;
  }

  if(reader().is_finished()) { // 关了最多发个FIN
    //cout << "bytestream finished" << endl;
    // //cout << ack_bound_ << " : " << isn_.unwrap(init_isn_, checkpoint_) << " in buffer -> " << buffer_msg_.size() << " cp-> " << checkpoint_ << endl;
    // if(buffer_msg_.size() == 1) {
    //   //cout << "only string in buffer -> " << buffer_msg_.front().payload << endl;
    // }
    if(window_size_ > byteInBuffer(buffer_msg_) && last_FIN_ == false) {
      TCPSenderMessage msg = make_empty_message();
      msg.FIN = true;
      in_flight_++;
      buffer_msg_.push(msg);
      isn_ = isn_ + msg.sequence_length();
      window_size_ -= msg.sequence_length();
      ack_bound_ += 1;
      transmit(msg);
      last_FIN_ = true;
    }
    return;
  }
  if(reader().bytes_buffered() == 0) { // 暂时没有
    // 啥也没有就不要发了
    //cout << "empty bytestream no push" << endl;
    return;
  }
  
  if(window_size_ == 0 || TCPConfig::MAX_PAYLOAD_SIZE == 0) { // 试探0窗口
    return;
  }

  uint64_t winsz = min(window_size_, TCPConfig::MAX_PAYLOAD_SIZE);

  string new_seg;
  new_seg.clear();
  auto view = reader().peek();
  size_t view_len = min(view.size(), winsz);
  //cout << "view size " << view.size() << endl;

  if(window_size_ > TCPConfig::MAX_PAYLOAD_SIZE && view.size() > TCPConfig::MAX_PAYLOAD_SIZE) {
    //cout << "many transmit in one push" << endl;

    while (view.size() && window_size_) {
      winsz = min(window_size_, TCPConfig::MAX_PAYLOAD_SIZE);
      view_len = min(view.size(), winsz);
      new_seg.clear();
      new_seg = view.substr(0, view_len);
      input_.reader().pop(view_len);
      TCPSenderMessage msg = make_empty_message();
      msg.payload.clear();
      msg.payload = new_seg;
      in_flight_ += new_seg.size();

      if(reader().is_finished()) {
        //cout << "bytestream finished in loop" << endl;
        if(winsz > new_seg.size() || new_seg.size() == TCPConfig::MAX_PAYLOAD_SIZE){
          msg.FIN = true;
          in_flight_++;
          ack_bound_++;
          last_FIN_ = true;
        }
      }

      buffer_msg_.push(msg);
      transmit(msg);
      isn_ = isn_ + msg.sequence_length();
      window_size_ -= msg.sequence_length();
      ack_bound_ = max(ack_bound_, isn_.unwrap(init_isn_, checkpoint_) + msg.sequence_length());
      
      view = reader().peek();
    }

    return;
  }

  new_seg = view.substr(0, view_len);
  input_.reader().pop(view_len);

  TCPSenderMessage msg = make_empty_message();
  msg.payload.clear();
  msg.payload = new_seg;
  in_flight_ += new_seg.size();

  ////cout << "push size " << new_seg << "->" << new_seg.size() << " windows->" << winsz << endl;
  if(reader().is_finished()) {
    //cout << "bytestream finished" << endl;
    if(winsz > new_seg.size() || new_seg.size() == TCPConfig::MAX_PAYLOAD_SIZE){ //MAX_PAYLOAD_SIZE limits payload only
      msg.FIN = true;
      in_flight_++;
      ack_bound_++;
      last_FIN_ = true;
    }
  }

  buffer_msg_.push(msg);
  ////cout << '\n' << msg.payload << endl;
  ////cout << msg.sequence_length() << endl;
  transmit(msg);
  isn_ = isn_ + msg.sequence_length();
  window_size_ -= msg.sequence_length();
  ack_bound_ = max(ack_bound_, isn_.unwrap(init_isn_, checkpoint_) + msg.sequence_length());
  return;
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  //cout << "new empty msg" << endl;
  // Your code here.
  TCPSenderMessage msg;
  msg.FIN = false;
  msg.payload = "";
  msg.RST = reader().has_error();
  msg.seqno = isn_;
  msg.SYN = false;
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if(msg.RST) {
    writer().set_error();
    return;
  }
  //cout << "tcp receive" << endl;
  if(msg.ackno.has_value() && !buffer_msg_.empty()) {
    uint64_t msgseq = msg.ackno.value().unwrap(init_isn_, checkpoint_);
    uint64_t lastseq = buffer_msg_.front().seqno.unwrap(init_isn_, checkpoint_);
    uint64_t len = buffer_msg_.front().sequence_length();
    //cout << "bound " << msgseq << ' ' << ack_bound_ << endl;
    if(msgseq > ack_bound_) return;
    ////cout << "DEBUG:  tcp receiver ackno->" << msgseq << " lastseq->" << lastseq << " len->" << len << endl;
    while(msgseq >= lastseq + len) {
      //cout << "receiver pop ->" << lastseq << " len->" << len << endl;
      RTO_ms_ = initial_RTO_ms_;
      checkpoint_ = lastseq;
      in_flight_ -= len;
      // window_size_ += len;
      buffer_msg_.pop();
      if(!buffer_msg_.empty()) {
        lastseq = buffer_msg_.front().seqno.unwrap(init_isn_, checkpoint_);
        len = buffer_msg_.front().sequence_length();
      } else {
        break;
      }
      RTO_alart_ms_ = live_time_;
    }

    window_size_ = msg.window_size;
    new_RTO_alart_ = false;
    retransmissions_ = 0;
    alart_pause_ = false;

    if(msg.window_size == 0){
      window_size_ = 1;
      zero_ws_ = true;
    }else zero_ws_ = false;
  }

}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  live_time_ += ms_since_last_tick;
  if(!new_RTO_alart_) {
    new_RTO_alart_ = true;
  }
  //cout << "tick: " << live_time_ << ' ' << RTO_alart_ms_ << ' ' << RTO_ms_ << " retx->" << retransmissions_ << endl;
  
  if(live_time_ >= RTO_alart_ms_ + RTO_ms_) {
    // resent the first message in the buffer
    if(buffer_msg_.empty()) return;
    transmit(buffer_msg_.front());
    retransmissions_++;
    if (alart_pause_ || (retransmissions_ > TCPConfig::MAX_RETX_ATTEMPTS)) {
      alart_pause_ = true;
      return;
    }
    RTO_alart_ms_ = live_time_;
    if(!zero_ws_) RTO_ms_ *= 2;
  }
}
