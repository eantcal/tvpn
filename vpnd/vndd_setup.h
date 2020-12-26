/*
 *  This file is part of TVPN.
 *
 *  See also COPYING, README, README.md
 *
 *  Author: Antonino Calderone, <antonino.calderone@gmail.com>
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

