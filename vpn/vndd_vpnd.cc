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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "vndd_setup.h"
#include "vndd_mgr.h"
#include "vndd_tunnel.h"
#include "udp_socket.h"

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
#include <vector>
#include <map>
#include <list>
#include <set>
#include <sstream>
#include <algorithm>
#include <memory>
#include <stdio.h>
#include <assert.h>

#include <thread>
#include <string>
#include <map>
#include <memory>
#include <iostream>


/* -------------------------------------------------------------------------- */

static void signal_handler(int sig) 
{
   switch(sig) 
   {
      case SIGHUP:
         break;
      case SIGTERM:
         exit(0);
         break;
   }
}


/* -------------------------------------------------------------------------- */

static void daemonize(const std::string& loc_filename) 
{
   // already a daemon 
   if (getppid() == 1) 
      return; 

   int pid = fork();
   if (pid<0) exit(1); 
   if (pid>0) exit(0); 

   // close std file descriptors
   close(STDIN_FILENO);
   close(STDOUT_FILENO);
   close(STDERR_FILENO);

   // handle standard I/O
   int null_fd=open("/dev/null",O_RDWR); 
   int res = dup(null_fd); 
   if (res !=0) exit(1);

   res = dup(null_fd); 
   if (res !=0) exit(1);

   //unmask the file mode
   umask(0);

   //set new session
   pid_t sid = setsid();
   if(sid < 0) exit(1);

   // set running directory to tmp
   res = chdir("/tmp"); 
   if (res != 0) exit(1);

   int lfp = open(loc_filename.c_str(), O_RDWR|O_CREAT, 0640);

   // Cannot create the lock file
   if (lfp<0) exit(1); 

   // File already locked by other instance
   if (lockf(lfp, F_TLOCK, 0)<0) exit(1); 

   // First instance only continues
   // Record our pid within the lock file
   std::string pid_str = std::to_string( getpid() );
   res = write(lfp, pid_str.c_str(), pid_str.size()); 
   if (res<0) exit(1);

   // Set up the singnals
   signal(SIGCHLD,SIG_IGN); // ignore child 
   signal(SIGTSTP,SIG_IGN); // ignore tty signals
   signal(SIGTTOU,SIG_IGN);
   signal(SIGTTIN,SIG_IGN);
   signal(SIGUSR1,SIG_IGN);
   signal(SIGUSR2,SIG_IGN);

   signal(SIGHUP,signal_handler); // catch hangup signal 
   signal(SIGTERM,signal_handler); // catch kill signal

   vndd::setup_t::daemon_mode = true;
}


/* -------------------------------------------------------------------------- */

static void wait_for_termination() 
{
   sigset_t newmask;
   sigemptyset(&newmask);
   sigaddset(&newmask, SIGINT);
   sigaddset(&newmask, SIGHUP);
   sigaddset(&newmask, SIGTERM);

   sigprocmask(SIG_BLOCK, &newmask, NULL); 

   while (1) 
   { 
      int rcvd_sig = sigwaitinfo(&newmask, NULL);

      if (rcvd_sig == -1) 
      {
         perror("sigusr: sigwaitinfo");
         _exit(1);
      }
      else { 
         _exit(0);
      }
   }
}


/* -------------------------------------------------------------------------- */

static void usage( std::ostream & os ) 
{
   static const std::string cdev ( VNDDMGR_CDEV_DIR "" VNDDMGR_CDEV_NAME );

   os << "vnndvpnd -tunnel <tunnel_param> " 
      "[-tunnel <tunnel_param, ...>] [-cdev <cdevname>] [-daemonize]\n"
      "Where\n\t<tunnel_param> = if_name local_ip local_port "
      "remote_ip remote_port [-pwd <password>]\n\n"
      "\tdefault <cdevname> = '" << cdev << "'\n";
}


/* -------------------------------------------------------------------------- */

typedef std::pair< vndd::tunnel_t::tun_param_t, std::string > tunnel_info_t;  
typedef std::map< std::string, tunnel_info_t > tunnel_config_param_t;


/* -------------------------------------------------------------------------- */

static void parse_param(
      int argc, 
      char* argv[], 
      tunnel_config_param_t& tunnel_param,
      std::string& cdev, 
      bool & must_daemonize) 
{
   if (argc <= 1) 
   {
      usage( std::cout );
      exit(0);
   }

   must_daemonize = false;

   for (int i=1; i<argc; ++i) 
   {
      std::string token = argv[i];

      if (token == "-cdev" && i<(argc-1)) 
      {
         cdev = argv[++i];
      }
      else if (token == "-daemonize" && i<argc) 
      {
         must_daemonize = true;
      }
      else if (token == "-tunnel" && i<(argc-5)) 
      {
         // -tunnel ifname src_ip src_port dst_ip dst_port [password]
         std::string ifname = argv[++i];                     //1st param

         if (ifname.size() > IFNAMSIZ) 
            ifname = ifname.substr(0, IFNAMSIZ);

         ip_address_t src_ip = std::string(argv[++i]);       //2nd param
         udp_socket_t::port_t source_port = atoi(argv[++i]); //3rd param
         ip_address_t dst_ip = std::string(argv[++i]);       //4th param
         udp_socket_t::port_t dest_port = atoi(argv[++i]);   //5th param

         std::string pwd;

         if (i < (argc-2)) 
         {
            pwd = argv[++i];                    //6th optional param 

            if (pwd == "-pwd") 
               pwd = argv[++i];
            else 
               --i; //undo pop parameter
         }

         tunnel_info_t tinfo ( 
               std::make_pair
               ( vndd::tunnel_t::tun_param_t( 
                                             src_ip, source_port, dst_ip, dest_port ), pwd));

         std::pair<std::string, tunnel_info_t> tp(ifname, tinfo);

         std::cout << "vnddvpnd setting up "
            << src_ip << ":" << source_port << "<->" 
            << dst_ip << ":" << dest_port << " if='"
            << ifname << "'" << std::endl; 

         if (! tunnel_param.insert( tp ).second ) 
         {
            std::cerr << "vnddvpnd failed: reuse of ifname not allowed\n";
            usage( std::cerr );
            exit(1);
         }
      }
      else 
      {
         std::cerr << "vnddvpnd failed: invalid arguments\n";
         usage( std::cerr );
         exit(1);
      }
   }
}


/* -------------------------------------------------------------------------- */

int main(int argc, char* argv[]) 
{
   signal(SIGPIPE, SIG_IGN);

   // Parse command line to load the tunnels configuration
   tunnel_config_param_t tunnel_param;
   std::string cdev;
   bool must_daemonize = false;

   parse_param(argc, argv, tunnel_param, cdev, must_daemonize);

   if ( tunnel_param.empty() ) 
   {
      usage( std::cerr );
      exit(1);
   }

   if (cdev.empty()) 
   {
      cdev = VNDDMGR_CDEV_DIR VNDDMGR_CDEV_NAME;
   }

   // Open vnddmgr device
   std::shared_ptr<vndd::mgr_t> vnddmgr_obj ( new vndd::mgr_t( cdev ) );

   if (! vnddmgr_obj ) 
   {
      std::cerr << "vnddvpnd failed: no enough memory\n";
      exit(1);
   }

   if ( vnddmgr_obj->open_dev() ) 
   {
      int ver = vnddmgr_obj->get_driver_ver();

      std::cout << "Using vnddmgr module version " << 
         (ver >> 8) << "." << (ver & 0xff) << std::endl;
   }
   else {
      std::cerr << "vnddvpnd failed: cannot open '%s'" 
         << cdev << std::endl;
      exit(1);
   }

   // Create a tunnel manager instance
   std::auto_ptr<vndd::tunnelmgr_t> tmgr ( new vndd::tunnelmgr_t() );

   if (must_daemonize) 
      daemonize ( vndd::setup_t::lockfilename.c_str() );

   // Create the tunnels for each configured vndd interface
   for ( tunnel_config_param_t::const_iterator i = tunnel_param.begin();
         i != tunnel_param.end();
         ++i )
   {
      try {
         tmgr->add_tunnel( 
               i->first,          /* ifname */
               i->second.first,   /* tun_param_t */
               vnddmgr_obj,       /* vnddmgr instance  */ 
               i->second.second   /* password 
                                     (if empty tunnel won't be encrypted) */);
      }
      catch (vndd::tunnel_t::excp_t & e) 
      {
         if ( ! vndd::setup_t::daemon_mode)
            std::cerr << "vnddvpnd failed to create tunnel instance for '" 
               << i->first << "'" << std::endl;
         continue;
      }
   }

   // Verify if we have at least one tunnel instance
   if (tmgr->empty()) 
   {
      if ( ! vndd::setup_t::daemon_mode)
         std::cerr << "vnddvpnd failed: no tunnel instances specified" 
            << std::endl; 
      exit(1);
   }  


   std::cout << "Logging on " << NU_LOGGER_LOG << std::endl;

   wait_for_termination();
}

