#include "wrapping_integers.hh"
#include <iostream>

using namespace std;

#define MASK64 (0xFFFFFFFF00000000)
#define MASK1  (1UL<<32)

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  uint64_t zr = zero_point.raw_value_;
  uint64_t ret = (n+zr) % MASK1;
  return Wrap32((uint32_t)ret);
}

static inline uint64_t closer(uint64_t a, uint64_t b, uint64_t x) {
  uint64_t dis1 = x > a ? x-a : a-x, dis2 = x > b ? x-b : b-x;
  return dis1 > dis2 ? b : a;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // checkpoint mask
  uint64_t rv  = (uint64_t)raw_value_, zr = zero_point.raw_value_;
  uint64_t ab  = rv >= zr ? rv - zr : rv + MASK1 - zr;
  uint64_t cpm = (checkpoint & MASK64);
  if(cpm == 0) {
    return closer(cpm+ab, cpm+MASK1+ab, checkpoint);
  }else {
    uint64_t _ = closer(cpm+ab, cpm-MASK1+ab, checkpoint);
    return closer(_, cpm+MASK1+ab, checkpoint);
  }
  return 0;
}

void Wrap32::print() const
{
  cout << "DEBUG " << " wrap32->" << raw_value_ << endl;
}