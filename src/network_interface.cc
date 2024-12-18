#include <iostream>
#include <map>
#include <queue>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

# define ARP_BROADCAST_INTERVAL 5000
# define ARP_CACHE_TIMEOUT 30000

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
  , ms_boot_( 0 ), ms_last_broadcast_( 0 )
  , arp_cache_()
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{

  auto it = arp_cache_.find( next_hop.ipv4_numeric() );
  if (it != arp_cache_.end() && it->second.ethernet_address != ETHERNET_BROADCAST) {
    /**
     * If  the  destination  Ethernet  address  is  already  known,
     * send  it  right  away. 
     * Create an Ethernet frame (with type = EthernetHeader::TYPE_IPv4), 
     * set the payload  to  be  the  serialized  datagram,  
     * and  set the  source  and  destination addresses.
     */

    cout << "find dgram in arp_cache: "<< Address::from_ipv4_numeric( next_hop.ipv4_numeric() ).ip() << endl;
    show_arp_cache();

    EthernetFrame frame;
    frame.header.dst = it->second.ethernet_address;
    frame.header.src = ethernet_address_;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.payload = serialize( dgram );
    transmit( frame );

  } else if (it != arp_cache_.end() && it->second.ethernet_address == ETHERNET_BROADCAST) {
    if (ms_boot_ - it->second.timestamp > ARP_BROADCAST_INTERVAL) {
      // just timeout
      cout << "ARP: arp reply timeout so broadcast in:" << Address::from_ipv4_numeric( next_hop.ipv4_numeric() ).ip() << endl;
      //show_arp_cache();

      ARPMessage arp_request;
      arp_request.opcode = ARPMessage::OPCODE_REQUEST;
      arp_request.sender_ethernet_address = ethernet_address_;
      arp_request.sender_ip_address = ip_address_.ipv4_numeric();
      arp_request.target_ip_address = next_hop.ipv4_numeric();

      EthernetFrame arp_frame;
      arp_frame.header.dst = ETHERNET_BROADCAST;
      arp_frame.header.src = ethernet_address_;
      arp_frame.header.type = EthernetHeader::TYPE_ARP;
      arp_frame.payload = serialize(arp_request);

      ms_last_broadcast_ = ms_boot_;

      arp_cache_[next_hop.ipv4_numeric()].timestamp = ms_boot_;
      transmit( arp_frame );
    }
  } else {
    /**
     * If the destination Ethernet address is unknown,
     * broadcast an ARP request for the next hop's Ethernet address, 
     * and queue the IP datagram 
     * so it can be sent after the ARP reply is received.
     */
    cout << "ARP: can't find arp so broadcast in:" << Address::from_ipv4_numeric( next_hop.ipv4_numeric() ).ip() << endl;
    //show_arp_cache();

    ARPMessage arp_request;
    arp_request.opcode = ARPMessage::OPCODE_REQUEST;
    arp_request.sender_ethernet_address = ethernet_address_;
    arp_request.sender_ip_address = ip_address_.ipv4_numeric();
    arp_request.target_ip_address = next_hop.ipv4_numeric();

    EthernetFrame arp_frame;
    arp_frame.header.dst = ETHERNET_BROADCAST;
    arp_frame.header.src = ethernet_address_;
    arp_frame.header.type = EthernetHeader::TYPE_ARP;
    arp_frame.payload = serialize(arp_request);

    ms_last_broadcast_ = ms_boot_;

    arp_cache_[next_hop.ipv4_numeric()] = ARPEntry(ETHERNET_BROADCAST, ms_boot_, dgram);
    transmit( arp_frame );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // The Ethernet destination is either the broadcast address or the interface's own Ethernet address 
  // stored in the _ethernet_address member variable
  // cout << "recv0" << endl;
  if(frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST) {
    return;
  }
  //cout << "recv1" << endl;
  if(frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram dgram;
    if (parse(dgram, frame.payload)) {
      datagrams_received_.push( dgram );
    }
  } else if(frame.header.type == EthernetHeader::TYPE_ARP) {
    
    ARPMessage arp_message;
    
    parse(arp_message, frame.payload);

    if(arp_message.opcode == ARPMessage::OPCODE_REQUEST) {
      //cout << "reply1" << endl;
      if(arp_message.target_ip_address == ip_address_.ipv4_numeric()) {
        //cout << "reply2" << endl;
        
        // learn from ARP request
        arp_cache_[arp_message.sender_ip_address].ethernet_address = arp_message.sender_ethernet_address;
        arp_cache_[arp_message.sender_ip_address].timestamp = ms_boot_;

        ARPMessage arp_reply;
        
        arp_reply.opcode = ARPMessage::OPCODE_REPLY;
        arp_reply.sender_ethernet_address = ethernet_address_;
        arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
        arp_reply.target_ethernet_address = frame.header.src;
        arp_reply.target_ip_address = arp_message.sender_ip_address;

        EthernetFrame arp_reply_frame;
        arp_reply_frame.header.dst = frame.header.src;
        arp_reply_frame.header.src = ethernet_address_;
        arp_reply_frame.header.type = EthernetHeader::TYPE_ARP;
        arp_reply_frame.payload = serialize(arp_reply);

        
        transmit( arp_reply_frame );
      }
    } else if(arp_message.opcode == ARPMessage::OPCODE_REPLY) {
      //cout << "recv reply" << endl;
      arp_cache_[arp_message.sender_ip_address].ethernet_address = arp_message.sender_ethernet_address;
      arp_cache_[arp_message.sender_ip_address].timestamp = ms_boot_;

      EthernetFrame dgram_frame;
      dgram_frame.header.dst = arp_message.sender_ethernet_address;
      dgram_frame.header.src = ethernet_address_;
      dgram_frame.header.type = EthernetHeader::TYPE_IPv4;
      dgram_frame.payload = serialize((IPv4Datagram)arp_cache_[arp_message.sender_ip_address].dgram);
      
      transmit(dgram_frame);
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  ms_boot_ += ms_since_last_tick;

  // Expire  any  IP-to-Ethernet  mappings  that  have expired.
  for(auto it = arp_cache_.begin(); it != arp_cache_.end(); ) {
    if(ms_boot_ - it->second.timestamp > ARP_CACHE_TIMEOUT) {
      it = arp_cache_.erase( it );
    } else {
      ++it;
    }
  }
}
