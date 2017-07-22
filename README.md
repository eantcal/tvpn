# tvpn
TVPN is an open source software application that implements virtual private network techniques for creating p2p or site2site connection configurations.

You can compile TVPN code using GCC (to generate user space tools g++ 4.8.1 or higher is required).
- TVPN works on GNU/Linux distributions based on kernel since version 2.6.38
- TVPN relies on Vnddmgr which is an alternative network device driver similar to Linux TAP (namely network tap). 
  - Vnddmgr simulates a link layer device and it operates with layer 2 packets like Ethernet frames. Packets sent by an operating system via a Vnddmgr virtual devices are delivered to a user-space program which attaches itself to the device. 
A user-space program may also pass packets into a vnddmgr device. In this case the vnddmgr device delivers (or "injects") these packets to the operating-system network stack thus emulating their reception from an external source.

## Building TVPN

- Getting the latest released code. 
- Download and uncompress it.

- Enter the package-name directory where you uncompressed it, and type the following commands:
```
cd driver
./clean.sh
make
cd ..
./configure
make
```

## Prerequisites for building TVPN
To build TVPN on Linux you need to install GNU g++ compiler and kernel headers.
For example, using a Debian/Ubuntu distros open the Terminal and then type the following apt-get command as root user:
```
   sudo apt-get install build-essential linux-headers-$(uname -r)
```

Example: Using TVPN to can create tunnels to connect private networks across public networks (Internet).
Consider the following sample scenario:

![alt text](https://7bcac53c-a-62cb3a1a-s-sites.googlegroups.com/site/eantcal/home/c/creating-tiny-vpn/vpnfigura5.jpg?attachauth=ANoY7cqqXhAH6qcveOrt8n_uB3qIaUWKndED_wANzqkwOZgRhvAnLjQMF5o6u0JGvK8Hbux1tJl-8Ip1JlOscvDCfgClNhm0-r2YhLL0n3OiTt-xK2DEMz11fWZpf6FaflhzJoAUkRFBnxoO5x7t9j8zskn-oRdNpXlRnVRsfmIT9hut_FQ4QtXU5HjCvpAs1TjQE64s9e1yQKI4UsPn2Nmz5llL0Mf0e9wZ6MNaP44uLNpk4KpfLypc_3vzA4o_EBa_KG8GrAtp&attredirects=0 "Scenario")

- H1 and H2 are two hosts on which TVPN framework is installed, in particular:
- LAN 1 is C class network with the address 192.168.1.0/24.
- LAN 2 is C class network with the address 192.168.2.0/24.
- H1 a host with two network interfaces: one configured with a public IP address 120.0.0.1 and the other with a private address 192.168.1.254.
- H2 a host with two network interfaces : one configured with a public IP address 120.0.0.2 and the other with a private address 192.168.2.254.
- H1 is able to reach H2 through the interface IP 120.0.0.1. Just like H2 can reach H1 through its public interface.

The two hosts are default gateway for the respective private networks.
The choice of addresses is random and made to illustrate the example, so replace it with your proper values.
Taking into account the above scenario, let us take an example of the configuration of hosts H1 and H2 in order to create the virtual LAN 3.

First we have to create virtual interfaces (let's call it vlan3) on both the hosts using the same command:
sudo vnddconfig add vlan3
The previous operation is permissible since the namespace of the interface is confined to each host.
After we have to configure the virtual interfaces. 
To configure H1, we may use the following command:

```
sudo ifconfig vlan3 192.168.3.1
```
And similarly for H2:
```
sudo ifconfig vlan3 192.168.3.2
```
Alternatively, you can create interfaces as broadcast. In this case you need to give different mac address to the VLAN3 interface, leaving ARP eanbled or statically updating the ARP cache of each host.
Once the creation of virtual interfaces, you can create the tunnels running on both hosts the program vnddvpnd.

On H1 you can use the command:
```
sudo vnddvpnd -tunnel vlan3 120.0.0.1 33000 120.0.0.2 33000
```
And similarly on the H2:
```
sudo vnddvpnd -tunnel vlan3 120.0.0.2 33000 120.0.0.1 33000 
```
Even if the port 33000 was chosen arbitrarily, in general , that choice should take into account the configuration of the system, the firewall configuration and etc.

The vnddvpnd program can be run as a service by specifying the optional parameter "-daemonize".
To obtain that the connection of the tunnel is encrypted you can use the parameter "-pwd" followed by the string used as key of DES, which must be the same for H1 and H2.

The full list of parameters accepted by vnddvpnd and vnddconfig can be obtained by running these programs without arguments.
To allow H1 and H2 be the gateway for the respective sub-networks, we need to enable IP forwarding.
This can be achieved by writing "1" in the entry "/proc/sys/net/ipv4/ip_forward" in the /proc file system, using (for example) the command:
```
sudo sysctl -w net.ipv4.ip_forward=1
```
Finally, to complete the configuration of the VPN you must update the routing table of the hosts of the private networks, setting as default gateway the host 254 of the respective subnets.
When configuration will be completed, any host of the 192.168.1.x private network can communicate with any other host on the private network 192.168.2.x.
