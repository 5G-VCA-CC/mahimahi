#include <chrono>

#include "abstract_dualpi2_packet_queue.hh"
#include "dropping_packet_queue.hh"
#include "timestamp.hh"

#include <netinet/ip.h>
#include <arpa/inet.h>

#include <cstddef>

using namespace std;

void AbstractDualPI2PacketQueue::enqueue( QueuedPacket && p )
{
    print_ipv4_header( p ) ;

    queue_size_in_bytes_ += p.contents.size();
    queue_size_in_packets_++;
    internal_queue_.emplace( std::move( p ) );
}

QueuedPacket AbstractDualPI2PacketQueue::dequeue( void )
{
    assert( not internal_queue_.empty() );

    QueuedPacket ret = std::move( internal_queue_.front() );
    internal_queue_.pop();

    queue_size_in_bytes_ -= ret.contents.size();
    queue_size_in_packets_--;

    return ret;
}

bool AbstractDualPI2PacketQueue::empty( void ) const
{
    return internal_queue_.empty();
}

unsigned int AbstractDualPI2PacketQueue::size_bytes( void ) const
{
    assert( queue_size_in_bytes_ >= 0 );
    return unsigned( queue_size_in_bytes_ );
}

unsigned int AbstractDualPI2PacketQueue::size_packets( void ) const
{
    assert( queue_size_in_packets_ >= 0 );
    return unsigned( queue_size_in_packets_ );
}



string AbstractDualPI2PacketQueue::to_string( void ) const
{
    // string ret = type() + " [";

    // if ( byte_limit_ ) {
    //     ret += string( "bytes=" ) + ::to_string( byte_limit_ );
    // }

    // if ( packet_limit_ ) {
    //     if ( byte_limit_ ) {
    //         ret += ", ";
    //     }

    //     ret += string( "packets=" ) + ::to_string( packet_limit_ );
    // }

    // ret += "]";

    return "";
}

QueuedPacket& AbstractDualPI2PacketQueue::peek( void ) 
{
    return internal_queue_.front();
}

uint64_t AbstractDualPI2PacketQueue::qdelay_in_ns ( uint64_t ref ) 
{
    if ( internal_queue_.empty() ) return 0;
    
    QueuedPacket& head = peek();
    //return ref - head.arrival_time_ns;
    return ref - head.enqueue_time_ns;
}

// Utilities

unsigned int get_arg( const string & args, const string & name )
{
    return DroppingPacketQueue::get_arg( args, name );
}

void print_ipv4_header( QueuedPacket & p ) 
{   
    struct iphdr *ip_header = (struct iphdr *) &p.contents[4];
    struct in_addr sip;
    sip.s_addr = ip_header->saddr;
    struct in_addr dip;
    dip.s_addr = ip_header->daddr;
}

/* Calculate_ip_checksum function, borrowed from:
   https://github.com/prateshg/ABC-NSDI2020/blob/main/mahimahi/src/packet/cellular_packet_queue.hh
   */

/* set ip checksum of a given ip header*/
/* Compute checksum for count bytes starting at addr, using one's complement of one's complement sum*/
/* NOTE: The checksum field in the header should be set to 0 before calling this function! */
unsigned short calculate_ip_checksum(unsigned short *addr, unsigned int count) 
{
    register unsigned long sum = 0;
    while (count > 1) {
        sum += * addr++;
        count -= 2;
    }
    //if any bytes left, pad the bytes and add
    if(count > 0) {
        sum += ((*addr)&htons(0xFF00));
    }
    //Fold sum to 16 bits: add carrier to result
    while (sum>>16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    //one's complement
    sum = ~sum;
    return ((unsigned short)sum);
}
