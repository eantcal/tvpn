/*
 *  des_crypt.h
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

#ifndef __DESCRYPTER_H__
#define __DESCRYPTER_H__


// -----------------------------------------------------------------------------

#include "crypter.h"
#include <array>


// -----------------------------------------------------------------------------

class des_key_t : public crypter_key_t 
{
  public:
    enum { KEY_LEN = 8 };

  private:
    std::array<char, KEY_LEN+1> _key;

  public:
    des_key_t( const std::string& key ) throw();
    virtual std::string operator()() const throw();
    virtual ~des_key_t() throw() {}

    virtual explicit operator char*() const throw() 
    { return const_cast<char*>( &_key[0]) ; }

    virtual explicit operator const char*() const throw() 
    { return &_key[0]; }
};


// -----------------------------------------------------------------------------

class des_exc_t : public crypter_exc_t 
{
  private:
    int _excp_code;

  public:
    inline des_exc_t(int excp_code) : _excp_code(excp_code) { }

    virtual const std::string& get_description() const throw ();
    virtual error_code_t get_error_code() const throw ();

    virtual ~des_exc_t() throw() {};
};


// -----------------------------------------------------------------------------

class des_t : public crypter_t 
{
  private:
    static des_t* _des_instance;

    des_t() {}
    virtual ~des_t() throw() {};
  
  public:
    static const des_t& get_instance() throw();

    virtual void
    operator()(const method_t& method,
               const crypter_key_t& key,
               block_ptr_t block_ptr, 
               size_t block_size ) const throw (crypter_exc_t);
};


// -----------------------------------------------------------------------------

#endif // __DESCRYPTER_H__

