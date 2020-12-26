/*
 *  This file is part of TVPN.
 *
 *  See also COPYING, README, README.md
 *
 *  Author: Antonino Calderone, <antonino.calderone@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */

#ifndef __CRYPTER_H__
#define __CRYPTER_H__


/* -------------------------------------------------------------------------- */

#include <string>


/* -------------------------------------------------------------------------- */

class crypter_key_t 
{
   public:
      virtual std::string operator()() const noexcept = 0;
      virtual ~crypter_key_t() noexcept {}

      virtual explicit operator char*() const noexcept = 0;  
      virtual explicit operator const char*() const noexcept = 0;  
};


/* -------------------------------------------------------------------------- */

class crypter_exc_t 
{ 
   public:
      using error_code_t = int;

      virtual const std::string& get_description() const noexcept = 0;
      virtual error_code_t get_error_code() const noexcept = 0;
      virtual ~crypter_exc_t() noexcept {}
};


/* -------------------------------------------------------------------------- */

class crypter_t 
{ 
   public:
      using block_ptr_t = char*;

      enum class method_t { ENCRYPT_BLOCK, DECRYPT_BLOCK };

      virtual void operator()( 
            const method_t& method,
            const crypter_key_t& key,
            block_ptr_t block_ptr, 
            size_t block_size ) const  = 0;

      virtual ~crypter_t() noexcept {}
};


/* -------------------------------------------------------------------------- */

#endif // ... __CRYPTER_H__

