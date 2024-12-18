#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <set>

#include "exception.hh"
#include "network_interface.hh"

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    _interfaces.push_back( notnull( "add_interface", std::move( interface ) ) );
    return _interfaces.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return _interfaces.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

  // DEBUG: Show the routing table
  void Router_show() {
    for (auto rt : _routing_table) {
      std::cerr << "Route table: " << Address::from_ipv4_numeric(rt.route_prefix).ip() << "/" << std::hex << rt.subnet_mask << " => " << rt.next_hop.ip() << " on interface " << rt.interface_num << std::endl;
    }
  }

private:
  static uint8_t matched_bits( const uint32_t dst, const uint32_t src );

  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> _interfaces {};

  struct RouterInfo
  {
    uint32_t route_prefix;
    uint32_t subnet_mask;
    Address next_hop;
    size_t interface_num;
    uint32_t destination;

    RouterInfo()
      : route_prefix(0),
      subnet_mask(0),
      next_hop(Address::from_ipv4_numeric(0)),
      interface_num(0),
      destination(0) 
      {}
  };

  std::vector<RouterInfo> _routing_table {};
};
