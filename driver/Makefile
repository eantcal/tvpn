#  Makefile for vnddmgr driver 
#  vnddmgr driver is part of TVPN.
#  Copyright (c) 2006 Antonino Calderone, All Rights Reserved.
#  
#  TVPN is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of
#  the License, or (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston,
#  MA 02111-1307 USA
#  

CONFIG_MODULE_SIG=y
CONFIG_MODULE_SIG_HASH=sha512

CFLAGS +=  $(cflags-y)

obj-m :=  vnddmgr.o

KDIR    := /lib/modules/$(shell uname -r)/build
PWD    := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) $(PWD)/.. modules

