/*
 *  vndd_mgr.cc
 *
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


// ----------------------------------------------------------------------------- 

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <features.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <assert.h>
#include <string>

#include "vndd_mgr.h"
#include "mac_addr.h"
#include "ip_addr.h"


// ----------------------------------------------------------------------------- 

void vndd::mgr_t::_init_cdev_request(
      cdev_request_t& cdevreq, 
      int cdev_request_cmd) const throw() 
{
   memset(&cdevreq, 0, sizeof(cdevreq));

   cdevreq.cmd_code = cdev_request_cmd;
   memcpy(cdevreq.magic_cookie,  
         CDEV_REQUEST_MAGIC_COOKIE, 
         sizeof(CDEV_REQUEST_MAGIC_COOKIE));
}


// ----------------------------------------------------------------------------- 

ssize_t vndd::mgr_t::add_if(
   const std::string& ifname, 
   const mac_addr_t& mac, int mtu) const throw() 
{
   cdev_request_t cdevreq;
   _init_cdev_request(cdevreq, CDEV_REQUEST_CMD_ADD_IF);

   strcpy(cdevreq.cdevreq_spec_param.add_eth_if.device_name, 
         ifname.substr(0, IFNAMSIZ).c_str());

   mac.assign_to(cdevreq.cdevreq_spec_param.add_eth_if.hw_address);
   cdevreq.cdevreq_spec_param.add_eth_if.mtu = mtu;

   size_t cdevreq_len = CDEV_REQUEST_HEADER_SIZE + 
      sizeof(cdevreq.cdevreq_spec_param.add_eth_if);

   return ::write(_dev_fd, (const void *) &cdevreq, cdevreq_len);
}


// ----------------------------------------------------------------------------- 

ssize_t vndd::mgr_t::remove_if(const std::string& ifname) const throw() 
{
   cdev_request_t cdevreq;
   _init_cdev_request(cdevreq, CDEV_REQUEST_CMD_REMOVE_IF);

   strcpy(cdevreq.cdevreq_spec_param.remove_eth_if.device_name, 
         ifname.substr(0, IFNAMSIZ).c_str());

   size_t cdevreq_len = CDEV_REQUEST_HEADER_SIZE + 
      sizeof(cdevreq.cdevreq_spec_param.remove_eth_if);

   return ::write(_dev_fd, (const void *) &cdevreq, cdevreq_len);
}


// ----------------------------------------------------------------------------- 

ssize_t vndd::mgr_t::announce_packet(
      const std::string& ifname, 
      const char* data, 
      size_t datalen) const throw() 
{
   cdev_request_t cdevreq;
   _init_cdev_request(cdevreq, CDEV_REQUEST_CMD_ANNOUNCE_TO_IF);

   strcpy(cdevreq.cdevreq_spec_param.announce_pkt.device_name, 
         ifname.substr(0, IFNAMSIZ).c_str());

   size_t dlen = datalen < CDEV_REQUEST_MAX_LENGTH ? 
      datalen : CDEV_REQUEST_MAX_LENGTH;

   memcpy(cdevreq.cdevreq_spec_param.announce_pkt.pkt_buffer, data, dlen);
   cdevreq.cdevreq_spec_param.announce_pkt.pkt_len = dlen;

   size_t cdevreq_len = dlen + CDEV_REQUEST_ANNOUNCE_PKT_H_LEN;

   return ::write(_dev_fd, (const void *) &cdevreq, cdevreq_len);
}


// ----------------------------------------------------------------------------- 

ssize_t vndd::mgr_t::get_packet(char* buf, size_t bufsize, 
      std::string& ifname) const throw() 
{
   assert(buf && bufsize>=0);

   if (bufsize == 0) 
      return 0;

   ssize_t readBytes = 0;

   //reserve the space for both packet data and ifname
   ssize_t lbuf_size = bufsize+IFNAMSIZ; 

   char* lbuf = new char [lbuf_size];
   assert (lbuf);
   memset(lbuf, 0, lbuf_size);

   // Each buffer received from the driver has an header which 
   // contains the name of the inbound interface 
   if ( (readBytes = ::read(_dev_fd, lbuf, lbuf_size)) <= IFNAMSIZ) 
   {
      delete [] lbuf;
      lbuf = 0;

      return readBytes;
   }

   //parse frame payload ...
   memcpy(buf, lbuf+IFNAMSIZ, readBytes-IFNAMSIZ);

   //... and the interface name 
   char ifname_buf [IFNAMSIZ + 1] = {0};
   memcpy(ifname_buf, lbuf, IFNAMSIZ);
   ifname = ifname_buf;

   delete [] lbuf;

   //return the payload length
   return readBytes-IFNAMSIZ;
}


// ----------------------------------------------------------------------------- 

