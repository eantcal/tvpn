/*
 *  This file is part of TVPN
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
 *  Author: <antonino.calderone@ericsson.com>, <acaldmail@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */

#ifndef __NU_CONSOLE_H__
#define __NU_CONSOLE_H__


/* -------------------------------------------------------------------------- */

#include "nu_terminal.h"

#include <stdlib.h>
#include <termios.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio_ext.h>


/* -------------------------------------------------------------------------- */

namespace nu
{


/* -------------------------------------------------------------------------- */

class console_t
{
  private:
     class save_tty_settings_t
     {
        private:
           int _old_flags = 0;
           struct termios _tty_set;

        public:
           save_tty_settings_t() noexcept
           {
              _old_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
              tcgetattr(STDIN_FILENO, &_tty_set); // grab old terminal I/O settings
              _tty_set.c_lflag &= ~ICANON;
              tcsetattr(STDIN_FILENO, TCSANOW, &_tty_set);
           }

           ~save_tty_settings_t() noexcept
           {
              fcntl(STDIN_FILENO, F_SETFL, _old_flags);
           }
     };

     console_t(const console_t &) = delete;
     console_t(console_t &&) = delete;
     console_t operator=(const console_t &) = delete;
     console_t operator=(console_t &&) = delete;

   public:
      console_t(bool echo_mode = true) noexcept 
         : _echo_mode(echo_mode) 
      { 
         _init_tty(); 
      }

      std::string input_line() const noexcept;
      std::string input_ch( size_t count = 1 ) const noexcept;
      
      void cls() const noexcept;
      void locate( int y, int x ) const noexcept;
      void set_cursor_visible( bool on ) const noexcept;
 
      int keybhit() const noexcept;

   private:
      save_tty_settings_t _tty_settings;
      bool _echo_mode;

      void _init_tty() const noexcept;
};


/* -------------------------------------------------------------------------- */

}


/* -------------------------------------------------------------------------- */

#endif // __NU_CONSOLE_H__
