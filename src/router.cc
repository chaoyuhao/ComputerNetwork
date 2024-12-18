#include "router.hh"

#include <iostream>
#include <limits>
#include <iomanip>
#include <queue>

using namespace std;

// // show a uint32_t in ipv4 format
// string ipv4_numeric_to_string(uint32_t ip) {
//   return to_string((ip >> 24) & 0xFF) + "." + to_string((ip >> 16) & 0xFF) + "." + to_string((ip >> 8) & 0xFF) + "." + to_string(ip & 0xFF);
// }

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  
  // Your code here.
  RouterInfo rt;
  rt.route_prefix = route_prefix;
  if (prefix_length == 0) rt.subnet_mask = 0;
  else if (prefix_length < 32) rt.subnet_mask = (((1U << prefix_length) - 1U) << (32U - prefix_length));
  else rt.subnet_mask = numeric_limits<uint32_t>::max();
  rt.destination = route_prefix & rt.subnet_mask;
  if(next_hop.has_value()) rt.next_hop = next_hop.value();
  else {
    rt.next_hop = Address::from_ipv4_numeric(0);
  }
  rt.interface_num = interface_num;
  _routing_table.push_back(rt);
  
  // DEBUG INFO
  static int count = 0;
  if(++count == 9) Router_show();
}

uint8_t Router::matched_bits(const uint32_t dst, const uint32_t src) {
  uint32_t matched = 0;
  while((dst >> (31 - matched)) == (src >> (31 - matched))) {
    matched++;
  }
  return static_cast<uint8_t>( matched );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  //cerr << "Router: routing datagrams\n";
  //int count=0;
  for (auto inter : _interfaces) {
    //cerr << "Router: interface idx->" << count++ << endl;
    queue<InternetDatagram> datagrams = inter->datagrams_received();

    
    while (!datagrams.empty()) {
      auto datagram = datagrams.front();
      inter->datagrams_received().pop();
      datagrams.pop();

      // Process the datagram
      if(datagram.header.ver != 4) {
        continue;
      }
      if(datagram.header.ttl == 0 || datagram.header.ttl == 1) {
        continue;
      }
      datagram.header.ttl--;
      datagram.header.compute_checksum();
      
      uint8_t matched_prefix = 0; 
      Address next_hop = Address::from_ipv4_numeric(0);
      size_t outgoing_interface = 0;

      // longest-prefix-match routing
      for (auto rt : _routing_table) {
        if (matched_bits(datagram.header.dst, rt.destination) >= max(matched_prefix, matched_bits(rt.subnet_mask, 0xFFFFFFFF))) {
          matched_prefix = matched_bits(datagram.header.dst, rt.destination);
          next_hop = rt.next_hop;
          outgoing_interface = rt.interface_num;
        }
      }

      //if (matched_prefix != 0)
      cout << "Router: Longest prefix matching result: nexthop->" << Address::from_ipv4_numeric( next_hop.ipv4_numeric() ).ip() << " on eth->" << outgoing_interface << endl;
      cout << "Router: datagram dst->" << datagram.header.dst << " which is " << Address::from_ipv4_numeric( datagram.header.dst ).ip() << " / " << matched_prefix << endl;
      if(next_hop.ipv4_numeric() == 0) {
        cerr << "Router: (directly) send datagram to " << Address::from_ipv4_numeric( datagram.header.dst ).ip() << " on eth" << outgoing_interface << endl;
        interface(outgoing_interface)->send_datagram( datagram , Address::from_ipv4_numeric( datagram.header.dst ) );
      }else{ 
        cerr << "Router: send datagram to " << Address::from_ipv4_numeric( next_hop.ipv4_numeric() ).ip() << " on eth" << outgoing_interface << endl;
        interface(outgoing_interface)->send_datagram( datagram , next_hop );
      }
    }
  }
}
