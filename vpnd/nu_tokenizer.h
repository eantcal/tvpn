/*
 *  This file is part of TVPN.
 *
 *  See also COPYING, README, README.md
 *
 *  Author: Antonino Calderone, <antonino.calderone@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */

#ifndef __NU_TOKENIZER_H__  
#define __NU_TOKENIZER_H__  


/* -------------------------------------------------------------------------- */

#if HAVE_CONFIG_H
# include <config.h>
#endif


#include <cassert>
#include <set>
#include <map>
#include <list>
#include <algorithm>
#include <stdio.h>


/* -------------------------------------------------------------------------- */

#ifndef NU_TKNZR_MAX_LINE_LEN 
#define NU_TKNZR_MAX_LINE_LEN 4096
#endif


/* -------------------------------------------------------------------------- */

namespace nu 
{


/* -------------------------------------------------------------------------- */

template <class T=std::string> class base_stream 
{
   public:
      virtual bool eof() const noexcept = 0;
      virtual bool get_line(T & line, char delimiter) noexcept = 0;
      virtual ~base_stream() noexcept {}
};


/* -------------------------------------------------------------------------- */

template <class T=std::string> class string_stream : public nu::base_stream<T> 
{
   private:
      T _line;
      bool _eof;
      int _seekptr;

   public:
      enum { MAX_LINE_LEN = NU_TKNZR_MAX_LINE_LEN };

      string_stream( const T & text ) noexcept : 
         _line(text), 
         _eof(false),
         _seekptr(0) 
         {}

      bool eof() const noexcept override
      { 
         return _eof; 
      }

      bool getline( char * line_buf, int max_len, char delimiter ) 
      {
         char c = 0;

         if (_seekptr>=_line.size())
         {
            _eof = true;
            return false;
         }

         while ( _seekptr < max_len ) 
         {
            c = _line[_seekptr];

            if ( c == delimiter ) 
            {
               ++_seekptr;
               break;
            }

            line_buf[_seekptr++] = c;

            if (_seekptr==_line.size())
               break;
         }

         return true;
      }

      virtual bool get_line(T & line, char delimiter) noexcept
      { 
         char lb[ MAX_LINE_LEN ] = { 0 };

         bool ret = getline(lb, sizeof(lb)-1, delimiter );
         line = lb;

         return ret;
      }

      virtual ~string_stream() noexcept
      {}
};


/* -------------------------------------------------------------------------- */

template <class T = std::string> class file_stream : public base_stream<T> 
{
   private:
      std::string _filename;
      FILE* _fstrm;

   public:
      enum { MAX_LINE_LEN = NU_TKNZR_MAX_LINE_LEN };

      file_stream( const std::string & filename ) noexcept : 
         _filename(filename),
         _fstrm(0)
         { }

      bool open() noexcept 
      {
         _fstrm = fopen( _filename.c_str(), "r" );

         return _fstrm ? true : false;
      }

      bool is_open() const noexcept
      { 
         return _fstrm != 0; 
      }

      bool close() noexcept
      {
         if (_fstrm) 
         {
            bool ok = 0 == fclose(_fstrm);
            _fstrm = 0;

            return ok;
         }

         return true;
      }

      virtual bool eof() const noexcept
      {
         return feof(_fstrm) != 0;
      }

      bool getline( char * line_buf, int max_len, char delimiter ) 
      {
         int i = 0;
         char c = 0;

         while ( i < max_len ) 
         {
            if (fread( &c, 1, 1, _fstrm ) != 1) 
            {
               return false;
               break;
            }

            if ( c == delimiter ) 
               break;

            line_buf[i] = c;

            i++;
         }

         return true;
      }

      virtual bool get_line(T & line, char delimiter) noexcept
      { 
         char lb[ MAX_LINE_LEN ] = { 0 };

         if ( getline( lb, sizeof(lb)-1, delimiter ) ) 
         {
            line = lb;
            return true;
         }

         return false;
      }

      virtual ~file_stream() noexcept
      {
         close();
      }
};    


/* -------------------------------------------------------------------------- */

template <class T=std::string> class tokenizer_t 
{
   public:
      enum token_class_t {
         BLANK            = 0, 
         ATOMIC,
         EMPTY_LINE,
         OTHER,
         END_OF_STREAM,
         LINESTYLE_COMMENT,

         TOKEN_CLASS_CNT
      };

      using token_class_set_t = std::set< T >;

   private:
      base_stream<T> & _strm;

      int _current_line;
      int _current_col;

      T _current_line_buf;

      char _line_delimiter;

      token_class_set_t _token_class[ TOKEN_CLASS_CNT ];

   public:
      struct token_t 
      {
         token_class_t tkncls;
         T value;
         size_t col;
         size_t line;

         token_t() noexcept : 
            tkncls(OTHER), col(0), line(0)
            {}

         token_t(const token_t& obj) noexcept
         {
            tkncls = obj.tkncls;
            value = obj.value;
            col = obj.col;
            line = obj.line;
         }

         token_t & operator = (const token_t& obj) noexcept
         {
            if ( this != &obj ) {
               tkncls = obj.tkncls;
               value = obj.value;
               col = obj.col;
               line = obj.line;
            }
            return *this;
         }

         bool operator == (const token_t& obj) const noexcept
         { 
            return (this == &obj) || 
               (tkncls == obj.tkncls && value == obj.value);
         }

         bool operator == (const T& str) const noexcept
         { 
            return value == str; 
         }

         bool operator != (const T& str) const noexcept
         { 
            return value != str; 
         }
      };

   private:
      token_t _last_processed_token;

      using _rtoken_list_t = std::list< token_t >;

      bool _rtoken_enable;
      _rtoken_list_t _rtoken_list;
      typename _rtoken_list_t::const_iterator _rtoken_list_it;

      void _register_token_class( token_class_t cl, 
            token_class_set_t clset ) noexcept
      {
         assert( cl < TOKEN_CLASS_CNT && cl >= BLANK) ;
         _token_class [ cl ] = clset;
      }

   public:
      tokenizer_t( base_stream<T> & strm ) : 
         _strm(strm),
         _current_line( 0 ),
         _current_col( 0 ),
         _line_delimiter('\n'),
         _rtoken_enable(false),
         _rtoken_list(),
         _rtoken_list_it (_rtoken_list.begin())
      {}    

      T get_current_line_buf() const noexcept
      { return _current_line_buf; }

      void set_mark() noexcept
      { 
         _rtoken_enable = true;
         _rtoken_list.clear();
      }

      void remove_mark() noexcept
      { 
         _rtoken_enable = false;
         _rtoken_list.clear();
      }

      void rewind_to_mark() noexcept
      { 
         _rtoken_list_it = _rtoken_list.begin(); 
      }

      void register_token_blank( token_class_set_t clset ) noexcept
      {
         _register_token_class( BLANK, clset );
      }

      void register_token_atomic( token_class_set_t clset ) noexcept
      {  
         _register_token_class( ATOMIC, clset ); 
      }

      void register_token_linestyle_comment( token_class_set_t clset ) noexcept
      { 
         _register_token_class( LINESTYLE_COMMENT, clset ); 
      }

      int get_line_num() const noexcept
      { 
         return _current_line; 
      }

      int get_col_num() const noexcept
      { 
         return _current_col; 
      }

      bool eof() const noexcept
      { 
         return _strm.eof(); 
      }

      void set_line_delimiter( char delimiter ) noexcept
      { 
         _line_delimiter = delimiter; 
      }

      char get_line_delimiter() const noexcept
      { 
         return _line_delimiter; 
      }

      static size_t search_token_pos(
            const T& token, 
            const T& line, 
            const size_t pos = 0 ) noexcept
      { 
         return line.find( token, pos ); 
      }

      static size_t search_token_class_pos( 
            T & token_found,
            const token_class_set_t& tknset, 
            const T & line,
            const size_t pos = 0 )
      {
         int min_pos = unsigned(-1) >> 1;
         bool found = false;

         for ( typename token_class_set_t :: const_iterator i = 
               tknset.begin();
               i != tknset.end();
               ++i )
         {
            int ret_pos = -1;

            if ( (ret_pos = (int) 
                     search_token_pos( *i, line, pos )) != -1 ) 
            {
               if ( ret_pos <= min_pos ) {
                  min_pos = ret_pos;
                  token_found = *i;
                  found = true;
               }
            }
         }

         return found ? min_pos : size_t(-1);
      }

   private:
      void _record_token( const token_t & t ) 
      {
         if ( _rtoken_enable ) 
            _rtoken_list.push_back( t );
      }

      bool _replay_recorded_token( token_t & t ) 
      {
         if ( _rtoken_list_it != _rtoken_list.end()) 
         {

            t = *_rtoken_list_it;
            ++ _rtoken_list_it;

            return true;
         }

         return false;
      }

   public:
      bool get_next_token( token_t & t ) noexcept
      {
         if ( _replay_recorded_token( t ) ) 
            return true;

         t.tkncls = OTHER;

         //Read the text buffer one line at a time
         if ( ! _current_col || 
               _current_col == int(_current_line_buf.size()) ) 
         {

            if (! _strm.get_line( _current_line_buf, _line_delimiter ) ) 
            {
               t.tkncls = END_OF_STREAM;
               return false;
            }
            else 
            {
               ++ _current_line;
               _current_col = 0;
            }

            if ( eof() )
            {
               t.tkncls = END_OF_STREAM;
               return false;
            }

         }

         T token;
         int atomic_token_pos = -1;
         int blank_pos = -1;
         int linestyle_comment = -1;

         do {
            atomic_token_pos = search_token_class_pos( 
                  token, 
                  _token_class[ ATOMIC ], 
                  _current_line_buf,
                  _current_col );

            if ( atomic_token_pos == _current_col ) 
            {
               t.tkncls = ATOMIC;
               break;
            }


            // search blank pos 
            blank_pos = search_token_class_pos( token, 
                  _token_class[ BLANK ], 
                  _current_line_buf,
                  _current_col );

            if ( blank_pos == _current_col ) 
            {
               t.tkncls = BLANK;
               break;
            } 

            linestyle_comment = search_token_class_pos( 
                  token, 
                  _token_class[ LINESTYLE_COMMENT ], 
                  _current_line_buf,
                  _current_col );

            if ( linestyle_comment == _current_col ) 
            {
               t.tkncls = LINESTYLE_COMMENT;

               //left part of _current_line_buf is a token
               token = _current_line_buf.substr( 
                     _current_col, 
                     _current_line_buf.size() - _current_col );

               break;
            }

            int tpos = blank_pos > -1  && 
               blank_pos < atomic_token_pos ? blank_pos : 
               ( atomic_token_pos > -1 ? atomic_token_pos : blank_pos );


            if ( tpos >= _current_col ) 
            {
               t.tkncls = OTHER;

               //get token before blank
               token = _current_line_buf.substr( 
                     _current_col, tpos - _current_col );
            }
            else 
            {
               //left part of _current_line_buf is a token
               token = _current_line_buf.substr( 
                     _current_col, 
                     _current_line_buf.size() - _current_col );

               break;
            }
         }
         while (0);

         if ( token.empty() && _current_col == 0) 
            t.tkncls = EMPTY_LINE;

         t.value = token;
         t.col = _current_col;
         t.line = _current_line;

         _last_processed_token = t;

         _current_col += token.size();

         _record_token( t );

         return ! token.empty() || t.tkncls == EMPTY_LINE;
      }


      token_t get_last_token_processed() const noexcept
      { 
         return _last_processed_token; 
      }
};


/* -------------------------------------------------------------------------- */

} // namespace nu

#endif //__NU_TOKENIZER_H__  

