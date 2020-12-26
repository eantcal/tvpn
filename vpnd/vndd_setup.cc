/*
 *  This file is part of TVPN.
 *
 *  See also COPYING, README, README.md
 *
 *  Author: Antonino Calderone, <antonino.calderone@gmail.com>
 *
 */


/* -------------------------------------------------------------------------- */

#include "vndd_setup.h"


/* -------------------------------------------------------------------------- */

#ifndef VNDD_LOCK_FILENAME
   #define VNDD_LOCK_FILENAME "vnddvpnd.pid"
#endif // VNDD_DAEMON_PID_FILE


/* -------------------------------------------------------------------------- */

bool vndd::setup_t::daemon_mode = false;
const std::string vndd::setup_t::lockfilename ( VNDD_LOCK_FILENAME );


/* -------------------------------------------------------------------------- */


