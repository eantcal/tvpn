/*
 *  This file is part of TVPN.
 *
 *  TVPN is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  TVPN is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with TVPN; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  US
 *
 *  Author:	Antonino Calderone, <acaldmail@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */

#ifndef __UDP_SOCKET_H__
#define __UDP_SOCKET_H__


/* -------------------------------------------------------------------------- */

#include "ip_addr.h"

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
#include <cstdint>


/* -------------------------------------------------------------------------- */

class udp_socket_t 
{
   public:
      enum class pollst_t 
      {
         RECEIVING_DATA,
         TIMEOUT_EXPIRED,
         ERROR_IN_COMMUNICATION
      };

      typedef uint16_t port_t;

   private:
      int _sock = -1;

      static void _format_sock_addr(
         sockaddr_in & sa, 
         const ip_address_t& addr, 
         port_t port) noexcept;
      
      //copy not allowed
      udp_socket_t(const udp_socket_t&) = delete;
      udp_socket_t& operator=(const udp_socket_t&) = delete;
     
   public:
      udp_socket_t(udp_socket_t&& s) noexcept
      {
         //xfer socket descriptor to constructing object
         _sock = s._sock;
         s._sock = -1;
      }

      udp_socket_t& operator=(udp_socket_t&& s) noexcept
      {
         if (&s != this)
         {  
            //xfer socket descriptor to constructing object
            _sock = s._sock;
            s._sock = -1;
         }

         return *this;
      }


      int get_sd() const noexcept 
      { 
         return _sock; 
      }


      bool is_valid() const noexcept
      { 
         return get_sd() > -1;
      }


      udp_socket_t() noexcept;

      virtual ~udp_socket_t() noexcept;

      pollst_t poll(struct timeval& timeout) const noexcept;

      bool close() noexcept;

      int sendto(
         const char* buf, 
         int len, 
         const ip_address_t& ip, 
         port_t port, 
         int flags = 0
      ) const noexcept;


      int recvfrom(
         char *buf, 
         int len, 
         ip_address_t& src_addr, 
         port_t& src_port, 
         int flags = 0
      ) const noexcept;


      bool bind( 
         port_t& port, 
         const ip_address_t& ip = ip_address_t(INADDR_ANY), 
         bool reuse_addr = true
      ) const noexcept;
};


/* -------------------------------------------------------------------------- */

#endif //__UDP_SOCKET_H__


/* -------------------------------------------------------------------------- */
