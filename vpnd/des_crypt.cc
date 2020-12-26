/*
 *  This file is part of TVPN.
 *
 *  See also COPYING, README, README.md
 *
 *  Author: Antonino Calderone, <antonino.calderone@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */ 

#include "des_crypt.h"
#include <cassert>
#include <rpc/des_crypt.h>
#include <memory.h>


/* -------------------------------------------------------------------------- */ 

des_key_t::des_key_t( const std::string& key ) noexcept
{
   memset(&_key[0], 0, _key.size());
   strncpy( &_key[0], key.c_str(), KEY_LEN );
   des_setparity(&_key[0]);
}


/* -------------------------------------------------------------------------- */ 

std::string des_key_t::operator()() const noexcept
{
   std::string ret(&_key[0]);
   return ret;
}


/* -------------------------------------------------------------------------- */ 

const std::string& des_exc_t::get_description() const noexcept
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


/* -------------------------------------------------------------------------- */ 

des_exc_t::error_code_t 
des_exc_t::get_error_code() const noexcept
{
   return _excp_code;
}


/* -------------------------------------------------------------------------- */ 

const des_t& des_t::get_instance() noexcept
{
   if (! _des_instance ) 
      _des_instance = new des_t();

   assert ( _des_instance );

   return *_des_instance;
}


/* -------------------------------------------------------------------------- */ 

void des_t::operator() (
      const method_t& method,
      const crypter_key_t& key,
      block_ptr_t block_ptr, 
      size_t block_size 
   ) 
   const
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


/* -------------------------------------------------------------------------- */ 

des_t* des_t::_des_instance = 0;

