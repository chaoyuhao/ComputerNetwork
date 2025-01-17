#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) 
    : b_stream_(capacity, '\0'), capacity_(capacity), b_l_(0), b_r_(0), push_counter_(0), pop_counter_(0), close_flag_(false), error_(false) 
  {}

bool Writer::is_closed() const
{
  return close_flag_;
}

void Writer::push( string data )
{
  //cout << "bytestream 0\n";
  //if(close_flag_) return;
  int len = data.length();
  //cout << "bytestream 1\n";
  if(push_counter_ - pop_counter_ + len > capacity_) 
    len = capacity_ - (push_counter_ - pop_counter_);
  //cout << "bytestream 2 " << len <<"\n";
  for(int i=0; i<len; i++) {
    //cout << "byte stream push " << data[i] << '\n';
    b_stream_[b_r_] = data[i];
    b_r_ = (b_r_ + 1) % capacity_;
    push_counter_++;
  }
  //(void)data;
  return;
}

void Writer::close()
{
  close_flag_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - (push_counter_ - pop_counter_);
}

uint64_t Writer::bytes_pushed() const
{
  return push_counter_;
}

bool Reader::is_finished() const
{
  return (push_counter_ - pop_counter_ == 0) && close_flag_;
}

uint64_t Reader::bytes_popped() const
{
  return pop_counter_;
}

string_view Reader::peek() const
{
  size_t view_len = min(capacity_ - b_l_, push_counter_ - pop_counter_);
  string_view ret(&b_stream_[b_l_], view_len);
  return ret;
  // uint64_t buffered = push_counter_ - pop_counter_;
  // uint64_t half_len = capacity_ - b_l_;
  // if(buffered > half_len) {
  //   buffered = buffered - (half_len - 1);
  // }
  // string_view ret(&b_stream_[b_l_], capacity_ - b_l_);
}

void Reader::pop( uint64_t len )
{
  // assert(len < capacity_);
  if(len > push_counter_ - pop_counter_) len = push_counter_ - pop_counter_;
  b_l_ = (b_l_ + len) % capacity_;
  pop_counter_ += len;
}

uint64_t Reader::bytes_buffered() const
{
  return push_counter_ - pop_counter_;
}
