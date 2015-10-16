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
 *  Author: Antonino Calderone, <acaldmail@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */

#ifndef __NU_LOGGER_H__
#define __NU_LOGGER_H__


/* -------------------------------------------------------------------------- */

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include "nu_tokenizer.h"

#include <string>
#include <stdarg.h>

#include <mutex>


/* -------------------------------------------------------------------------- */

#ifdef USE_NU_LOGGER_DEFAULT_CONFIGURATION
#define NU_LOGGER_CFG "/var/log/nulogger.cfg"
#define NU_LOGGER_LOG "/var/log/nulogger.log"
#define NU_LOGGER_BAK "/var/log/nulogger.log.bak"
#endif


/* -------------------------------------------------------------------------- */

namespace nu
{

/* -------------------------------------------------------------------------- */

typedef nu::tokenizer_t< std::string > nu_tokenizer_t;


/* -------------------------------------------------------------------------- */

class logger_t 
{
   private:
      static logger_t * _instance;

   public:

      static inline 
         logger_t & get_instance() 
         {
            if ( ! _instance ) 
               _instance = new logger_t();

            return * _instance;
         }

      enum 
      {
         LOG_DISABLE, 
         LOG_FILE, 
         LOG_STDOUT,
         LOG_RBUF,
         LOG_RBUF_STDOUT
      };

   private:      
      mutable std::mutex _mtx;

      std::list<std::string> _filter_include_rule;
      std::list<std::string> _filter_exclude_rule;

      typedef std::list<std::string> _rbuf_t;
      _rbuf_t _rbuf;

      std::string _log_file;
      std::string _bak_file;

      int _logtype;
      int _rbuf_len;
      int _rbuf_idx;

      enum { DEF_RBUF_LEN = 1000 };
      enum { DEF_MAX_LLEN = 256 };

      logger_t() noexcept
         : 
         _log_file(NU_LOGGER_LOG),
         _bak_file(NU_LOGGER_BAK),
         _logtype(LOG_FILE),
         _rbuf_len(DEF_RBUF_LEN),
         _rbuf_idx(0) 
         {
         }


      logger_t(const logger_t&);


      void _set_rbuf_len( int lines ) 
      {
         _rbuf_len = lines;
      }

      void _rbuf_add( const std::string& line )
      {
         if (int(_rbuf.size()) >= _rbuf_len ) 
            _rbuf.erase( _rbuf.begin() );

         _rbuf.push_back( line );
      }


      const char* _mode_desc() const noexcept
      {
         return 
            _logtype == LOG_DISABLE ? "disabled" :
            (_logtype == LOG_FILE ? "log on file" : 
             (_logtype == LOG_STDOUT ? "log on stdout" :
              (_logtype == LOG_RBUF ? "log on ring buffer" :
               (_logtype == LOG_RBUF_STDOUT ? "log on both stdout and ring buf" :
                "?"))));
      }


      static bool get_token( nu_tokenizer_t::token_t & token, nu_tokenizer_t& tknzr, 
            const nu_tokenizer_t::token_class_t & skipping_cls = nu_tokenizer_t::BLANK); 

      static bool match( const std::string& s, const std::list<std::string>& l);
      static bool extract_expected_token( const std::string & expected_token, 
            nu_tokenizer_t& tknzr );

      bool config_logger( nu_tokenizer_t & tknzr );
      bool load_config_file( const std::string & filename );
      void _swap();

   public:
      static bool make_def_cfg_file( const std::string & cfgfile = NU_LOGGER_CFG );
      bool configure_logger( const std::string& cfg_file_name = NU_LOGGER_CFG );
      int vprintf(const char *format, va_list ap);
      void print(const char* msg, ...);
      void printwrn(const char* msg, ...);
      void set_log_file( const std::string& log_file );
      void set_bak_file( const std::string& bak_file );
      void set_mode( int mode );

      int get_mode() const
      { 
         std::lock_guard<std::mutex> lock(_mtx);
         return _logtype;  
      }

      void add_include_rule( const std::string& s );
      void add_exclude_rule( const std::string& s );

      void remove_all_exclude_rules( );
      void remove_all_include_rules( );

      void remove_include_rule( const std::string& s );
      void remove_exclude_rule( const std::string& s );

      bool save_cfg_file(const std::string & cfgfile = NU_LOGGER_CFG);

      inline void set_rbuf_len( int maxl )
      { 
         std::lock_guard<std::mutex> lock(_mtx);

         _rbuf_len = maxl > 0 ? maxl : DEF_RBUF_LEN;
      };

      void monitor_help(const char* arg1 = 0) const;

      void swap();

      void show_rbuf( 
         int maxline = 0, 
         FILE * f = stdout,
         const std::string& include = "",
         const std::string& exclude = "");

      bool save_rbuf( 
         const std::string& filename = "", /*unspecified means use default */
         int maxline = DEF_RBUF_LEN  );
};


/* -------------------------------------------------------------------------- */

} // nu


/* -------------------------------------------------------------------------- */

#define NU_DBGOUT nu::logger_t::get_instance().print
#define NU_DBGWRN nu::logger_t::get_instance().printwrn
#define NU_DBGERR nu::logger_t::get_instance().printwrn


/* -------------------------------------------------------------------------- */

#endif // __NU_LOGGER_H__

