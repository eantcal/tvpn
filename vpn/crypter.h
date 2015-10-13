/*
 *  crypter.h
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

#ifndef __CRYPTER_H__
#define __CRYPTER_H__

// ----------------------------------------------------------------------------

#include <string>


// ----------------------------------------------------------------------------

class crypter_key_t 
{
   public:
      virtual std::string operator()() const throw() = 0;
      virtual ~crypter_key_t() throw() {}

      virtual explicit operator char*() const throw() = 0;  
      virtual explicit operator const char*() const throw() = 0;  
};


// ----------------------------------------------------------------------------

class crypter_exc_t 
{ 
   public:
      using error_code_t = int;

      virtual const std::string& get_description() const throw () = 0;
      virtual error_code_t get_error_code() const throw () = 0;
      virtual ~crypter_exc_t() throw() {}
};


// ----------------------------------------------------------------------------

class crypter_t 
{ 
   public:
      using block_ptr_t = char*;

      enum class method_t { ENCRYPT_BLOCK, DECRYPT_BLOCK };

      virtual void operator()( 
            const method_t& method,
            const crypter_key_t& key,
            block_ptr_t block_ptr, 
            size_t block_size ) const throw (crypter_exc_t) = 0;

      virtual ~crypter_t() throw() {}
};


// ----------------------------------------------------------------------------

#endif // ... __CRYPTER_H__

