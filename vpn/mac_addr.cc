/*
 *  This file is part of TVPN.
 *
 *  vnddmgr is free software; you can redistribute it and/or modify
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

#include "mac_addr.h"


/* -------------------------------------------------------------------------- */

std::string mac_addr_t :: _getpstr() const noexcept
{
   // reserve space for (at least) 2 digit + ":" + null termination char
   static const int LBUFDIGITSIZE=8; 

   std::string mac;
   ///std::array<char,LBUFDIGITSIZE> lbuf;
   char lbuf[LBUFDIGITSIZE] = {0};

   for (auto data : _macaddr) 
   {
      snprintf(&lbuf[0], LBUFDIGITSIZE-1, "%02x:", data & 0xFF);
      mac += &lbuf[0];
   }

   mac[mac.size()-1] = '\x0';

   return mac;
}


/* -------------------------------------------------------------------------- */

mac_addr_t :: mac_addr_t(const std::string& mac) noexcept
{
   if (mac.size() != (sizeof("xx:xx:xx:xx:xx:xx")-1))
      return;

   for (size_t i=0; i<sizeof(_macaddr); ++i) 
      _macaddr[i] = _getbs2(mac.c_str()+i*3);
}


/* -------------------------------------------------------------------------- */

mac_addr_t& mac_addr_t :: operator=(const mac_addr_t& mac_obj) noexcept
{
   if (this != &mac_obj) 
      memcpy(&_macaddr[0], & mac_obj._macaddr[0], MAC_LEN);

   return *this;
}


/* -------------------------------------------------------------------------- */

char mac_addr_t :: operator[](size_t i) const noexcept
{
   if (i>=0 && i<sizeof(_macaddr)) 
      return _macaddr[i];

   return 0;
}


/* -------------------------------------------------------------------------- */

char& mac_addr_t :: operator[](size_t i) noexcept
{
   static char dummy = 0;

   if (i>=0 && i<sizeof(_macaddr)) 
      return _macaddr[i];

   return dummy;
}


/* -------------------------------------------------------------------------- */

