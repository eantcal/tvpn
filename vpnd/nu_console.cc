/*
 *  This file is part of TVPN.
 *
 *  See also COPYING, README, README.md
 *
 *  Author: Antonino Calderone, <antonino.calderone@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */

#include "nu_console.h"


/* -------------------------------------------------------------------------- */

namespace nu
{


/* -------------------------------------------------------------------------- */

void console_t::_init_tty() const noexcept
{
   struct termios tty_set;

   tcgetattr(STDIN_FILENO, &tty_set); // grab old terminal I/O settings

   tty_set.c_cc[VMIN] = 1;
   tty_set.c_cc[VTIME] = 0;

   if (_echo_mode)
   {
      tty_set.c_lflag |= ECHO|ICANON;
   }
   else
   {
      tty_set.c_lflag &= ~ECHO;
      tty_set.c_lflag &= ~ICANON;
   }

   tcsetattr(STDIN_FILENO, TCSANOW, &tty_set);

   tty_set.c_cc[VMIN] = 1;
   tty_set.c_cc[VTIME] = 0;
   tcsetattr (STDIN_FILENO, TCSAFLUSH, &tty_set);
}


/* -------------------------------------------------------------------------- */

void console_t::cls() const noexcept
{
   int res = system("clear");
   (void) res;
}


/* -------------------------------------------------------------------------- */

void console_t::locate(int y, int x) const noexcept
{
   printf("%c[%d;%df", 0x1B, y, x);
}


/* -------------------------------------------------------------------------- */

void console_t::set_cursor_visible( bool on ) const noexcept
{
   printf("%c[?25%c", 0x1B, !on ? 'l' : 'h');
}


/* -------------------------------------------------------------------------- */

std::string console_t::input_ch(size_t count) const noexcept
{
   std::string ret;
   std::string line;

   while (count > 0)
   {
      terminal_input_t ti( true );
      terminal_t terminal( ti );
      terminal.set_max_line_length( count );
      terminal.register_eol_ch( nu::terminal_input_t::CR );
      terminal.register_eol_ch( nu::terminal_input_t::LF );
      terminal.set_flags( terminal_t::ECHO_DIS );
      terminal.get_line( line, true /* return if len(line) == count */);

      if (line.empty())
         line="\n";

      ret += line;
      count -= line.size();
      line.clear();
   }

   ret += line;

   return ret;
}


/* -------------------------------------------------------------------------- */

std::string console_t::input_line() const noexcept
{
   std::string line;

   struct _term
   {
      terminal_input_t ti;
      terminal_t terminal;

      _term() : ti(true), terminal(ti)
      {
         terminal.register_eol_ch( nu::terminal_input_t::CR );
         terminal.register_eol_ch( nu::terminal_input_t::LF );
         terminal.set_insert_enabled( true );
      }
   };

   static _term term;

   term.terminal.get_line( line );

   if (_echo_mode)
   {
      printf("\n");
      fflush(stdout);
   }

   return line;
}


/* -------------------------------------------------------------------------- */

int console_t :: keybhit() const noexcept
{
   terminal_input_t ti( true );
   int c = 0;

   while (ti.keybhit())
      c = fgetc(stdin);

   return c;
}

/* -------------------------------------------------------------------------- */


} // namespace nu
