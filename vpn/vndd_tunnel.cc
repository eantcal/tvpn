/*
 *  vndd_tunnel.cc
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

#include "vndd_setup.h"
#include "vndd_tunnel.h"
#include "vndd_mgr.h"
#include "des_crypt.h"
#include "udp_socket.h"

#include <rpc/des_crypt.h>
#include <cassert>
#include <string>
#include <sstream>
#include <map>

// -----------------------------------------------------------------------------

class crypt_buf_t 
{
   private:
      size_t _orglen = -1;
      size_t _len8 = -1;
      std::unique_ptr<char[]> _bufptr;

      crypt_buf_t() = delete;
      crypt_buf_t(const crypt_buf_t&) = delete;
      crypt_buf_t& operator=(const crypt_buf_t&) = delete;
   
   public:
      using handle_t = std::unique_ptr< crypt_buf_t >;

      crypt_buf_t(const char* buf, size_t buflen, const char * keystr) 
      throw ( des_exc_t ) : _orglen(buflen) 
      {
         assert(buf && buflen && keystr);      

         _len8 = ((_orglen + 4) & 7) ? (((_orglen + 4) & ~7) + 8) : _orglen + 4;
         _bufptr = std::unique_ptr<char[]> ( new char[ _len8 ] );

         assert( _bufptr != nullptr );

         _bufptr[0] = (_orglen >> 24) & 0xff;
         _bufptr[1] = (_orglen >> 16) & 0xff;
         _bufptr[2] = (_orglen >>  8) & 0xff;
         _bufptr[3] = _orglen         & 0xff;

         memcpy( _bufptr.get() + 4, buf, buflen );

         des_key_t key ( keystr );

         try 
         {
            des_t::get_instance() ( 
                  des_t::method_t::ENCRYPT_BLOCK,
                  key, _bufptr.get(), _len8
                  );
         }
         catch( des_exc_t & e ) 
         {
            if ( e.get_error_code() != DESERR_NOHWDEVICE ) 
               throw;
         }
      }

      inline const char* get_buf() const { return _bufptr.get(); }
      inline size_t get_buf_len() const { return _len8; }
};


// -----------------------------------------------------------------------------

class decrypt_buf_t 
{
   private:
      size_t _orglen = -1;
      std::unique_ptr<char[]> _bufptr;
      
      decrypt_buf_t() = delete;
      decrypt_buf_t(const decrypt_buf_t&) = delete;
      decrypt_buf_t& operator=(const decrypt_buf_t&) = delete;
   

   public:
      using handle_t = std::unique_ptr<decrypt_buf_t>;
   
      decrypt_buf_t(const char* buf, size_t buflen, const char * keystr) 
         throw ( des_exc_t )
         : _orglen(buflen) 
         {
            assert(buf && 
                  buflen>=8 && 
                  ((buflen & 7)==0) && 
                  keystr);      

            _bufptr = std::unique_ptr<char[]> ( new char [ buflen ] );
   
            assert( _bufptr != nullptr );

            memcpy( _bufptr.get(), buf, buflen );
            des_key_t key ( keystr );

            try 
            {
               des_t::get_instance() 
                  ( des_t::method_t::DECRYPT_BLOCK, key, _bufptr.get(), buflen);
            }
            catch( des_exc_t & e ) 
            {
               //Ignore DESERR_NOHWDEVICE (this causes a performace issue, 
               //but it is not blocking error)
               if ( e.get_error_code() != DESERR_NOHWDEVICE ) 
                  throw; 
            }

            _orglen = 
               ( _bufptr[3] & 0xff) |
               ((_bufptr[2] & 0xff)<<8)  |
               ((_bufptr[1] & 0xff)<<16) |
               ((_bufptr[0] & 0xff)<<24);
         }

      inline const char* get_buf() const { return _bufptr.get() + 4; }
      inline size_t get_buf_len() const { return _orglen; }
};


// -----------------------------------------------------------------------------

void vndd::tunnel_t::_create_tunnel_ch() throw(excp_t) 
{
   if ( ! _tunnel_socket.is_valid() )
      throw excp_t::INVALID_SOCKET_ERROR;
      
   if ( ! _tunnel_socket.bind( _local_port, _local_ip,  
            true /* reuse addr && port */))
      throw excp_t::BINDING_SOCKET_ERROR;
}


// -----------------------------------------------------------------------------

vndd::tunnel_t::tunnel_t( const vndd::tunnel_t::tun_param_t& tp ) 
throw(vndd::tunnel_t::excp_t) :
  _local_ip(tp.local_ip()),
  _local_port(tp.local_port()),
  _remote_ip(tp.remote_ip()),
  _remote_port(tp.remote_port())
{
   _create_tunnel_ch();
}


// -----------------------------------------------------------------------------

int vndd::tunnelmgr_t::tunnel_recv_thread(
      const std::string& name, 
      vndd::tunnelmgr_t* tvm_ptr, 
      vndd::mgr_t* vnddmgr_ptr, 
      const std::string& keystr)
{
   assert ( tvm_ptr );
   assert ( vnddmgr_ptr );

   try 
   {
      vndd::tunnel_t & tunnel = 
         tvm_ptr->get_tunnel_instance( name ); 

      // Manager will wait for unloking the mutex
      // while thread is terminanting
      struct tunnel_lock_grd_t
      {
         vndd::tunnel_t & _t;

         tunnel_lock_grd_t(vndd::tunnel_t & t) : _t(t) 
         { _t.lock(); }

         ~tunnel_lock_grd_t() { _t.unlock(); }
      }
      grd(tunnel);

      while ( true ) 
      {
         if (tunnel.remove_req_pending()) 
            return 0; 

         struct timeval timeout = {0};
         timeout.tv_usec = 0;
         timeout.tv_sec = 5;

         udp_socket_t::pollst_t pollst = 
            tunnel.tunnel_socket().poll(timeout);

         if (pollst == udp_socket_t::pollst_t::TIMEOUT_EXPIRED) 
            continue;
         else if (pollst == udp_socket_t::pollst_t::ERROR_IN_COMMUNICATION)
            return -1;

         char buf[CDEV_REQUEST_MAX_LENGTH] = {0};
         ip_address_t remote_ip;
         udp_socket_t::port_t remote_port;

         size_t rbytescnt = 
            tunnel.tunnel_socket().recvfrom( buf, sizeof(buf), 
                  remote_ip, remote_port);

         if (rbytescnt > 0) 
         {
            if (! keystr.empty())
            { 
               decrypt_buf_t::handle_t decrypted_buf = 
                  decrypt_buf_t::handle_t ( 
                     new decrypt_buf_t(buf, rbytescnt, keystr.c_str()));

               assert( decrypted_buf != nullptr );

               vnddmgr_ptr->announce_packet(name.c_str(), 
                     decrypted_buf->get_buf(), 
                     decrypted_buf->get_buf_len());   
            }
            else 
            {
               vnddmgr_ptr->announce_packet(name.c_str(), buf, rbytescnt);   
            }

            if ( ! vndd::setup_t::daemon_mode)
               printf("%s announced packet from %s:%i to ndd %s\n",
                     __FUNCTION__, 
                     std::string( remote_ip ).c_str(), 
                     remote_port & 0xffff, name.c_str()
                     );
         }
         else 
         {
            if ( ! vndd::setup_t::daemon_mode)
               fprintf(stderr, 
                     "%s failed receiving packet from "
                     "tunnel for ndd %s\n",
                     __FUNCTION__, name.c_str() 
                     );

            return -1;
         }
      }
   } 
   catch (excp_t) 
   {
      return -1;
   }
   catch (...) 
   {
      assert( 0 );
   }

   return 0;
}


// -----------------------------------------------------------------------------

int vndd::tunnelmgr_t::tunnel_xmit_thread(
      const std::string& keystr, 
      vndd::tunnelmgr_t* tvm_ptr, 
      vndd::mgr_t* vnddmgr_ptr )
{
   assert ( tvm_ptr );
   assert ( vnddmgr_ptr );

   std::string if_name;

   try 
   {
      while (true) 
      {
         char buf[CDEV_REQUEST_MAX_LENGTH] = {0};

         // Get a new packet from vndd manager kernel driver 
         const size_t buflen = 
            vnddmgr_ptr->get_packet(buf, sizeof(buf), if_name);

         if (buflen>0 && buflen <= sizeof(buf)) 
         {
            try 
            {
               // Search for tunnel instance corrisponding to 
               // the interface if_name
               vndd::tunnel_t & tunnel = 
                  tvm_ptr->get_tunnel_instance( if_name ); 

               const ip_address_t remote_ip = tunnel.get_remote_ip();
               const udp_socket_t::port_t remote_port = 
                  tunnel.get_remote_port();

               if ( ! vndd::setup_t::daemon_mode)
                  printf("%s: receiving packet from ndd %s, tx to %s:%i\n",
                        __FUNCTION__, if_name.c_str(), 
                        std::string( remote_ip ).c_str(), remote_port & 0xffff
                        );

               size_t bsent = 0;

               if (! keystr.empty()) 
               {
                  crypt_buf_t::handle_t crypted_buf( 
                     new crypt_buf_t (buf, buflen, keystr.c_str()));

                  // Crypted packet is sent across the tunnel to the remote peer
                  bsent = tunnel.tunnel_socket().
                     sendto(
                        crypted_buf->get_buf(), 
                        crypted_buf->get_buf_len(), 
                        remote_ip, 
                        remote_port );
               } //..if
               else 
               {
                  // Uncrypted packet is sent across the tunnel to the remote peer
                  bsent = tunnel.tunnel_socket().
                     sendto(buf, buflen, remote_ip, remote_port );
               } //..else

               if ( bsent<=0 ) 
               {
                  if ( ! vndd::setup_t::daemon_mode)
                     fprintf(stderr, "%s: tunnel.tunnel_socket().sendto "
                           "error sending sending to %s:%i\n",
                           __FUNCTION__, std::string( remote_ip ).c_str(),
                           remote_port & 0xffff
                           );
                  return -1;
               } //..if
            }
            catch (vndd::tunnelmgr_t::excp_t & e) 
            {
               switch (e) {
                  case vndd::tunnelmgr_t::excp_t::TUNNEL_INSTANCE_NOT_FOUND:
                     break;
                  default:
                     assert(0);
               }
            } //...catch
         }
      } // ... while (1)
   } 
   catch (...) 
   {
      assert( 0 );
   }

   return 0;
}


// -----------------------------------------------------------------------------

bool vndd::tunnelmgr_t::add_tunnel(
      const std::string& ifname, 
      const vndd::tunnel_t::tun_param_t& tp,
      vndd::mgr_t* vnddmgr_ptr,
      const std::string& password) 
{
   assert ( vnddmgr_ptr );
   lock_guard_t with ( _lock );

   vndd::tunnel_t::handle_t htunnel ( new vndd::tunnel_t ( tp ) );

   if( htunnel == nullptr )
      return false; //no enough memory

   const uint32_t ip = tp.remote_ip().to_uint32();
   const uint16_t port = tp.remote_port();

   if (! _rpeer2dev.insert ( 
            std::make_pair( std::make_pair (ip, port), ifname ) ).second ) 
      return false;

   if (! _dev2tunnel.insert ( make_pair(ifname, htunnel) ).second ) 
   {
       // clean up allocated resources
      _rpeer2dev.erase ( std::make_pair (ip, port) );
      return false;
   }

   // There is a single tx thread for all the tunnels
   if ( _tunnel_xmit_thread == nullptr ) 
   {
      _tunnel_xmit_thread.reset ( 
            new std::thread(
               &vndd::tunnelmgr_t::tunnel_xmit_thread,
               password, this, vnddmgr_ptr ));

      if ( _tunnel_xmit_thread == nullptr )
      {
         // clean up allocated resources
         _dev2tunnel.erase( ifname );
         _rpeer2dev.erase ( std::make_pair (ip, port) );
         return false; 
      }
   }

   // Create a receiver thread for this tunnel instance
   std::thread recv_thread(  
            &vndd::tunnelmgr_t::tunnel_recv_thread,
            ifname, this, vnddmgr_ptr , password ); 

   recv_thread.detach();

   return true;
}


// -----------------------------------------------------------------------------

vndd::tunnel_t& vndd::tunnelmgr_t::get_tunnel_instance(
      const std::string& ifname) throw(vndd::tunnelmgr_t::excp_t)
{ 
   lock_guard_t with ( _lock );

   dev2tunnel_map_t :: iterator i = _dev2tunnel.find(ifname);

   if (i == _dev2tunnel.end()) 
      throw excp_t::TUNNEL_INSTANCE_NOT_FOUND;

   return *i->second;
}


// -----------------------------------------------------------------------------

bool vndd::tunnelmgr_t::remove_tunnel(const std::string& ifname) throw() 
{
   lock_guard_t with ( _lock );

   dev2tunnel_map_t :: const_iterator i = _dev2tunnel.find(ifname);

   if (i != _dev2tunnel.end()) 
   {
      const uint32_t ip = i->second->get_remote_ip().to_uint32();
      const uint16_t port = i->second->get_remote_port();

      _rpeer2dev.erase ( std::make_pair( ip, port ) );

      // notify to the receiver thread that 
      // we are going to delete the tunnel instance
      i->second->notify_remove_req_pending();

      auto tunnel_instance = i->second;
      
      // wait until recv thread terminates execution
      tunnel_instance->lock();
      
      //remove the tunnel instance
      _dev2tunnel.erase( ifname );

      tunnel_instance->unlock(); // reset lock counter

      return true;
   }

   return false;
}


// -----------------------------------------------------------------------------


