#pragma once

#include "byte_stream.hh"

class Reassembler
{
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output ) 
    : output_( std::move( output ) ), expecting_syn(0), segments_()
  {
    //std::cout << "init Reassembler!\n";
  }

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in. (important!!)
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

private:

  struct Segment {
      uint64_t first_index;
      uint64_t len;
      bool is_last_substring;
      std::string buffer;

      Segment(uint64_t idx, bool flag, const std::string& buf)
          : first_index(idx), len(buf.size()), is_last_substring(flag), buffer(buf) {}

      bool operator<(const Segment& other) const {
        return first_index < other.first_index;
      }
  };

  uint64_t first_unaccept_idx() const;
  void check_close(bool is_last_substring);
  void check_buffer(uint64_t last_index);
  Segment merge_lastseg(Segment new_segment);
  Segment merge_nextseg(Segment new_segment);

  //debug
  void show_seg();

  ByteStream output_; // the Reassembler writes to this ByteStream
  uint64_t expecting_syn;
  std::multiset<Segment> segments_;
};
