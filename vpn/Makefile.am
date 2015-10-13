SUBDIRS =
sbin_PROGRAMS = vnddconfig vnddvpnd

vnddconfig_CXXFLAGS = \
   -pthread -std=c++11 \
   -g 

vnddconfig_SOURCES = \
   vndd_config.cc \ 
   vndd_mgr.cc \
   vndd_mgr.h \
   mac_addr.cc \
   mac_addr.h \
   ip_addr.h 

vnddconfig_AM_CXXFLAGS = ${vnddconfig_CXXFLAGS}

vnddconfig_LDFLAGS = -pthread 


vnddvpnd_CXXFLAGS = \
   -pthread -std=c++11 \
   -g 

vnddvpnd_SOURCES = \
   vndd_vpnd.cc \
   vndd_mgr.cc \
   vndd_setup.cc \
   vndd_setup.h \
   vndd_tunnel.cc \
   vndd_tunnel.h \
   udp_socket.cc \
   udp_socket.h \
   des_crypt.cc \
   des_crypt.h \
   mac_addr.h \
   mac_addr.cc \
   ip_addr.h \
   crypter.h


vnddvpnd_AM_CXXFLAGS = ${vnddvpnd_CXXFLAGS}

vnddvpnd_LDFLAGS = -pthread 


