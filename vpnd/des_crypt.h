/*
 *  This file is part of TVPN.
 *
 *  See also COPYING, README, README.md
 *
 *  Author: Antonino Calderone, <antonino.calderone@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */

#ifndef __DESCRYPTER_H__
#define __DESCRYPTER_H__


/* -------------------------------------------------------------------------- */

#include "crypter.h"
#include <array>


/* -------------------------------------------------------------------------- */

class des_key_t : public crypter_key_t 
{
  public:
    enum { KEY_LEN = 8 };

  private:
    std::array<char, KEY_LEN+1> _key;

  public:
    des_key_t( const std::string& key ) noexcept;
    virtual std::string operator()() const noexcept;
    virtual ~des_key_t() noexcept {}

    virtual explicit operator char*() const noexcept 
    { return const_cast<char*>( &_key[0]) ; }

    virtual explicit operator const char*() const noexcept 
    { return &_key[0]; }
};


/* -------------------------------------------------------------------------- */

class des_exc_t : public crypter_exc_t 
{
  private:
    int _excp_code;

  public:
    inline des_exc_t(int excp_code) : _excp_code(excp_code) { }

    virtual const std::string& get_description() const throw ();
    virtual error_code_t get_error_code() const throw ();

    virtual ~des_exc_t() noexcept {};
};


/* -------------------------------------------------------------------------- */

class des_t : public crypter_t 
{
  private:
    static des_t* _des_instance;

    des_t() {}
    virtual ~des_t() noexcept {};
  
  public:
    static const des_t& get_instance() noexcept;

    virtual void
    operator()(const method_t& method,
               const crypter_key_t& key,
               block_ptr_t block_ptr, 
               size_t block_size ) const ;
};


/* -------------------------------------------------------------------------- */

#endif // __DESCRYPTER_H__

