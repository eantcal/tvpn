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
 *  Author:	Antonino Calderone, <acaldmail@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */

#ifndef __VNDD_SETUP_H__
#define __VNDD_SETUP_H__


/* -------------------------------------------------------------------------- */

#include <string>


/* -------------------------------------------------------------------------- */

namespace vndd
{
   struct setup_t 
   {
      static bool daemon_mode;
      static const std::string lockfilename;
   };
}


/* -------------------------------------------------------------------------- */

#endif // __VNDD_SETUP_H__

