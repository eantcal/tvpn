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

#ifndef __VNDD_TUNNEL_H__
#define __VNDD_TUNNEL_H__


/* -------------------------------------------------------------------------- */

#include "udp_socket.h"
#include "ip_addr.h"
#include "vndd_mgr.h"

#include <thread>
#include <mutex>
#include <string>
#include <sstream>
#include <map>
#include <memory>


/* -------------------------------------------------------------------------- */

namespace vndd 
{


/* -------------------------------------------------------------------------- */

class tunnel_t 
{
   public:
      using handle_t = std::shared_ptr<tunnel_t>;

      enum class excp_t 
      { 
         BINDING_SOCKET_ERROR, 
         INVALID_SOCKET_ERROR 
      };

      struct tun_param_t 
      {
         private:
            ip_address_t _local_ip; 
            udp_socket_t::port_t _local_port;
            ip_address_t _remote_ip; 
            udp_socket_t::port_t _remote_port;

            tun_param_t() = delete;
         public:

            tun_param_t(const tun_param_t&) = default;
            tun_param_t& operator=(const tun_param_t&) = default;

            const ip_address_t& local_ip() const noexcept
            {
               return _local_ip;
            }

            const ip_address_t& remote_ip() const noexcept
            {
               return _remote_ip;
            }

            const udp_socket_t::port_t& local_port() const noexcept
            {
               return _local_port;
            }

            const udp_socket_t::port_t& remote_port() const noexcept
            {
               return _remote_port;
            }

            explicit inline tun_param_t(
                  const ip_address_t& local_ip_, 
                  udp_socket_t::port_t local_port_,
                  const ip_address_t& remote_ip_, 
                  udp_socket_t::port_t remote_port_ ) : 
               _local_ip(local_ip_),
               _local_port(local_port_),
               _remote_ip(remote_ip_),
               _remote_port(remote_port_) 
         {} 
      };

   private:
      ip_address_t _local_ip; 
      udp_socket_t::port_t _local_port;
      ip_address_t _remote_ip; 
      udp_socket_t::port_t _remote_port;

      udp_socket_t _tunnel_socket; 

      std::string _des_key;

      mutable std::recursive_mutex _lock;
      using lock_guard_t = std::lock_guard<std::recursive_mutex>;

      bool _remove_req_pending = false;

      void _create_tunnel_ch() throw(excp_t);

      tunnel_t () = delete;
      tunnel_t ( const tunnel_t& obj ) = delete;
      tunnel_t& operator=(const tunnel_t& obj) = delete;

   public:
      ip_address_t get_local_ip() const noexcept
      { 
         return _local_ip; 
      }


      ip_address_t get_remote_ip() const noexcept
      { 
         return _remote_ip; 
      }


      udp_socket_t::port_t get_local_port() const noexcept
      { 
         return _local_port; 
      }


      udp_socket_t::port_t get_remote_port() const noexcept
      { 
         return _remote_port; 
      }


      bool remove_req_pending() const noexcept
      { 
         return _remove_req_pending; 
      }


      void notify_remove_req_pending() noexcept
      { 
         _remove_req_pending = true; 
      }


      void lock() noexcept
      { 
         _lock.lock(); 
      }


      void unlock() noexcept
      { 
         _lock.unlock(); 
      }


      bool try_lock() noexcept
      { 
         return _lock.try_lock(); 
      }


      tunnel_t( const tun_param_t& tp ) throw(excp_t);


      friend std::stringstream& operator << (
            std::stringstream& os, const tunnel_t& tunnel) 
      {
         return os << std::string(tunnel), os;
      }


      bool equals(const tunnel_t& obj) const noexcept
      {
         return 
            _local_ip == obj._local_ip &&
            _local_port == obj._local_port &&
            _remote_ip == obj._remote_ip &&
            _remote_port == obj._remote_port;
      }


      bool operator==(const tunnel_t& obj) const noexcept
      {
         return equals(obj);
      }


      bool operator!=(const tunnel_t& obj) const noexcept
      {
         return ! equals(obj);
      }


      udp_socket_t& tunnel_socket() noexcept
      { 
         return _tunnel_socket; 
      }


      operator std::string() const noexcept
      {
         std::stringstream os;

         os <<  _local_ip.to_str().c_str()  << ":" << _local_port << "<->" 
            <<  _remote_ip.to_str().c_str() << ":" << _remote_port;

         return os.str();
      }
};


/* -------------------------------------------------------------------------- */

class tunnelmgr_t 
{
   public:
      enum class excp_t { TUNNEL_INSTANCE_NOT_FOUND };

   private:
      using lock_guard_t = std::lock_guard<std::recursive_mutex>;
      using dev2tunnel_map_t = std::map < std::string, tunnel_t::handle_t >;
      using rpeer_dev_pair_t = std::pair<uint32_t, uint16_t>;
      using rpeer2dev_map_t = std::map < rpeer_dev_pair_t, std::string >;
      using thread_handle_t = std::unique_ptr< std::thread >;


      mutable std::recursive_mutex _lock;
      thread_handle_t _tunnel_xmit_thread;
      dev2tunnel_map_t _dev2tunnel;
      rpeer2dev_map_t _rpeer2dev;


      static int tunnel_recv_thread(
            const std::string& name_, 
            vndd::tunnelmgr_t* tvm_, 
            std::shared_ptr<vndd::mgr_t> vnddmgr_, 
            const std::string& des_key_);


      static int tunnel_xmit_thread(
            const std::string& des_key_, 
            vndd::tunnelmgr_t* tvm_, 
            std::shared_ptr<vndd::mgr_t> vnddmgr_);


      tunnelmgr_t(const tunnelmgr_t&) = delete;
      tunnelmgr_t& operator=(const tunnelmgr_t&) = delete;


   public:
      tunnelmgr_t() = default; 


      ~tunnelmgr_t() 
      { 
         _tunnel_xmit_thread->join();
      }


      size_t size() const noexcept
      {
         lock_guard_t cs ( _lock );

         return _dev2tunnel.size();
      }


      bool empty() const noexcept
      { 
         return size() == 0; 
      }


      bool add_tunnel(
            const std::string& ifname, 
            const tunnel_t::tun_param_t& tp,
            std::shared_ptr<mgr_t> vnddmgr_ptr,
            const std::string& password = std::string());


      bool find_tunnel_for_if(const std::string& ifname) const noexcept
      {
         lock_guard_t cs ( _lock );
         return _dev2tunnel.find(ifname) != _dev2tunnel.end();
      }


      tunnel_t& get_tunnel_instance(const std::string& ifname) throw(excp_t);


      bool remove_tunnel(const std::string& ifname) noexcept;


      friend std::stringstream& operator<<(
            std::stringstream& os, 
            const tunnelmgr_t& vtm) 
      {
         lock_guard_t cs ( vtm._lock );

         for (const auto &e : vtm._dev2tunnel) 
            os << e.first << " " << std::string( *e.second ) << std::endl;

         return os;
      }
};  


/* -------------------------------------------------------------------------- */

} // namespace vndd 


/* -------------------------------------------------------------------------- */

#endif //__VNDD_TUNNEL_H__


/* -------------------------------------------------------------------------- */

