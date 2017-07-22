# tvpn
TVPN is an open source software application that implements virtual private network techniques for creating p2p or site2site connection configurations.

You can compile TVPN code using GCC (to generate user space tools g++ 4.8.1 or higher is required).
- TVPN works on GNU/Linux distributions based on kernel since version 2.6.38
- TVPN relies on Vnddmgr which is an alternative network device driver similar to Linux TAP (namely network tap). 
  - Vnddmgr simulates a link layer device and it operates with layer 2 packets like Ethernet frames. Packets sent by an operating system via a Vnddmgr virtual devices are delivered to a user-space program which attaches itself to the device. 
A user-space program may also pass packets into a vnddmgr device. In this case the vnddmgr device delivers (or "injects") these packets to the operating-system network stack thus emulating their reception from an external source.

