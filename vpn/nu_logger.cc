/*
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


/* -------------------------------------------------------------------------- */

#include "nu_logger.h"
#include "nu_tokenizer.h"


/* -------------------------------------------------------------------------- */

#include <stdio.h> 
#include <stdlib.h>
#include <stdarg.h>

#include <list>
#include <string>
#include <mutex>
#include <string.h>

#include <sstream>
#include <chrono>


/* -------------------------------------------------------------------------- */

static bool timespec2str(char *buf, uint len, struct timespec *ts) 
{
    int ret;
    struct tm t;

    tzset();
    if (localtime_r(&(ts->tv_sec), &t) == NULL)
        return false;

    ret = strftime(buf, len, "%F %T", &t);

    if (ret == 0)
        return false;

    len -= ret - 1;

    ret = snprintf(&buf[strlen(buf)], len, ".%09ld", ts->tv_nsec);

    if (ret >= len)
        return false;

    return 0;
}


/* -------------------------------------------------------------------------- */

static std::string get_local_time()
{
   char timestamp[256] = {0};
   struct timespec t;
   clock_gettime(CLOCK_REALTIME, &t);
   timespec2str(timestamp, sizeof(timestamp), &t);
   return timestamp;
}


/* -------------------------------------------------------------------------- */

static bool rename_file(const char *src, const char *dst)
{ 
   return ::rename( src, dst ) == 0; 
}


/* -------------------------------------------------------------------------- */

static bool remove_file(const char *filename)
{ 
   return ::remove( filename ) == 0; 
}


/* -------------------------------------------------------------------------- */

namespace nu
{

/* -------------------------------------------------------------------------- */

   logger_t * logger_t::_instance = 0;


/* -------------------------------------------------------------------------- */

   typedef nu::tokenizer_t< std::string > nu_tokenizer_t;


/* -------------------------------------------------------------------------- */

   bool logger_t::get_token( 
      nu_tokenizer_t::token_t & token, 
      nu_tokenizer_t & tknzr,   
      const nu_tokenizer_t::token_class_t & skipping_cls)
   {
      do 
      {
         if (! tknzr.get_next_token( token ) ) 
         {
            return false;      
         }

      } 
      while (token.tkncls == skipping_cls);  

      return true;
   }


/* -------------------------------------------------------------------------- */

   bool logger_t::match( const std::string& s, const std::list<std::string>& l) 
   {
      std::list<std::string>::const_iterator i = l.begin();

      for (; i != l.end(); ++i ) 
      {
         if ( s.find(*i) != std::string::npos ) 
            return true;
      }

      return false;
   }


/* -------------------------------------------------------------------------- */

   bool logger_t::extract_expected_token(
      const std::string & expected_token, 
      nu_tokenizer_t & tknzr )
   {
      nu_tokenizer_t::token_t token;

      if (! get_token(token, tknzr)) 
         return false;

      if ( token != expected_token ) 
         return false;

      return true;
   }


/* -------------------------------------------------------------------------- */

   void logger_t::set_log_file( const std::string& log_file )
   { 
      std::lock_guard<std::mutex> lock(_mtx);
      _log_file = log_file; 
   }


/* -------------------------------------------------------------------------- */

   void logger_t::set_bak_file( const std::string& bak_file )
   {
      std::lock_guard<std::mutex> lock(_mtx);
      _log_file = bak_file; 
   }


/* -------------------------------------------------------------------------- */

   void logger_t::set_mode( int mode ) 
   { 
      std::lock_guard<std::mutex> lock(_mtx);
      _logtype = mode; 
   }


/* -------------------------------------------------------------------------- */

   void logger_t::add_include_rule( const std::string& s ) 
   {
      std::lock_guard<std::mutex> lock(_mtx);
      _filter_include_rule.remove( s );
      _filter_include_rule.push_back( s );
   }


/* -------------------------------------------------------------------------- */

   void logger_t::remove_all_include_rules( ) 
   {
      std::lock_guard<std::mutex> lock(_mtx);
      _filter_include_rule.clear();
   }


/* -------------------------------------------------------------------------- */

   void logger_t::add_exclude_rule( const std::string& s ) 
   {
      std::lock_guard<std::mutex> lock(_mtx);
      _filter_include_rule.remove( s );
      _filter_exclude_rule.push_back( s );
   }


/* -------------------------------------------------------------------------- */

   void logger_t::remove_all_exclude_rules( ) 
   {
      std::lock_guard<std::mutex> lock(_mtx);
      _filter_exclude_rule.clear();
   }


/* -------------------------------------------------------------------------- */

   void logger_t::remove_include_rule( const std::string& s ) 
   {
      std::lock_guard<std::mutex> lock(_mtx);
      _filter_include_rule.remove( s );
   }


/* -------------------------------------------------------------------------- */

   void logger_t::remove_exclude_rule( const std::string& s ) 
   {
      std::lock_guard<std::mutex> lock(_mtx);
      _filter_exclude_rule.remove( s );
   }


/* -------------------------------------------------------------------------- */

   bool logger_t::config_logger( nu_tokenizer_t & tknzr )
   {
      nu_tokenizer_t::token_class_set_t blnk_cls;
      nu_tokenizer_t::token_class_set_t sngt_cls;
      nu_tokenizer_t::token_class_set_t linestyle_comment_cls;

      blnk_cls.insert(" ");
      blnk_cls.insert("\t");

      sngt_cls.insert("[");
      sngt_cls.insert("]");
      sngt_cls.insert(",");
      sngt_cls.insert("+");

      sngt_cls.insert("(");
      sngt_cls.insert(")");
      sngt_cls.insert("\"");
      sngt_cls.insert("=");

      linestyle_comment_cls.insert("//");
      linestyle_comment_cls.insert("#");

      tknzr.register_token_atomic( sngt_cls );
      tknzr.register_token_blank( blnk_cls );
      tknzr.register_token_linestyle_comment( linestyle_comment_cls );

      nu_tokenizer_t::token_t token;
      bool end = false;

      _filter_include_rule.clear();
      _filter_exclude_rule.clear();

      while ( ! end ) 
      {
         token.value = "";

         end = ! tknzr.get_next_token( token );

         if ( token.tkncls == nu_tokenizer_t::EMPTY_LINE ||
            token.tkncls == nu_tokenizer_t::LINESTYLE_COMMENT ||
            token.tkncls == nu_tokenizer_t::BLANK ) 
         {
            //skip empty line or line-style comment
            continue;
         }

         tknzr.set_mark();

         if ( token.tkncls == nu_tokenizer_t::END_OF_STREAM ) 
         {
            // no more token
            break;
         }

         if ( token.value == "logfile" ) 
         {
            if ( ! extract_expected_token( "=", tknzr ) )
               return false;

            if ( ! get_token( token, tknzr ) ) 
               return false;

            _log_file = token.value;

            continue;
         }

         if ( token.value == "bakfile" ) 
         {
            if ( ! extract_expected_token( "=", tknzr ) )
               return false;

            if ( ! get_token( token, tknzr ) ) 
               return false;

            _bak_file = token.value;

            continue;
         }

         if ( token.value == "include" ) 
         {
            if ( ! extract_expected_token( "=", tknzr ) )
               return false;

            if ( ! get_token( token, tknzr ) ) 
               return false;

            _filter_include_rule.push_back( token.value );

            continue;
         }

         if ( token.value == "exclude" ) 
         {
            if ( ! extract_expected_token( "=", tknzr ) )
               return false;

            if ( ! get_token( token, tknzr ) ) 
               return false;

            _filter_exclude_rule.push_back( token.value );

            continue;
         }

         if ( token.value == "mode" ) 
         {
            if ( ! extract_expected_token( "=", tknzr ) )
               return false;

            if ( ! get_token( token, tknzr ) )  return false;

            if ( 
               token.value != "0" &&
               token.value != "1" &&
               token.value != "2" &&
               token.value != "3" &&
               token.value != "4" )
               return false;

            _logtype = token.value.c_str()[0] - '0';

            continue;
         }

         if ( token.value == "rbuflen" ) 
         {
            if ( ! extract_expected_token( "=", tknzr ) )
               return false;

            if ( ! get_token( token, tknzr ) ) 
               return false;

            int lines = atoi( token.value.c_str() );

            if ( lines<=0 )
               return false;

            _rbuf_len = lines;

            continue;
         }

      } // while

      return true;
   }


/* -------------------------------------------------------------------------- */

   bool logger_t::load_config_file( const std::string & fname )
   {
      nu::file_stream< std::string> fs( fname );
      nu_tokenizer_t t( fs );

      if ( ! fs.open() ) 
         return false;

      bool res = config_logger( t );
      fs.close();

      return res;
   }


/* -------------------------------------------------------------------------- */

   bool logger_t::configure_logger( const std::string& cfile )
   {
      bool ret = true;

      _mtx.lock();

      if (! load_config_file( cfile.c_str()) ) 
      {
         _logtype = 0;
         ret = false;
      }

      _mtx.unlock();

      if ( _logtype == LOG_FILE ) 
         rename_file(_log_file.c_str(), _bak_file.c_str());

      return ret;
   }


/* -------------------------------------------------------------------------- */

   void logger_t::_swap()
   {
      remove_file( _bak_file.c_str() );
      rename_file( _log_file.c_str(), _bak_file.c_str() );

      FILE * f = fopen(_log_file.c_str(), "wb");

      if (f) 
         fclose(f);
      else 
         remove_file( _log_file.c_str() );
   }


/* -------------------------------------------------------------------------- */

   void logger_t::swap()
   {
      std::lock_guard<std::mutex> lock(_mtx);
      _swap();
   }
   

/* -------------------------------------------------------------------------- */

   int logger_t::vprintf(const char *format, va_list ap)
   {
      char lbuf[ DEF_MAX_LLEN ] = {0};
      int ret = vsnprintf( lbuf, sizeof(lbuf)-1, format, ap );
      print("%s", lbuf);
      return ret;
   }


/* -------------------------------------------------------------------------- */

   void logger_t::print(const char* msg, ...) 
   {
      if ( ! _logtype )
         return;

      std::lock_guard<std::mutex> lock(_mtx);

      va_list marker;
      va_start( marker, msg );     /* Initialize variable arguments. */

      if (_logtype == 1 && _rbuf_idx == 0)
         _swap();

      FILE * f = _logtype == 1 /*FILE*/ ? 
         fopen(_log_file.c_str(), "a+") : stdout ;

      if (f) 
      {
         std::stringstream ss;
         ss << get_local_time() << "   " ; 

         char buf[ DEF_MAX_LLEN ] = { 0 };
         int offset = snprintf(buf, ss.str().size(), "%s", ss.str().c_str());

         vsnprintf(buf + offset - 1, sizeof(buf)-1, msg, marker);

         bool bOutput = true;

         if ( _filter_include_rule.empty() ) 
            bOutput = true;
         else
            bOutput =
               match( std::string( buf ), _filter_include_rule );

         if (! _filter_exclude_rule.empty() ) 
            bOutput = ! match( std::string( buf ), _filter_exclude_rule );

         if ( bOutput ) 
         {
            switch (_logtype) 
            {
               case LOG_DISABLE:
                  break;

               case LOG_STDOUT:
                  fwrite( buf, strlen(buf), 1, f);
                  break;

               case LOG_FILE:
                  fwrite( buf, strlen(buf), 1, f);
                  _rbuf_idx = _rbuf_idx>=(DEF_RBUF_LEN-1) ? 0 : _rbuf_idx + 1;
                  break;

               case LOG_RBUF:
                  _rbuf_add( buf );
                  break;

               case LOG_RBUF_STDOUT:
                  _rbuf_add( buf );
                  fwrite( buf, strlen(buf), 1, f);
                  break;
            }

         }

         if (f != stdout) 
            fclose( f );
      }

      va_end( marker );     /* Initialize variable arguments. */
   }


/* -------------------------------------------------------------------------- */

   void logger_t::printwrn(const char* msg, ...) 
   {
      std::lock_guard<std::mutex> lock(_mtx);

      va_list marker;
      va_start( marker, msg );     /* Initialize variable arguments. */

      if (_logtype == 1 && _rbuf_idx == 0)
         _swap();

      FILE * f = _logtype == 1 /*FILE*/ ? 
         fopen(_log_file.c_str(), "a+") :
      stdout ;

      if (!f) 
         f = stdout;

      std::stringstream ss;
      ss << get_local_time() << " "; 
      char buf[ DEF_MAX_LLEN ] = { 0 };
      int offset = snprintf(buf, ss.str().size(), "%s", ss.str().c_str());

      vsnprintf(buf + offset - 1, sizeof(buf)-1, msg, marker);

      switch (_logtype) 
      {
      case LOG_DISABLE:
         printf("%s", buf);
         break;

      case LOG_FILE:
         fwrite( buf, strlen(buf), 1, f);
         _rbuf_idx = _rbuf_idx>=(DEF_RBUF_LEN-1) ? 0 : _rbuf_idx + 1;
         ::printf("%s",buf);
         break;

      case LOG_STDOUT:     
         fwrite( buf, strlen(buf), 1, f);
         break;

      case LOG_RBUF:
         printf("%s", buf);
         _rbuf_add( buf );
         break;

      case LOG_RBUF_STDOUT:
         _rbuf_add( buf );
         if ( f != stdout )
            printf("%s", buf);
         fwrite( buf, strlen(buf), 1, f);
         break;
      }

      if (f != stdout) 
         fclose( f );

      va_end( marker );     /* Initialize variable arguments. */
   }


/* -------------------------------------------------------------------------- */

   bool logger_t::make_def_cfg_file(const std::string & cfgfile)
   {
      FILE * f = ::fopen(cfgfile.c_str(), "rb");
      if ( f ) {
         ::fclose(f);
         return true;
      }

      f = ::fopen(cfgfile.c_str(), "w");

      if (! f) {
         return false;
      }

      ::fprintf(f, "// nulogger.cfg 1.0 //\n\n");    
      ::fprintf(f, "    logfile  = nulogger.log     // logging file    \n");
      ::fprintf(f, "    bakfile  = nulogger.log.old // old logging file\n");
      ::fprintf(f, " \n");
      ::fprintf(f, " // include  = <matching keyword 1> \n");
      ::fprintf(f, " // include  = <matching keyword 2> \n");
      ::fprintf(f, " // ... \n");
      ::fprintf(f, " // exclude  = <matching keyword 3> \n");
      ::fprintf(f, " // exclude  = <matching keyword 4> \n");
      ::fprintf(f, " // ... \n");
      ::fprintf(f, " \n");
      ::fprintf(f, "  //Mode: 0-dis, 1-file, 2-stdout, 3-rbuf, 4-rbuf & stdout \n");
      ::fprintf(f, "  mode = 3\n");
      ::fprintf(f, "  \n");
      ::fprintf(f, "  rbuflen = 700 //ring bufferlength (in lines)\n");
      ::fprintf(f, " \n");

      ::fclose(f);

      return true;
   }


/* -------------------------------------------------------------------------- */

   bool logger_t::save_cfg_file(const std::string & cfgfile)
   {
      FILE * f = ::fopen(cfgfile.c_str(), "w");

      if (! f) 
         return false;

      ::fprintf(f, "    logfile  = %s\n", _log_file.c_str());
      ::fprintf(f, "    bakfile  = %s\n", _bak_file.c_str());
      ::fprintf(f, " \n");

      if ( !_filter_include_rule.empty() ) 
      {
         for ( 
             std::list<std::string>::const_iterator 
               i = _filter_include_rule.begin();
               i != _filter_include_rule.end();
               ++ i 
         )
         ::fprintf(f,"    include = %s\n", i->c_str()); 
      }

      if ( !_filter_exclude_rule.empty() ) 
      {
         for ( 
            std::list<std::string>::const_iterator 
               i = _filter_exclude_rule.begin(); 
               i != _filter_exclude_rule.end();
               ++ i 
         )
         ::fprintf(f,"    exclude = %s\n", i->c_str()); 
      }

      ::fprintf(f, "\n    mode = %i // (%s)\n\n", _logtype, _mode_desc());
      ::fprintf(f, "\n    rbuflen = %i\n\n", _rbuf_len);

      ::fclose(f);

      return true;
   }


/* -------------------------------------------------------------------------- */

   void logger_t::show_rbuf ( 
      int maxline, 
      FILE * f, 
      const std::string& include,
      const std::string& exclude
   )
   {
      _rbuf_t tmp;

      _mtx.lock();

      int n = 0;

      for ( _rbuf_t :: const_iterator i = _rbuf.begin(); 
         (n<maxline || !maxline) && i !=  _rbuf.end(); 
         ++i, ++n )
      {
         tmp.push_back( *i );
      }

      _mtx.unlock();

      for ( _rbuf_t :: const_iterator i = tmp.begin(); i !=  tmp.end(); ++i )
      {
         bool showout = include.empty() || i->find(include) != std::string::npos;

         if ( !exclude.empty() && i->find(exclude) != std::string::npos ) 
            showout = false;

         if (showout) 
            fprintf(f, "%s", i->c_str());
      }
   }


/* -------------------------------------------------------------------------- */

   bool logger_t::save_rbuf( const std::string& filename, int maxline  )
   {
      _mtx.lock();

      FILE * f = fopen(filename.empty() ? 
         _log_file.c_str() : filename.c_str() , "w");

      _mtx.unlock();

      if (f) 
      {
         show_rbuf( maxline, f );
         ::fclose(f);
         return true;
      }

      return false;
   }


/* -------------------------------------------------------------------------- */

} 
   


