#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([tvpn], [1.4], [acaldmail@gmail.com])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

AC_DEFINE([NU_LOGGER_CFG], ["/var/log/tvpnlogger.cfg"], [Logger config file])
AC_DEFINE([NU_LOGGER_LOG], ["/var/log/tvpn.log"],       [Logger dest file])
AC_DEFINE([NU_LOGGER_BAK], ["/var/log/tvpn.log.bak"],   [Logger back dest file])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h memory.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([memset select socket])

AC_CONFIG_FILES([
  Makefile
  vpn/Makefile
])
AC_OUTPUT
