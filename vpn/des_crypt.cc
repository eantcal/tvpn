/*
 *  des_crypt.cc
 *
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


// -----------------------------------------------------------------------------

#include "des_crypt.h"
#include <cassert>
#include <rpc/des_crypt.h>
#include <memory.h>


// -----------------------------------------------------------------------------

des_key_t::des_key_t( const std::string& key ) throw() 
{
   memset(&_key[0], 0, _key.size());
   strncpy( &_key[0], key.c_str(), KEY_LEN );
   des_setparity(&_key[0]);
}


// -----------------------------------------------------------------------------

std::string des_key_t::operator()() const throw() 
{
   std::string ret(&_key[0]);
   return ret;
}


// -----------------------------------------------------------------------------

const std::string& des_exc_t::get_description() const throw () 
{
   static const std::string _desc[] =
   {
      "Encryption succeeded, but done in software instead" //0
      "of the requested hardware.",

      "An error occurred in the hardware or driver.",      //1

      "Bad parameter to routine.",                         //2

      "Unknown error."                                     //3
   };

   switch (_excp_code) 
   {
      case DESERR_NOHWDEVICE: return _desc[0];
      case DESERR_HWERROR:    return _desc[1];
      case DESERR_BADPARAM:   return _desc[2];
      default:
         break;
   }
   return _desc[3];
}


// -----------------------------------------------------------------------------

des_exc_t::error_code_t 
des_exc_t::get_error_code() const throw () 
{
   return _excp_code;
}


// -----------------------------------------------------------------------------

const des_t& des_t::get_instance() throw() 
{
   if (! _des_instance ) 
      _des_instance = new des_t();

   assert ( _des_instance );

   return *_des_instance;
}


// -----------------------------------------------------------------------------

void des_t::operator() (
      const method_t& method,
      const crypter_key_t& key,
      block_ptr_t block_ptr, 
      size_t block_size 
   ) 
   const throw (crypter_exc_t) 
{
   assert( block_ptr );

   assert( 
         method == method_t::ENCRYPT_BLOCK || 
         method == method_t::DECRYPT_BLOCK );

   int err_code = ecb_crypt(static_cast<char*>(key),
         block_ptr,
         block_size,
         method == method_t::ENCRYPT_BLOCK ? 
         DES_ENCRYPT : 
         DES_DECRYPT );

   if ( err_code != DESERR_NONE ) {
      throw des_exc_t ( err_code );
   }
}


// -----------------------------------------------------------------------------

des_t* des_t::_des_instance = 0;

