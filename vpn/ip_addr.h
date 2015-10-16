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
 *  Author: Antonino Calderone, <acaldmail@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */

#ifndef __IP_ADDR_H__
#define __IP_ADDR_H__


/* -------------------------------------------------------------------------- */

#include <string>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdint>
#include <sstream>
#include <ostream>


/* -------------------------------------------------------------------------- */

class ip_address_t 
{
   private:
      using _ip_addr_t = uint32_t;

      typedef char _ip_addr_v_t[sizeof(_ip_addr_t)];

      union _ip_addr_u_t 
      {
         _ip_addr_t _ip_addr;
         _ip_addr_v_t _ip_addr_v;
      } _ip_u;

   public:
      ip_address_t(unsigned int ip_addr = 0) noexcept
      { 
         _ip_u._ip_addr = ip_addr;
      }


      ip_address_t(const std::string& ip_addr) noexcept
      {
         in_addr_t ip_addr_value = inet_addr(ip_addr.c_str());
         _ip_u._ip_addr = ntohl(((in_addr*) &ip_addr_value)->s_addr);
      }


      ip_address_t(const ip_address_t& ip_obj) noexcept
      {
         _ip_u._ip_addr = ip_obj._ip_u._ip_addr;
      }


      ip_address_t& operator=(const ip_address_t& ip_obj) noexcept
      {
         if (this != &ip_obj) 
            _ip_u._ip_addr = ip_obj._ip_u._ip_addr;

         return *this;
      }


      bool operator==(const ip_address_t& ip_obj) const noexcept
      { 
         return _ip_u._ip_addr == ip_obj._ip_u._ip_addr;
      }


      char& operator[](size_t i) noexcept
      {
#ifndef BIGENDIAN
         return _ip_u._ip_addr_v[ 3-i ];
#else
         return _ip_u._ip_addr_v[ i ];
#endif
      }


      std::string to_str() const noexcept
      {
         std::stringstream ss;
         ss << ((_ip_u._ip_addr >> 24) & 0xff) << "."
            << ((_ip_u._ip_addr >> 16) & 0xff) << "."
            << ((_ip_u._ip_addr >> 8) & 0xff)  << "."
            << ((_ip_u._ip_addr >> 0) & 0xff);

         return ss.str();
      }


      friend std::ostream& operator<<(std::ostream& os, const ip_address_t& ip)
      {
         os << ip.to_str();
         return os;
      }


      explicit operator std::string() const noexcept
      { 
         return to_str(); 
      }


      explicit operator unsigned int() const noexcept
      { 
         return to_n<unsigned int>(); 
      }


      explicit operator unsigned long() const noexcept
      {
         return to_n<unsigned long>(); 
      }


      explicit operator int() const noexcept
      { 
         return to_n<int>(); 
      }


      explicit operator long() const noexcept
      { 
         return to_n<long>(); 
      }


      template<class T> T to_n() const noexcept
      { 
         return (T) (_ip_u._ip_addr); 
      }


      unsigned int to_uint32() const noexcept
      { 
         return to_n<uint32_t>(); 
      }


      int to_int() const noexcept
      { 
         return to_n<int>(); 
      }


      unsigned long to_ulong() const noexcept
      { 
         return to_n<unsigned long>(); 
      }


      long to_long() const noexcept
      {
         return to_n<long>(); 
      }
};


/* -------------------------------------------------------------------------- */

#endif // __IP_ADDR_H__

