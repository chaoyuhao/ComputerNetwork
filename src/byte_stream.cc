#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) 
    : b_stream_(capacity, '\0'), capacity_(capacity), b_l_(0), b_r_(0), push_counter_(0), pop_counter_(0) 
  {}

bool Writer::is_closed() const
{
  return close_flag_;
}

void Writer::push( string data )
{
  if(close_flag_) return;
  int len = data.length();
  if(push_counter_ - pop_counter_ + len > capacity_) 
    len = capacity_ - (push_counter_ - pop_counter_);
  for(int i=0; i<len; i++) {
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
  string_view ret(&b_stream_[b_l_], 1);
  return ret;
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
