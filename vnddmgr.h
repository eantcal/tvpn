/*
 *  vnddmgr.h
 *
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

#ifndef __VNDDMGR_H__
#define __VNDDMGR_H__

/* -------------------------------------------------------------------------- */

#define VNDDMGR_DESCRIPTION "Virtual Network Device Driver Manager Module"
#define VNDDMGR_AUTHOR      "Antonino Calderone (acaldmail@gmail.com)"
#define VNDDMGR_VERSION     "1.3"
#define VNDDMGR_LICENSE     "GPL"

/* -------------------------------------------------------------------------- */

#include <linux/ioctl.h>
#ifdef __KERNEL__
#  include <linux/if.h>
#else
#include <sys/types.h>
#endif

/* -------------------------------------------------------------------------- */
/* chardev definitions                                                        */
/* -------------------------------------------------------------------------- */
#define VNDDMGR_CDEV_IOC_MAGIC 0xAC
#define VNDDMGR_CDEV_IOCRESET (VNDDMGR_CDEV_IOC_MAGIC, 0)
#define VNDDMGR_CDEV_IOGETVER _IOR(VNDDMGR_CDEV_IOC_MAGIC, 1, int)
#define VNDD_PREFIX "vndd"
#define VNDDMGR_CDEV_DIR "/dev/"
#define VNDDMGR_CDEV_NAME "vnddmgr"
#define VNDDMGR_CDEV_VERSION 0x0102

/* -------------------------------------------------------------------------- */
/* Interface attribute sizes                                                  */
/* -------------------------------------------------------------------------- */

#ifndef IFNAMSIZ
#  define IFNAMSIZ 16
#endif

#ifndef IFHWADDRLEN
#  define IFHWADDRLEN 6
#endif

#ifndef ETH_ALEN
#  define ETH_ALEN 6
#endif

/* -------------------------------------------------------------------------- */

typedef int boolean_t;

/* ----------------------------------------------------------------------------
 * CDEV_REQUEST
 *
 * Request Message format
 * +----------+--------------+ - - - - - - -
 * | cmd code | magic cookie | specific part
 * +----------+--------------+- - - - - - -  
 * <-- CDEV_REQUEST header -->
 */
#define CDEV_REQUEST_MAGIC_COOKIE "CDEV"
#define CDEV_REQUEST_MAGIC_COOKIE_LEN (sizeof(CDEV_REQUEST_MAGIC_COOKIE)-1)

#define CDEV_REQUEST_CMD_ADD_IF         1
#define CDEV_REQUEST_CMD_ANNOUNCE_TO_IF 2
#define CDEV_REQUEST_CMD_REMOVE_IF      3

#define CDEV_REQUEST_MAX_LENGTH 2048

/* Max number of virtual manged interfaces */
#define MAX_VNDD_COUNT 1024

/* Packet queue length */
#define CDEV_PKTQ_LEN  1000

/* -------------------------------------------------------------------------- */

/* Add interface message struct */
typedef struct 
{
  char device_name[IFNAMSIZ];
  char hw_address [IFHWADDRLEN];
  unsigned int mtu;
  int enable_arp;
} 
cdev_request_add_interface_t;

/* -------------------------------------------------------------------------- */

/* Remove interface message struct */
typedef struct 
{
  char device_name[IFNAMSIZ];
} 
cdev_request_remove_interface_t;

/* -------------------------------------------------------------------------- */

/* Announce packet message struct */
typedef struct 
{
  size_t pkt_len;
  char device_name[IFNAMSIZ];
  char pkt_buffer[CDEV_REQUEST_MAX_LENGTH]; 
} 
cdev_request_announce_pkt_t;

/* -------------------------------------------------------------------------- */

/* message code type */
typedef unsigned int cdev_request_cmd_code_t;

/* -------------------------------------------------------------------------- */

/* request type */
typedef union 
{
  cdev_request_add_interface_t     add_eth_if;
  cdev_request_announce_pkt_t      announce_pkt;
  cdev_request_remove_interface_t  remove_eth_if;
} 
cdevreq_param_t;

/* -------------------------------------------------------------------------- */

/* request magic cookie type */
typedef char magic_cookie_t[CDEV_REQUEST_MAGIC_COOKIE_LEN];

/* -------------------------------------------------------------------------- */

/* request type */
typedef struct _cdev_request_t 
{         
                                      // +-------------------------
  cdev_request_cmd_code_t cmd_code;   // | cdev_request header
  magic_cookie_t magic_cookie;        // | (cmd_code, magic_cookie)
                                      // +-------------------------
  cdevreq_param_t cdevreq_spec_param; // | cdev_request spec. part
                                      // +-------------------------
} cdev_request_t;

/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* request const                                                              */
/* -------------------------------------------------------------------------- */

#define CDEV_REQUEST_HEADER_SIZE \
 (sizeof(cdev_request_cmd_code_t)+sizeof(magic_cookie_t))

#define CDEV_REQUEST_ANNOUNCE_PKT_H_LEN \
 (CDEV_REQUEST_HEADER_SIZE + \
   (sizeof(cdev_request_announce_pkt_t)-CDEV_REQUEST_MAX_LENGTH))

#ifdef __KERNEL__
#  define CDEV_REQUEST_IS_VALID_REQUEST(_CDEV_REQUEST_PTR_) \
     (memcmp((_CDEV_REQUEST_PTR_)->magic_cookie, \
              CDEV_REQUEST_MAGIC_COOKIE, \
              CDEV_REQUEST_MAGIC_COOKIE_LEN) == 0)
#endif

#endif /* __VNDDMGR_H__ */

