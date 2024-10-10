#include "reassembler.hh"

using namespace std;

void Reassembler::check_close(bool is_last_substring) {
  if(is_last_substring) 
    output_.writer().close();
  return;
}

void Reassembler::show_seg() {
  for(const auto& it: segments_) {
    cout << it.first_index << ' ' << it.len << " [" << it.buffer << "]\n";
  }
}

// if this segment is connect with the first data in the buffer, then release the data
void Reassembler::check_buffer(uint64_t last_index) {
  if(segments_.empty()) return;
  auto it = segments_.begin();
  while(it != segments_.end() && it->first_index + it->len <= expecting_syn) {
    it = segments_.erase(it);
  }
  if(it == segments_.end()) {
    return;
  }
  if(last_index < it->first_index){
    return;
  }
  uint64_t len = it->len - (last_index - it->first_index);
  //cout << "buffer push " << len << ' ' << it->buffer.substr(last_index - it->first_index, len) << '\n';
  output_.writer().push(it->buffer.substr(last_index - it->first_index, len));
  expecting_syn += len;
  check_close(it->is_last_substring);
  segments_.erase(it);
}

Reassembler::Segment Reassembler::merge_lastseg(Segment new_segment) {
  auto it = segments_.lower_bound(new_segment);
  if(it == segments_.begin()) return new_segment; // 没有比它小的
  auto prev = std::prev(it);

  uint64_t last_idx = new_segment.first_index + new_segment.len;
  uint64_t prev_last_idx = prev->first_index + prev->len;
  //cout << "debug:" << new_segment.buffer << ' ' << prev_last_idx << ' ' << new_segment.first_index << ' ' << new_segment.len << ' ' << first_unaccept_idx() <<  '\n';
  if(prev_last_idx < last_idx && prev_last_idx >= new_segment.first_index){
    uint64_t offset = prev_last_idx - new_segment.first_index;
    new_segment.buffer = prev->buffer + new_segment.buffer.substr(offset);
    new_segment.first_index = prev->first_index;
    new_segment.is_last_substring = prev->is_last_substring || new_segment.is_last_substring;
    new_segment.len = new_segment.buffer.length();
    segments_.erase(prev);
    return new_segment;
  }

  // totally overlapped
  if (prev_last_idx >= last_idx) {
    new_segment = *prev;
    segments_.erase(prev);
  }
  // prev_last_idx < new_segment.first_index
  return new_segment;
}

Reassembler::Segment Reassembler::merge_nextseg(Segment new_segment) {

  uint64_t last_idx = new_segment.first_index + new_segment.len;
  
  auto it = segments_.lower_bound(new_segment);
  // 移除完全重合的部分
  while(it != segments_.end() && it->first_index + it->len <= last_idx) {
    new_segment.is_last_substring = new_segment.is_last_substring || it->is_last_substring;
    it = segments_.erase(it);
    ////cout << "merge next erase" << it->first_index << " " << it->buffer << '\n';
  }
  // 合并最后一块
  if(it != segments_.end() && it->first_index <= last_idx) {
    uint64_t offset = last_idx - it->first_index;
    new_segment.buffer = new_segment.buffer + it->buffer.substr(offset);
    new_segment.len = new_segment.buffer.length();
    new_segment.is_last_substring = new_segment.is_last_substring || it->is_last_substring;
    segments_.erase(it);
  }

  return new_segment;
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // Your code here.
  //cout << "expecting_syn " << expecting_syn << '\n';
  if(first_index == expecting_syn) { // push1
    uint64_t write_len = min(output_.writer().available_capacity(), data.length());
    uint64_t last_index = expecting_syn + write_len;
    //cout << "push1 " << first_index << ' ' << data << " len->" << write_len <<" args->" << data.substr(0, write_len) <<'\n';
    output_.writer().push(data.substr(0, write_len));
    expecting_syn = last_index;
    check_buffer(last_index);
    check_close(is_last_substring);

  }else if(first_index >= first_unaccept_idx()) { // push2
    //cout << "push2 " << first_index << ' ' <<data << '\n';

    check_close(is_last_substring);

  }else if(first_index < expecting_syn){ // push3
    //cout << "push3 " <<first_index << ' ' << data << '\n';

    if(first_index + data.length() < expecting_syn) {
      check_close(is_last_substring);
      return;
    }

    uint64_t write_len = min(data.length() - (expecting_syn - first_index), output_.writer().available_capacity());
    uint64_t last_index = expecting_syn + write_len;
    //cout << "push3 real " << expecting_syn << ' ' << data.substr(expecting_syn - first_index, write_len) << '\n';
    output_.writer().push(data.substr(expecting_syn - first_index, write_len));
    expecting_syn = last_index;
    check_buffer(last_index);
    check_close(is_last_substring);

  }else { // first_index > expecting_syn && first_index < expecting_syn + capacity_
  
    //cout << "push4 " << first_index << ' ' << data << '\n';
    // push the dataflow into the buffer of Reassembler
    uint64_t len = first_unaccept_idx() - first_index;
    if(len < data.length()) is_last_substring = false;
    Segment new_segment(first_index, is_last_substring, data.substr(0, len));
    segments_.insert(merge_nextseg(merge_lastseg(new_segment)));
  }
  return;
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t cnt = 0;
  for(const auto& it : segments_) {
    ////cout << "segment:" << it.first_index << " [" <<  it.buffer << "] \n";
    cnt += it.len;
  }
  return cnt;
}

uint64_t Reassembler::first_unaccept_idx() const {
  return expecting_syn + output_.writer().available_capacity();
}