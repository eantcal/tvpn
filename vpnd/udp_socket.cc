/*
 *  This file is part of TVPN.
 *
 *  See also COPYING, README, README.md
 *
 *  Author: Antonino Calderone, <antonino.calderone@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */

#include "udp_socket.h"


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/* -------------------------------------------------------------------------- */

void udp_socket_t::_format_sock_addr(sockaddr_in & sa, 
      const ip_address_t& addr,
      port_t port) noexcept
{
   memset(&sa, 0, sizeof(sa));

   sa.sin_family = AF_INET;
   sa.sin_addr.s_addr = htonl(addr.to_uint32());
   sa.sin_port = htons(port);
}


/* -------------------------------------------------------------------------- */

udp_socket_t::udp_socket_t() noexcept : 
   _sock(::socket(AF_INET, SOCK_DGRAM, 0))
{
   assert( _sock > -1 );
}


/* -------------------------------------------------------------------------- */

udp_socket_t::~udp_socket_t() noexcept
{
   close();
}


/* -------------------------------------------------------------------------- */

int udp_socket_t::sendto(
      const char* buf,
      int len, 
      const ip_address_t& ip,
      port_t port, 
      int flags) const noexcept
{
   struct sockaddr_in remote_host = {0}; 

   _format_sock_addr(remote_host, ip, port);

   int bytes_sent = ::sendto(get_sd(),
      buf, len,
      flags,
      (struct sockaddr*) &remote_host,
      sizeof(remote_host));

   return bytes_sent;
}


/* -------------------------------------------------------------------------- */

int udp_socket_t::recvfrom(
      char *buf, 
      int len,
      ip_address_t& src_addr,
      port_t& src_port,
      int flags) const noexcept

{
   struct sockaddr_in source; 
   socklen_t sourcelen = sizeof(source);
   memset(&source, 0, sizeof(source));

   int recv_result = ::recvfrom(
      get_sd(), buf, len, 
      flags, (struct sockaddr*)&source, 
      &sourcelen);

   src_addr = ip_address_t(htonl(source.sin_addr.s_addr));
   src_port = port_t(htons(source.sin_port));

   return recv_result;
}


/* -------------------------------------------------------------------------- */

udp_socket_t::pollst_t udp_socket_t::poll(struct timeval& timeout) const noexcept
{
   fd_set readMask;
   FD_ZERO(&readMask);
   FD_SET(get_sd(), &readMask);

   long nd = 0;

   do 
   {
      nd = ::select (FD_SETSIZE, &readMask, 0, 0, &timeout);
   } 
   while (! FD_ISSET(_sock, &readMask) && nd > 0);

   if (nd <= 0) 
      return nd == 0 ? 
         pollst_t::TIMEOUT_EXPIRED :
         pollst_t::ERROR_IN_COMMUNICATION;

   return pollst_t::RECEIVING_DATA;
}


/* -------------------------------------------------------------------------- */

bool udp_socket_t::bind(
   port_t& port,
   const ip_address_t& ip,
   bool reuse_addr) const noexcept
{
   struct sockaddr_in sin = {0};
   socklen_t address_len = 0;

   if (reuse_addr) 
   {
      int bool_val = 1;

      setsockopt(
         get_sd(), 
         SOL_SOCKET, 
         SO_REUSEADDR, 
         (const char*) &bool_val, 
         sizeof(bool_val));
   }

   _format_sock_addr(sin, ip, port);

   int sock = get_sd();
   int err_code = ::bind (sock, (struct sockaddr *) &sin, sizeof(sin));

   if (port == 0) 
   {
      address_len = sizeof(sin);
      getsockname(sock, (struct sockaddr *) &sin, &address_len);
      port = ntohs(sin.sin_port);
   }

   return err_code == 0;
}


/* -------------------------------------------------------------------------- */

bool udp_socket_t::close() noexcept
{
   int sock = _sock;
   _sock = -1;

   return ::close(sock) == 0;
}


/* -------------------------------------------------------------------------- */

