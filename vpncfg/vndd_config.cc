/*
 *  This file is part of TVPN.
 *
 *  See also COPYING, README, README.md
 *
 *  Author: Antonino Calderone, <acaldmail@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */

#include "vndd_mgr.h"
#include "mac_addr.h"

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
#include <signal.h>
#include <map>
#include <list>
#include <string>
#include <sstream>
#include <algorithm>
#include <stdio.h>
#include <assert.h>
#include <memory> 
#include <iostream>


/* -------------------------------------------------------------------------- */

enum class request_t { NO_REQUEST, ADD_REQUEST, REMOVE_REQUEST };


/* -------------------------------------------------------------------------- */

#define DEFAULT_MTU 1500
#define DEFAULT_MAC  "02:00:00:00:00:00"


/* -------------------------------------------------------------------------- */

static void usage( std::ostream& os ) 
{
  static const std::string cdev_path = VNDDMGR_CDEV_DIR "" VNDDMGR_CDEV_NAME;

  os << "vpncfg add|remove <ifname> [...]\n"
     << "- add command parameters:\n"
     << "\tmtu <u16> (default " << DEFAULT_MTU << ")\n"
     << "\tmac <u8:u8:u8:u8:u8:u8> (default: " << DEFAULT_MAC << ")\n"
     << "- cdev (optional) parameter:\n"
     << "\tcdev <string> (default " << cdev_path << ")\n";
}


/* -------------------------------------------------------------------------- */

request_t parse_param(int argc, 
                char* argv[], 
                std::string& cdev_path,
                std::string& name,
                mac_addr_t & mac,
                int & mtu)
{
   if (argc<3) 
   {
      std::cerr << argv[0] << ": too few parameters" << std::endl;
      exit(1);
   }

   request_t request = request_t::NO_REQUEST;

   if (std::string(argv[1]) == "add") 
      request = request_t::ADD_REQUEST;
   else if (std::string(argv[1]) == "remove") 
      request = request_t::REMOVE_REQUEST;

   if (request == request_t::NO_REQUEST) 
   {
      std::cerr 
         << argv[0] 
         << ": 'add' or 'remove' not specified" 
         << std::endl;

      usage( std::cerr );
      exit(1);
   }

   char lname[IFNAMSIZ + 1] = {0}; 
   strncpy(lname, argv[2], IFNAMSIZ);
   name = lname;
   mtu = DEFAULT_MTU;

   for (int i=3; i<argc; ++i) 
   {
      std::string param = std::string(argv[i]);

      if (param.empty()) 
         return request;

      if (param == "cdev" && (i+1)<argc) 
         cdev_path = argv[++i];
      else if (param == "mac" && (i+1)<argc) 
         mac = std::string(argv[++i]);
      else if (param == "mtu" && (i+1)<argc) 
         mtu = atoi(argv[++i]) & 0xffff;
      else 
      {
         std::cerr 
            << argv[0] 
            << ": syntax error (cdev, mac or mtu missing)" 
            << std::endl;

         usage( std::cerr );

         exit(1);
      }      

   }

   return request;
}


/* -------------------------------------------------------------------------- */

int main(int argc, char* argv[]) 
{
   using vndd_mgr_ptr_t = std::unique_ptr<vndd::mgr_t>;
   vndd_mgr_ptr_t vnddmgr_obj;

   if (argc<3) 
   {
      usage( std::cerr );
      exit(1);
   }  

   std::string cdev_path = VNDDMGR_CDEV_DIR VNDDMGR_CDEV_NAME;

   mac_addr_t mac((const std::string)(DEFAULT_MAC));

   int mtu = 0;
   std::string name;

   request_t req = parse_param(argc, argv, cdev_path, name, mac, mtu);

   if (req != request_t::NO_REQUEST) 
   {
      vnddmgr_obj.reset( new vndd::mgr_t( cdev_path ) );

      if (vnddmgr_obj == nullptr) 
      {
         std::cerr << argv[0] << ": no enough memory" << std::endl;
         exit(1);
      }

      if ( vnddmgr_obj->open_dev() ) 
      {
         unsigned int ver = vnddmgr_obj->get_driver_ver();
         std::cout << argv[0] << ": driver version " 
                  << (ver >> 8) << "." << (ver & 0xff) << std::endl;
      }
      else 
      {
         std::cerr << argv[0] << ": failed opening driver '" 
                   << cdev_path << "'" << std::endl;

         exit(1);
      }

      if ( name.empty() ) 
      {
         std::cerr << argv[0] << ": 'name' parameter missing" << std::endl;

         exit(1);
      }
   }

   switch (req) 
   {
      case request_t::ADD_REQUEST:
         assert(vnddmgr_obj != nullptr);

         if ( vnddmgr_obj->add_if( name, mac, mtu ) < 0 ) 
         {
            std::cout << argv[0] << ": failed to create interface '"
                      << name << "' (mac='" << std::string(mac) 
                      << "', mtu=" << (mtu & 0xffff) 
                      << ")" << std::endl;
            exit(1);
         }
         else 
         {
            std::cout << argv[0] << ": interface '"
                      << name << "' (mac='" << std::string(mac) 
                      << "', mtu=" << (mtu & 0xffff) 
                      << ") has been successfully created"
                      << std::endl;
         }
         break;

      case request_t::REMOVE_REQUEST:
         assert(vnddmgr_obj != nullptr);

         if ( vnddmgr_obj->remove_if(name) < 0 ) 
         {
            std::cerr << argv[0] << ": failed to remove interface '" 
                      << name << "'" << std::endl;

            exit(1);
         }

         std::cout << argv[0] << ": interface '"
                      << name << "' has been successfully deleted"
                      << std::endl;

         break;

      case request_t::NO_REQUEST:
      default:
         std::cerr << argv[0] << ": invalid request" << std::endl; 
         usage( std::cerr );
         exit(1);
   }

   return 0;
}


/* -------------------------------------------------------------------------- */

