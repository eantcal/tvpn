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

#ifndef __VNDD_MGR_H__
#define __VNDD_MGR_H__


/* -------------------------------------------------------------------------- */

extern "C" 
{
   #include "../vnddmgr.h"
}


/* -------------------------------------------------------------------------- */

#include "mac_addr.h"
#include "ip_addr.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>


/* -------------------------------------------------------------------------- */

namespace vndd
{


/* -------------------------------------------------------------------------- */

class mgr_t 
{
   public: 
      using driver_version_t=int;

   private:
      std::string _if_name;
      int _dev_fd = -1;

      void _init_cdev_request(
            cdev_request_t& irp, 
            int cdev_request_cmd) const throw();

   public:
      mgr_t(
            const std::string& device_name = VNDDMGR_CDEV_DIR VNDDMGR_CDEV_NAME ) 
         : _if_name(device_name) { }

      ~mgr_t() 
      { 
         if (! is_dev_fd_ok()) 
            close_dev(); 
      }

      bool open_dev() throw () 
      { 
         return _dev_fd = ::open(_if_name.c_str(), O_RDWR), is_dev_fd_ok(); 
      }

      bool close_dev() throw() 
      { 
         int fd = _dev_fd; 
         _dev_fd = 0;

         return ::close(fd) > -1; 
      }

      bool is_dev_fd_ok() const throw() 
      { 
         return _dev_fd > -1; 
      }

      driver_version_t get_driver_ver() const throw() 
      { 
         return (driver_version_t) ioctl(_dev_fd, VNDDMGR_CDEV_IOGETVER);
      }


      ssize_t remove_if(const std::string& ifname) const throw();

      ssize_t add_if(
         const std::string& ifname, 
         const mac_addr_t& mac, 
         int mtu) const throw();


      ssize_t announce_packet(
         const std::string& ifname, 
         const char* data, 
         size_t datalen) const throw();

      ssize_t get_packet(
         char* buf, 
         size_t bufsize, 
         std::string& ifname) const throw();
};


/* -------------------------------------------------------------------------- */

} // namespace vndd


/* -------------------------------------------------------------------------- */

#endif // __VNDD_MGR_H__

