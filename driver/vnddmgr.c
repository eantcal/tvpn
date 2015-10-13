/*
 *  vnddmgr.c
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

/* -------------------------------------------------------------------------- */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/mempool.h>
#include <linux/in.h>
#include <linux/netdevice.h>   
#include <linux/etherdevice.h> 
#include <linux/skbuff.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/ethtool.h>
#include <linux/version.h>
#include <linux/init.h>

/* -------------------------------------------------------------------------- */

#include "../vnddmgr.h"

/* -------------------------------------------------------------------------- */
/* DBG                                                                        */
/* -------------------------------------------------------------------------- */

#define DBG(frmt, args...) printk( KERN_DEBUG frmt, ## args)

static int dbglevel = 0;
module_param(dbglevel, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(dbglevel, "vnddfmgr debug level (0-disabled)");

/* -------------------------------------------------------------------------- */

#define frmt_mac(_M) (_M)[0] & 0xff, (_M)[1] & 0xff, (_M)[2] & 0xff,\
   (_M)[3] & 0xff, (_M)[4] & 0xff, (_M)[5] & 0xff  

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Mutex defs                                                                 */
/* -------------------------------------------------------------------------- */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
#define init_MUTEX(sem)         sema_init(sem, 1)
#define DECLARE_MUTEX(name)     \
   struct semaphore name = __SEMAPHORE_INITIALIZER(name, 1)
#endif


/* -------------------------------------------------------------------------- */
/* Hash table definition                                                      */
/* (key -> device name, value -> net device)                                  */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

typedef void* hash_data_ptr_t;

/* -------------------------------------------------------------------------- */


#define HASH_KEY_SIZE (IFNAMSIZ+1)
typedef char hash_key_t[HASH_KEY_SIZE];


/* -------------------------------------------------------------------------- */

typedef struct _hash_item_t 
{
   hash_key_t key;
   hash_data_ptr_t data_ptr;
   struct _hash_item_t * next;
} 
hash_item_t;


/* -------------------------------------------------------------------------- */

typedef size_t (*hash_fnc_t)(hash_key_t);


/* -------------------------------------------------------------------------- */

typedef struct _hash_tbl_t 
{
   size_t size;
   hash_item_t** chain_ptr;
   mempool_t * hash_item_pool;
   hash_fnc_t hash;
   spinlock_t lock;
} 
hash_tbl_t;


/* -------------------------------------------------------------------------- */

typedef int(*hash_iterator_t)(hash_key_t, hash_data_ptr_t);  


/* -------------------------------------------------------------------------- */

static void hash_constructor(
      hash_tbl_t * hash_p, 
      size_t size, 
      hash_fnc_t hash_fnc_ptr);
static void hash_destructor(hash_tbl_t* hash_p);
static void hash_insert(
      hash_tbl_t * hash_p, 
      hash_key_t key, 
      hash_data_ptr_t data_ptr);
static int hash_remove(hash_tbl_t* hash_p, hash_key_t key); 
static hash_item_t* hash_search(hash_tbl_t* hash_p, hash_key_t key);
static int hash_for_each(hash_tbl_t * hash_p, hash_iterator_t it);
static size_t hash_fnc(hash_key_t key);
#define hash_key_assign(_KEY1,_KEY2) strncpy(_KEY1,_KEY2, HASH_KEY_SIZE-1)
#define hash_key_equals(_KEY1,_KEY2) (strcmp(_KEY1,_KEY2)==0)


/* -------------------------------------------------------------------------- */
/* Packet definition                                                          */ 
/* -------------------------------------------------------------------------- */

typedef struct _netdev_packet 
{
   struct _netdev_packet *next;
   struct net_device *dev;
   int datalen;
   u8 data[ETH_FRAME_LEN];
} 
netdev_packet_t;

/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Packet queue definition                                                    */ 
/* -------------------------------------------------------------------------- */

typedef struct _pkt_queue_t 
{
   spinlock_t lock;
   spinlock_t pool_lock;

   netdev_packet_t * queue_head_ptr;
   netdev_packet_t * queue_tail_ptr;
   netdev_packet_t * pkt_pool;

   size_t queue_length;
   size_t current_cnt;
} 
pkt_queue_t;

static int pkt_queue_constructor(pkt_queue_t* queue_ptr, size_t size);
static void pkt_queue_destructor(pkt_queue_t* queue_ptr);
static void pkt_queue_push(pkt_queue_t* queue_ptr, netdev_packet_t* pkt_ptr);
static netdev_packet_t* pkt_queue_pop(pkt_queue_t * queue_ptr );
static netdev_packet_t * pkt_queue_get_pkt(pkt_queue_t * queue_ptr );
static void pkt_queue_release_pkt(pkt_queue_t * queue_ptr, netdev_packet_t* pkt_ptr);

#define pkt_get_cnt(_Q) (_Q)->current_cnt 
#define pkt_get_queue_length(_Q) (_Q)->queue_length 
#define pkt_inc_cnt(_Q) (_Q)->current_cnt++
#define pkt_dec_cnt(_Q) (_Q)->current_cnt-- 
#define pkt_queue_empty(_Q) (! (_Q)->queue_head_ptr )
#define pkt_queue_congested(_Q) (pkt_get_cnt(_Q)>=pkt_get_queue_length(_Q))


/* -------------------------------------------------------------------------- */
/* Chardev definition                                                         */ 
/* -------------------------------------------------------------------------- */
#ifndef VNDDMGR_CDEV_MAJOR
#define VNDDMGR_CDEV_MAJOR 0 
#endif

#ifndef VNDDMGR_CDEV_MINOR
#define VNDDMGR_CDEV_MINOR 0
#endif 

#define MAX_CDEV_INSTANCES 1 
static unsigned int cdev_major_num = VNDDMGR_CDEV_MAJOR;


/* -------------------------------------------------------------------------- */
/* Vnddmgr definition                                                         */ 
/* -------------------------------------------------------------------------- */
typedef struct _vnddmgr_t 
{
   struct semaphore critical_section_mutex; 
   spinlock_t lock;
   struct cdev vnddmgr_cdev;  
   hash_tbl_t hash_tbl;   
   int cdev_ref_cnt;
   pkt_queue_t cdev_pkt_queue;
   wait_queue_head_t cdev_read_queue;

} vnddmgr_t;

/* vnddmgr methods */
static ssize_t vnddmgr_announce_packet(
   vnddmgr_t* vnddmgr_ptr, netdev_packet_t *pkt, const char* name);

static int vnddmgr_allocate_net_device(
   vnddmgr_t* vnddmgr_ptr, const char* name, const char* mac_address, int mtu );

static int vnddmgr_remove_net_device(vnddmgr_t* vnddmgr_ptr, const char* name);

/* vnddmgr global unique instance */
static vnddmgr_t g_vnddmgr;

/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Chardev definition                                                         */ 
/* -------------------------------------------------------------------------- */

/* Char device methods */
static dev_t _vnddmgr_cdev_no;
static struct class *_vnddmgr_cl;

static int cdev_vnddmgr_open(
   struct inode *, struct file * filp);

static int cdev_vnddmgr_release(
   struct inode *, struct file * filp);

static ssize_t cdev_vnddmgr_write(
   struct file *filp, const char __user *buf, size_t count, loff_t * ppos);

static ssize_t cdev_vnddmgr_read(
   struct file *filp, char __user *buf, size_t count, loff_t * ppos);

static long cdev_vnddmgr_ioctl(
   struct file *file, unsigned int cmd, unsigned long arg);

/* char device fops */
struct file_operations cdev_vnddmgr_fops = {
   .owner = THIS_MODULE,
   .open  = cdev_vnddmgr_open,
   .release  = cdev_vnddmgr_release,
   .read  = cdev_vnddmgr_read,
   .write  = cdev_vnddmgr_write,
   .unlocked_ioctl  = cdev_vnddmgr_ioctl,
};

/* -------------------------------------------------------------------------- */
/* Chardev request handling methods                                           */ 
/* -------------------------------------------------------------------------- */

static ssize_t cdevreq_announce_packet(
      const cdev_request_announce_pkt_t* cdevreq);

static ssize_t cdevreq_add_if(
      const cdev_request_add_interface_t* cdevreq);

static ssize_t cdevreq_remove_if(
      const cdev_request_remove_interface_t* cdevreq);


/* -------------------------------------------------------------------------- */
/* Netdev definition                                                          */ 
/* -------------------------------------------------------------------------- */

#define MAX_MTU_SIZE ETH_DATA_LEN     /* 1500 */
#define MIN_MTU_SIZE 68

typedef struct _netdev_priv 
{
   vnddmgr_t * vnddmgr_ptr;
   struct net_device_stats stats;
   struct sk_buff *skb;
   spinlock_t lock;
   u32 flags;
} 
netdev_priv_t;

#define NETDEV_TX_ON 1
#define NETDEV_RX_ON 2

#define netdev_tx_enable(_FLGS) { _FLGS |= NETDEV_TX_ON; }
#define netdev_rx_enable(_FLGS) { _FLGS |= NETDEV_RX_ON; }
#define netdev_tx_disable(_FLGS) { _FLGS &= ~NETDEV_TX_ON; }
#define netdev_rx_disable(_FLGS) { _FLGS &= ~NETDEV_RX_ON; }
#define netdev_tx_enabled(_FLGS) ((_FLGS) & NETDEV_TX_ON)
#define netdev_rx_enabled(_FLGS) ((_FLGS) & NETDEV_RX_ON)

static int netdev_open(struct net_device *dev);
static int netdev_release(struct net_device *dev);
static int netdev_tx(struct sk_buff *skb, struct net_device *dev);
static struct net_device_stats *netdev_stats(struct net_device *dev);
static int netdev_change_mtu(struct net_device *dev, int new_mtu);
static void netdev_setup(struct net_device *dev); 


/* -------------------------------------------------------------------------- */
/* vnddmgr implementation                                                     */
/* -------------------------------------------------------------------------- */

static int vnddmgr_allocate_net_device(
      vnddmgr_t* vnddmgr_ptr, 
      const char* name, 
      const char* mac_address,
      int mtu)
{
   int handle = 0, err_code = 0;
   struct net_device* netdev;
   netdev_priv_t *priv = NULL;
   hash_key_t key = {0};

   BUG_ON(! name);
   BUG_ON(! mac_address);
   BUG_ON(! vnddmgr_ptr);

   if (dbglevel>0)
      DBG ("%s: allocating device name "
            "%s with mac address %02x:%02x:%02x:%02x:%02x:%02x\n", 
            __FUNCTION__, name, frmt_mac(mac_address));

   /* Alloc a new netdev instance */
   netdev = alloc_netdev( 
	sizeof(netdev_priv_t), 
	name,  
#ifdef NET_NAME_UNKNOWN
        NET_NAME_UNKNOWN,
#endif
	netdev_setup );

   if ( unlikely( ! netdev ) ) {
    
      printk(KERN_WARNING "%s: alloc_netdev(%ld, %s, %p) failed\n",
            __FUNCTION__, sizeof(netdev_priv_t), name, netdev_setup);

      return -ENODEV; 
   }

   /* register the netdev within hash tables */
   hash_key_assign(key, name);

   spin_lock( & vnddmgr_ptr->hash_tbl.lock );
   hash_insert(& (vnddmgr_ptr->hash_tbl), key, netdev);
   spin_unlock( & vnddmgr_ptr->hash_tbl.lock );

   /* link the netdev to the vnddmgr instance */
   priv = netdev_priv(netdev);
   priv->flags = 0;
   priv->vnddmgr_ptr = vnddmgr_ptr;

   /* Complete netdev configuration with user provided parameters */
   memcpy(/*dst*/ netdev->dev_addr, /*src*/ mac_address, ETH_ALEN); 
   netdev->mtu = mtu;

   /* Register the net device */
   err_code = register_netdev(netdev);

   if (unlikely (err_code) ) {
      printk(KERN_WARNING "%s: register_netdev(%p) failed\n",
            __FUNCTION__, netdev);

      return err_code;
   }

   return (int) handle;
}


/* -------------------------------------------------------------------------- */

static int vnddmgr_remove_net_device(vnddmgr_t* vnddmgr_ptr, const char* name) 
{
   struct net_device* netdev;
   hash_item_t* found = NULL;
   hash_key_t key = {0};
   netdev_priv_t *priv;

   BUG_ON(! vnddmgr_ptr);
   BUG_ON(! name);

   if (dbglevel>0)
      DBG("%s: removing net device named %s\n", __FUNCTION__, name);

   hash_key_assign(key, name);

   spin_lock( & vnddmgr_ptr->hash_tbl.lock );
   found = hash_search( & vnddmgr_ptr->hash_tbl, key );
   spin_unlock( & vnddmgr_ptr->hash_tbl.lock );

   if (unlikely (! found ) ) 
   {
      printk(KERN_WARNING "%s: hash_search failed: %s not found\n",
            __FUNCTION__, name);

      return -ENODEV;
   }

   netdev = (struct net_device *) found->data_ptr;

   if (unlikely (! netdev) ) 
   {
      printk(KERN_WARNING "%s: found->data_ptr is NULL\n", __FUNCTION__);

      return -ENODEV; 
   }

   priv = netdev_priv(netdev);
   netdev_tx_disable(priv->flags);
   netdev_rx_disable(priv->flags);

   spin_lock( & vnddmgr_ptr->hash_tbl.lock );
   hash_remove( & vnddmgr_ptr->hash_tbl, key );
   spin_unlock( & vnddmgr_ptr->hash_tbl.lock );

   /* Unregister netdev */
   unregister_netdev( netdev );
   free_netdev( netdev );

   return 0; /* OK */
}


/* -------------------------------------------------------------------------- */
/* netdev implementation                                                      */
/* -------------------------------------------------------------------------- */

static int netdev_open(struct net_device *dev) 
{
   /* enable tx and rx for current interface */
   netdev_priv_t *priv = netdev_priv(dev);

   netdev_tx_enable(priv->flags);
   netdev_rx_enable(priv->flags);

   /* Interface UP */
   netif_start_queue(dev);
   return 0;
}


/* -------------------------------------------------------------------------- */

static int netdev_release(struct net_device *dev) 
{
   /* can't transmit any more */
   netdev_priv_t *priv = netdev_priv(dev);

   netdev_tx_disable(priv->flags);
   netdev_rx_disable(priv->flags);

   /* Interface DOWN */
   netif_stop_queue(dev); 
   return 0;
}


/* -------------------------------------------------------------------------- */

/* Receive a packet: retrieve, encapsulate and pass over to upper levels
 * Here we announce an ethernet frame to linux !
 */
static void netdev_rx(struct net_device *dev, netdev_packet_t *pkt) 
{
   int rx_ret_code = 0;
   struct sk_buff *skb;
   netdev_priv_t *priv = netdev_priv(dev);

   if (! netdev_rx_enabled(priv->flags)) 
   {
      ++priv->stats.rx_dropped;

      printk(KERN_WARNING 
             "%s: rx disabled: can't announce frame %p for if %s\n", 
            __FUNCTION__, pkt, dev->name);

      return;
   }

   if (! (dev->flags & IFF_UP)) 
   {
      ++priv->stats.rx_dropped;

      printk(KERN_WARNING 
             "%s: if down: can't announce frame %p for if %s\n", 
            __FUNCTION__, pkt, dev->name);

      return;
   }

   /* The packet has been retrieved from the tunnel instance
    * Make a socket buffer around it, so upper layers can handle it
    */
   skb = dev_alloc_skb(pkt->datalen + 2);

   if (unlikely (!skb)) 
   {
      if (printk_ratelimit()) 
      {
         printk(KERN_NOTICE "%s error: enough memory, packet dropped\n", 
               __FUNCTION__);
      }

      ++priv->stats.rx_dropped;
      return;
   }

   skb_reserve(skb, 2); /* align IP on 16B boundary */  
   memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);

   /* Write metadata, and then pass to the receive level */
   skb->dev = dev;
   skb->protocol = eth_type_trans(skb, dev);

   skb->ip_summed = CHECKSUM_UNNECESSARY; 

   ++priv->stats.rx_packets;
   priv->stats.rx_bytes += pkt->datalen;

   rx_ret_code = netif_rx(skb);

   if (rx_ret_code != NET_RX_SUCCESS) 
   {
      printk(KERN_NOTICE "%s: congestion error code %x\n", 
            __FUNCTION__, rx_ret_code);
   }
}


/* -------------------------------------------------------------------------- */

static int netdev_cdev_qsend(char *buf, int len, struct net_device *dev) 
{
   netdev_priv_t *priv;
   int result = 0;
   pkt_queue_t * pkt_queue_ptr;
   netdev_packet_t * pkt = 0;

   BUG_ON (! ( buf && len>0 && dev ) );

   priv = netdev_priv(dev);
   BUG_ON (! priv );

   pkt_queue_ptr = & priv->vnddmgr_ptr->cdev_pkt_queue;

   spin_lock(& pkt_queue_ptr->lock);

   if (unlikely (pkt_queue_congested( pkt_queue_ptr ))) 
   {
      printk(KERN_NOTICE "%s pkt queue congested\n", __FUNCTION__);
      result=-EBUSY;
      goto err_handler; 
   }

   pkt = pkt_queue_get_pkt( pkt_queue_ptr ); 

   if (unlikely (! pkt)) 
   {
      printk(KERN_NOTICE "%s failed pkt pool exhausted\n", __FUNCTION__);
      result=-ENOMEM;
      goto err_handler; 
   } 

   pkt->dev = dev;
   memcpy(pkt->data, buf, len);
   pkt->datalen = len;

   pkt_queue_push( pkt_queue_ptr, pkt );

   priv->stats.tx_packets++;
   priv->stats.tx_bytes += len;
   kfree_skb(priv->skb);

err_handler:
   spin_unlock( & pkt_queue_ptr->lock );

   return result;
}


/* -------------------------------------------------------------------------- */

static int netdev_tx(struct sk_buff *skb, struct net_device *dev) 
{
   int len, ret;
   char *data;
   netdev_priv_t *priv; 

   BUG_ON (! ( skb && dev ) );

   priv = netdev_priv(dev);

   BUG_ON (! priv );

   data = skb->data;
   len = skb->len;

   if (! netdev_tx_enabled(priv->flags) ||
         ! (dev->flags & IFF_UP)) 
   {
      ++ priv->stats.tx_dropped;

      printk(KERN_NOTICE "%s: tx disabled or interface %s down, "
            "(%p, %i) has been dropped\n", 
            __FUNCTION__, dev->name, data, len);

      kfree_skb(skb);

      return 0;
   }

   BUG_ON (!data || len<=0);

   /* save the timestamp */
   dev->trans_start = jiffies; 

   /* skb will be freed by netdev_cdev_qsend */
   priv->skb = skb;

   if ( (ret = netdev_cdev_qsend(data, len, dev)) != 0 ) 
      return ret;

   wake_up_interruptible(& priv->vnddmgr_ptr->cdev_read_queue);

   ++ priv->stats.tx_packets;
   priv->stats.tx_bytes += len;

   return 0;
}


/* -------------------------------------------------------------------------- */

static struct net_device_stats *netdev_stats(struct net_device *dev) 
{
   netdev_priv_t *priv;

   BUG_ON( ! dev );

   priv = netdev_priv(dev);
   BUG_ON( ! priv );

   return &priv->stats;
}


/* -------------------------------------------------------------------------- */

static int netdev_change_mtu(struct net_device *dev, int new_mtu) 
{
   unsigned long flags;
   netdev_priv_t *priv;
   spinlock_t *lock;

   BUG_ON( ! dev );
   priv = netdev_priv(dev);

   BUG_ON( ! priv );
   lock = &priv->lock;

   if ((new_mtu < MIN_MTU_SIZE) || (new_mtu > MAX_MTU_SIZE)) 
      return -EINVAL;

   spin_lock_irqsave(lock, flags);
   dev->mtu = new_mtu;
   spin_unlock_irqrestore(lock, flags);

   return 0; /* success */
}


/* -------------------------------------------------------------------------- */

static const struct net_device_ops netdev_ops = 
{
   .ndo_open		 = netdev_open,
   .ndo_stop		 = netdev_release,
   .ndo_start_xmit = netdev_tx,
   .ndo_change_mtu = netdev_change_mtu,
   .ndo_get_stats  = netdev_stats
};


/* -------------------------------------------------------------------------- */

static void netdev_setup(struct net_device *ndev) 
{ 
   netdev_priv_t *priv;

   BUG_ON( ! ndev );

   priv = netdev_priv(ndev);
   BUG_ON( ! priv );

   memset(priv, 0, sizeof(netdev_priv_t));
   spin_lock_init(&priv->lock);

   ether_setup(ndev); 

   ndev->netdev_ops = &netdev_ops;

   ndev->features |= NETIF_F_GEN_CSUM;
}


/* -------------------------------------------------------------------------- */

static ssize_t vnddmgr_announce_packet(
      vnddmgr_t* vnddmgr_ptr,
      netdev_packet_t *pkt,
      const char* name)
{
   hash_item_t* found = NULL;
   struct net_device * dev = NULL;
   hash_key_t key = {0};

   hash_key_assign(key, name);

   spin_lock( & vnddmgr_ptr->hash_tbl.lock );
   found = hash_search( & vnddmgr_ptr->hash_tbl, key);
   spin_unlock( & vnddmgr_ptr->hash_tbl.lock );

   if (unlikely (!found || !found->data_ptr) ) 
   {
      printk(KERN_NOTICE "%s: device not found by using name %s\n", 
            __FUNCTION__, name);

      return -ENODEV;
   }

   dev = (struct net_device *) found->data_ptr;
   pkt->dev = dev;
   netdev_rx(dev, pkt); 

   return 0;
}


/* -------------------------------------------------------------------------- */
/* chardev implementation                                                     */
/* -------------------------------------------------------------------------- */

static int cdev_vnddmgr_open(struct inode * inode, struct file * filp) 
{
   vnddmgr_t * vnddmgr_ptr;

   if (dbglevel>0)
      DBG ("%s inode=%p filp=%p\n", __FUNCTION__, inode, filp);

   vnddmgr_ptr = container_of(inode->i_cdev, vnddmgr_t, vnddmgr_cdev);
   filp->private_data = vnddmgr_ptr;

   if ( unlikely( down_interruptible (&vnddmgr_ptr->critical_section_mutex) ) ) 
   {
      printk (KERN_NOTICE "%s failed trying to lock cdev access'\n",
            __FUNCTION__);

      return -ERESTARTSYS;
   }

   if ( vnddmgr_ptr->cdev_ref_cnt < MAX_CDEV_INSTANCES ) 
   {
      ++vnddmgr_ptr->cdev_ref_cnt;
   }
   else 
   {
      printk (KERN_NOTICE "%s failed because MAX_CDEV_INSTANCES=%i reached\n", 
            __FUNCTION__, MAX_CDEV_INSTANCES);

      up ( & vnddmgr_ptr->critical_section_mutex );

      return -EBUSY;
   }

   up (& vnddmgr_ptr->critical_section_mutex );

   return 0;
}


/* -------------------------------------------------------------------------- */

static int cdev_vnddmgr_release(struct inode * inode, struct file * filp) 
{
   vnddmgr_t * vnddmgr_ptr;

   if (dbglevel>0)
      DBG ("%s inode=%p filp=%p\n", __FUNCTION__, inode, filp);

   vnddmgr_ptr = container_of(inode->i_cdev, vnddmgr_t, vnddmgr_cdev);
   --vnddmgr_ptr->cdev_ref_cnt;

   return 0;
}


/* -------------------------------------------------------------------------- */

static int cdev_vnddmgr_setup(vnddmgr_t* vnddmgr_ptr) 
{
   int err = 0;

   if (dbglevel>0)
      DBG ("%s dev=%p\n", __FUNCTION__, vnddmgr_ptr);

   BUG_ON (! vnddmgr_ptr );

   cdev_init(&vnddmgr_ptr->vnddmgr_cdev, &cdev_vnddmgr_fops);
   vnddmgr_ptr->vnddmgr_cdev.owner = THIS_MODULE;

   err = cdev_add(&vnddmgr_ptr->vnddmgr_cdev, _vnddmgr_cdev_no, 1);

   if (unlikely(err)) 
   {
      printk(KERN_ERR VNDDMGR_CDEV_NAME ":: %s: Error %d adding " 
            VNDDMGR_CDEV_NAME "0\n", __FUNCTION__, err);

      cdev_del( &vnddmgr_ptr->vnddmgr_cdev );

      return err;
   }
   else 
   {
      cdev_major_num = MAJOR(_vnddmgr_cdev_no);
      if (dbglevel>0)
         DBG ("%s: completed MAJOR = %i\n", __FUNCTION__, cdev_major_num);
   }

   init_MUTEX(& vnddmgr_ptr->critical_section_mutex);
   init_waitqueue_head(& vnddmgr_ptr->cdev_read_queue);
   spin_lock_init(& vnddmgr_ptr->lock );

   hash_constructor( & vnddmgr_ptr->hash_tbl, MAX_VNDD_COUNT, hash_fnc );

   vnddmgr_ptr->cdev_ref_cnt = 0;

   pkt_queue_constructor ( & (g_vnddmgr.cdev_pkt_queue), CDEV_PKTQ_LEN );

   return 0;
}


/* -------------------------------------------------------------------------- */

static ssize_t cdevreq_announce_packet( 
      const cdev_request_announce_pkt_t* cdevreq ) 
{
   netdev_packet_t *pkt = 0;
   ssize_t retVal = 0;

   if (dbglevel>1)
      DBG ("%s cdevreq=%p\n", __FUNCTION__, cdevreq );

   pkt = kmalloc(sizeof(netdev_packet_t), GFP_KERNEL);

   if (unlikely (! pkt) ) 
   {
      printk(KERN_NOTICE "%s: no enough memory to proceed\n", __FUNCTION__);
      return -ENOMEM;
   }

   /* Prepare pkt buffer */
   pkt->next = 0; /* not used here */
   pkt->dev  = 0; /* will be resolved by vnddmgr_announce_packet */
   pkt->datalen = cdevreq->pkt_len;

   if (unlikely (pkt->datalen > ETH_FRAME_LEN) ) 
   {
      printk(KERN_WARNING "%s: pkt->datalen=%i > %i pkt truncated\n", 
            __FUNCTION__, pkt->datalen, ETH_FRAME_LEN );

      pkt->datalen = ETH_FRAME_LEN;
   }

   /* get packet from user space */
   memcpy(pkt->data, cdevreq->pkt_buffer,  pkt->datalen);

   if (dbglevel>1)
      DBG ("%s(%p, %s)\n", __FUNCTION__, &g_vnddmgr, cdevreq->device_name);

   retVal = vnddmgr_announce_packet( &g_vnddmgr, pkt, cdevreq->device_name ); 

   /* free pkt buffer */
   kfree(pkt);

   return retVal;
}


/* -------------------------------------------------------------------------- */

static ssize_t cdevreq_add_if (const cdev_request_add_interface_t* cdevreq) 
{
   if (dbglevel>0)
      DBG ("%s device_name=%s hw_address=%02x:%02x:%02x:%02x:%02x:%02x mtu=%u\n",
            __FUNCTION__, cdevreq->device_name, frmt_mac(cdevreq->hw_address),
            cdevreq->mtu );

   return vnddmgr_allocate_net_device (
         &g_vnddmgr, 
         cdevreq->device_name, 
         cdevreq->hw_address,
         cdevreq->mtu);
}


/* -------------------------------------------------------------------------- */

static ssize_t cdevreq_remove_if(const cdev_request_remove_interface_t* cdevreq) 
{
   if (dbglevel>0)
      DBG ("%s device_name=%s\n", __FUNCTION__, cdevreq->device_name);

   return vnddmgr_remove_net_device ( & g_vnddmgr, cdevreq->device_name );
}


/* -------------------------------------------------------------------------- */

static ssize_t cdev_vnddmgr_write(
      struct file *filp, 
      const char __user *buf, 
      size_t count, 
      loff_t * ppos) 
{
   vnddmgr_t *vnddmgr_ptr = filp->private_data;

   cdev_request_t * cdevreq_ptr = 0;
   ssize_t ret = -EINVAL;

   if (dbglevel>1)
      DBG ("%s filp=%p _user buf=%p count=%ld ppos=%p\n", 
            __FUNCTION__, filp, buf, count, ppos);

   if ( unlikely( down_interruptible(&vnddmgr_ptr->critical_section_mutex) ) ) 
   {
      printk (KERN_WARNING "%s failed trying to lock cdev access'\n",
            __FUNCTION__);

      return -ERESTARTSYS;
   }

   if (unlikely (count > sizeof(cdev_request_t)) ) 
   {
      printk (KERN_WARNING "%s failed: count=%ld > %ld\n", 
            __FUNCTION__, count, sizeof(cdev_request_t));

      ret=-EINVAL;
      goto err_handler; 
   }

   if (unlikely (count < CDEV_REQUEST_HEADER_SIZE) ) 
   {
      printk (KERN_WARNING "%s failed: count=%ld < %ld\n", 
            __FUNCTION__, count, CDEV_REQUEST_HEADER_SIZE);

      ret=-EINVAL;
      goto err_handler; 
   }

   cdevreq_ptr = kmalloc(count, GFP_KERNEL);


   if (unlikely (! cdevreq_ptr) ) 
   {
      printk (KERN_WARNING "%s failed: kmalloc cannot allocate %ld bytes\n", 
            __FUNCTION__, count);

      ret=-ENOMEM;
      goto err_handler; 
   }


   if (unlikely (copy_from_user( cdevreq_ptr, buf, count ))) 
   {
      printk (KERN_WARNING "%s failed: copy_from_user cannot "
            "complete operation\n", __FUNCTION__);

      ret=-EINVAL;
      goto err_handler; 
   }


   if ( unlikely (! CDEV_REQUEST_IS_VALID_REQUEST( cdevreq_ptr )) ) 
   {
      printk (KERN_WARNING "%s failed: CDEV_REQUEST magic cookie "
            "does not match\n", __FUNCTION__);

      ret=-EFAULT;
      goto err_handler; 
   }


   if (dbglevel>0)
      DBG ("%s processing commnad %x \n", __FUNCTION__, cdevreq_ptr->cmd_code);

   switch (cdevreq_ptr->cmd_code) 
   {
      case CDEV_REQUEST_CMD_ADD_IF:
         ret = cdevreq_add_if( 
               (cdev_request_add_interface_t*) 
               &cdevreq_ptr->cdevreq_spec_param );
         break;

      case CDEV_REQUEST_CMD_REMOVE_IF:
         ret = cdevreq_remove_if( 
               (cdev_request_remove_interface_t*) 
               &cdevreq_ptr->cdevreq_spec_param );
         break;

      case CDEV_REQUEST_CMD_ANNOUNCE_TO_IF:
         ret = cdevreq_announce_packet( 
               (cdev_request_announce_pkt_t*) 
               &cdevreq_ptr->cdevreq_spec_param );
         break;

      default:
         printk (KERN_WARNING "%s cmd code %x not recognized\n", 
               __FUNCTION__, cdevreq_ptr->cmd_code);

         ret = -EINVAL;
         break;
   }

err_handler:

   if (likely(cdevreq_ptr)) 
   {
      kfree(cdevreq_ptr);
   }

   up( & vnddmgr_ptr->critical_section_mutex );

   return ret;
}


/* -------------------------------------------------------------------------- */

static ssize_t cdev_vnddmgr_read(
      struct file *filp, 
      char __user *buf, 
      size_t count, 
      loff_t * ppos) 
{
   vnddmgr_t *vnddmgr_ptr = filp->private_data;
   int queue_empty = 0;  
   netdev_packet_t * pkt = 0;
   ssize_t readbytes = 0;
   ssize_t ret = 0;
   pkt_queue_t* pkt_queue_ptr = & vnddmgr_ptr->cdev_pkt_queue;

   BUG_ON(! pkt_queue_ptr );
   BUG_ON(! vnddmgr_ptr );

   if (dbglevel>1)
      DBG ("%s filp=%p _user buf=%p count=%ld ppos=%p\n", 
            __FUNCTION__, filp, buf, count, ppos);

   spin_lock( & pkt_queue_ptr->lock );
   queue_empty = pkt_queue_empty( pkt_queue_ptr );
   spin_unlock( & pkt_queue_ptr->lock );

   if (queue_empty && (filp->f_flags & O_NONBLOCK)) 
   {
      printk (KERN_NOTICE "%s vnddmgr_ptr->cdev_pkt_queue is empty\n", 
            __FUNCTION__); 

      return -EAGAIN;
   }

   if ( unlikely (wait_event_interruptible(vnddmgr_ptr->cdev_read_queue, 
               pkt_queue_ptr->queue_head_ptr != NULL ))) 
   {
      printk (KERN_NOTICE "%s wait_event_interruptible failed\n", 
            __FUNCTION__); 

      return -ERESTARTSYS;
   }

   BUG_ON ( pkt_queue_ptr->queue_head_ptr == NULL );

   spin_lock( & pkt_queue_ptr->lock );
   pkt = pkt_queue_pop( pkt_queue_ptr );
   spin_unlock( & pkt_queue_ptr->lock );

   if (! pkt) 
   { //paranoid
      printk(KERN_WARNING "%s: pkt_queue_pop failed\n", 
            __FUNCTION__);

      ret=-EFAULT;
      goto err_handler; 
   }

   if (count < (pkt->datalen+IFNAMSIZ)) 
   {
      printk(KERN_WARNING "%s: failed while "
            "count=%ld < pkt->datalen+IFNAMSIZ=%i\n", 
            __FUNCTION__, count, pkt->datalen+IFNAMSIZ);

      ret=-EFAULT;
      goto err_handler; 
   }

   /* Notify which is the device is transmitting packet */
   if (dbglevel>1)
      DBG("%s: pkt->dev->name = %s\n", __FUNCTION__, pkt->dev->name);

   if (copy_to_user(buf, pkt->dev->name, IFNAMSIZ)) 
   {
      printk(KERN_WARNING "%s: copy_to_user failed copying ifname\n", 
            __FUNCTION__);
      ret=-EFAULT;
      goto err_handler; 
   }

   *ppos += IFNAMSIZ;

   /* Give the packet buffer to vpn manager */
   readbytes = pkt->datalen < count ? pkt->datalen : count;

   if (copy_to_user(buf+IFNAMSIZ, pkt->data, readbytes)) 
   {
      printk(KERN_WARNING "%s: copy_to_user failed copying pkt->data\n", 
            __FUNCTION__);
      ret=-EFAULT;
      goto err_handler; 
   }

   *ppos += readbytes;

   ret = readbytes + IFNAMSIZ;

err_handler:
   pkt_queue_release_pkt( pkt_queue_ptr, pkt );
   return ret;
}


/* -------------------------------------------------------------------------- */

static long cdev_vnddmgr_ioctl(
      struct file *file,
      unsigned int cmd,	
      unsigned long arg)
{
   if (dbglevel>0)
      DBG ("%s file=%p cmd=%x arg=%u\n", __FUNCTION__, file, cmd, (int) arg);

   switch (cmd) 
   {
      case VNDDMGR_CDEV_IOGETVER:
         return (int) VNDDMGR_CDEV_VERSION;
   }

   return -EIO;
}


/* -------------------------------------------------------------------------- */
/* hash table implementation                                                  */
/* -------------------------------------------------------------------------- */

static int hash_free_netdev(hash_key_t key, hash_data_ptr_t data_ptr) 
{
   struct net_device * netdev = data_ptr;

   if ( netdev ) 
   {
      unregister_netdev( netdev );
      free_netdev( netdev );
   }

   return 0;
}


/* -------------------------------------------------------------------------- */

static void * hash_mempool_alloc(gfp_t gfp_mask, void *pool_data) 
{
   (void) pool_data;

   return kmalloc(sizeof(hash_item_t), gfp_mask);
}


/* -------------------------------------------------------------------------- */

static void hash_mempool_free(void *element, void *pool_data) 
{
   (void) pool_data;

   kfree(element);
}


/* -------------------------------------------------------------------------- */

static void hash_constructor (
      hash_tbl_t * hash_p, 
      size_t size, 
      hash_fnc_t hash_fnc_ptr) 
{
   BUG_ON( ! hash_p );
   BUG_ON( ! size );
   BUG_ON( ! hash_fnc_ptr );

   hash_p->size = size;
   hash_p->chain_ptr= 
      (hash_item_t**) kmalloc(sizeof(hash_item_t*) * size, GFP_KERNEL);

   hash_p->hash_item_pool = mempool_create(size, hash_mempool_alloc, 
         hash_mempool_free, (void*) NULL );

   BUG_ON( ! hash_p->chain_ptr );

   memset(hash_p->chain_ptr, 0, sizeof(hash_item_t*) * size);

   hash_p->hash = hash_fnc_ptr; 

   spin_lock_init( & hash_p->lock );
}


/* -------------------------------------------------------------------------- */

static void hash_insert( 
      hash_tbl_t * hash_p,
      hash_key_t key,
      hash_data_ptr_t data_ptr )
{
   hash_item_t** chain_ptr;
   hash_item_t* new_item;

   BUG_ON(! key );
   BUG_ON(! hash_p );

   chain_ptr = &hash_p->chain_ptr[hash_p->hash( key ) % hash_p->size];

   new_item = (hash_item_t*) 
      mempool_alloc( hash_p->hash_item_pool, GFP_ATOMIC );

   BUG_ON(! new_item );

   hash_key_assign(new_item->key, key);
   new_item->data_ptr = data_ptr;

   if (*chain_ptr) 
   {
      new_item->next = *chain_ptr;
      *chain_ptr = new_item;
   }
   else 
   {
      new_item->next = NULL;
      *chain_ptr = new_item;
   }
}


/* -------------------------------------------------------------------------- */

static hash_item_t* hash_search( hash_tbl_t* hash_p, hash_key_t key ) 
{
   hash_item_t* item = 
      hash_p->chain_ptr[hash_p->hash( key ) % hash_p->size];

   while (item) 
   {
      if (hash_key_equals(key, item->key)) 
      {
         return item;
      }

      item = item->next;
   }

   return NULL;
}


/* -------------------------------------------------------------------------- */

static int hash_remove( hash_tbl_t* hash_p, hash_key_t key ) 
{
   hash_item_t** chain_ptr;
   hash_item_t *item, *prev;

   BUG_ON(! key );

   chain_ptr = 
      &hash_p->chain_ptr[hash_p->hash( key ) % hash_p->size];

   BUG_ON(! chain_ptr );

   item = *chain_ptr;

   if (unlikely(! item)) 
   {
      return -1; 
   }

   if (hash_key_equals(key, item->key)) 
   {
      *chain_ptr = item->next;
      mempool_free( item, hash_p->hash_item_pool );

      return 0; 
   }

   prev = item;

   while (item) 
   {
      if (hash_key_equals(key, item->key)) 
      {
         prev->next = item->next;
         mempool_free( item, hash_p->hash_item_pool );

         return 0; 
      }

      prev = item;
      item = item->next;
   }

   return -1;
}


/* -------------------------------------------------------------------------- */

static void hash_destructor( hash_tbl_t* hash_p ) 
{
   hash_item_t** chain_ptr;
   hash_item_t *item, *prev;
   size_t i = 0;

   for (; i<hash_p->size; ++i) 
   {
      chain_ptr = &hash_p->chain_ptr[i];

      BUG_ON( ! chain_ptr );

      item = *chain_ptr;

      while (item) 
      {
         prev = item;
         item = item->next;
         mempool_free( prev, hash_p->hash_item_pool );
      } 
   }

   mempool_destroy( hash_p->hash_item_pool );

   kfree( hash_p->chain_ptr );
}


/* -------------------------------------------------------------------------- */

static int hash_for_each( hash_tbl_t * hash_p, hash_iterator_t it ) 
{
   size_t i = 0;

   for (; i<hash_p->size; ++i) 
   {
      hash_item_t* item = hash_p->chain_ptr[i];

      while (item) 
      {
         if (it(item->key, item->data_ptr) != 0) 
         {
            return -1;
         }
         item = item->next;
      } 
   }

   return 0;
}


/* -------------------------------------------------------------------------- */

static size_t hash_fnc(hash_key_t key) 
{
   char c;
   size_t s = 0;

   do {
      c = *key++;
      s += c & 0xff;
   } 
   while (c != '\0');

   return s;
}


/* -------------------------------------------------------------------------- */
/* pkt queue implementation                                                   */
/* -------------------------------------------------------------------------- */

static int pkt_queue_constructor( pkt_queue_t * queue_ptr, size_t length ) 
{
   netdev_packet_t* pkt_ptr = 0;

   if (unlikely( ! queue_ptr || length <= 0 )) 
   {
      return -EINVAL;
   }

   queue_ptr->queue_head_ptr = 0; 
   queue_ptr->queue_tail_ptr = 0;
   queue_ptr->queue_length = length; 
   queue_ptr->current_cnt = 0; 

   spin_lock_init( &queue_ptr->lock );
   spin_lock_init( &queue_ptr->pool_lock );

   queue_ptr->pkt_pool = NULL;

   while (length --) 
   {
      pkt_ptr = kmalloc( sizeof(netdev_packet_t), GFP_KERNEL );

      if (pkt_ptr) 
      {
         memset(pkt_ptr, 0, sizeof(netdev_packet_t));
         pkt_ptr->next = queue_ptr->pkt_pool;
         queue_ptr->pkt_pool = pkt_ptr;
      }
      else 
      {
         pkt_ptr = queue_ptr->pkt_pool;

         while ( pkt_ptr ) 
         {
            queue_ptr->pkt_pool = queue_ptr->pkt_pool->next;
            kfree( pkt_ptr );
            pkt_ptr = queue_ptr->pkt_pool;
         }

         return -ENOMEM;
      }
   }

   return 0;
}


/* -------------------------------------------------------------------------- */

static void pkt_queue_destructor( pkt_queue_t * queue_ptr ) 
{
   netdev_packet_t* pkt_ptr = 0;
   netdev_packet_t* prev_pkt_ptr = 0;

   BUG_ON( queue_ptr == NULL );

   if (dbglevel>0)
      DBG ("%s queue_ptr=%p queue_ptr->queue_head_ptr=%p \n",
            __FUNCTION__, queue_ptr, queue_ptr->queue_head_ptr );

   pkt_ptr = queue_ptr->queue_head_ptr;

   while (likely(pkt_ptr)) 
   {
      prev_pkt_ptr = pkt_ptr;
      pkt_ptr = pkt_ptr->next;

      if (dbglevel>0)
         DBG ("%s kfree( %p )\n", __FUNCTION__, prev_pkt_ptr);

      kfree(prev_pkt_ptr);
   }
}


/* -------------------------------------------------------------------------- */

static void pkt_queue_push(pkt_queue_t * queue_ptr, netdev_packet_t* pkt_ptr) 
{

   BUG_ON(! queue_ptr );
   BUG_ON(! pkt_ptr );

   pkt_ptr->next = 0;

   if ( ! pkt_queue_congested( queue_ptr ) ) 
   {
      pkt_inc_cnt(queue_ptr);

      if (pkt_queue_empty(queue_ptr)) 
      {
         queue_ptr->queue_head_ptr = queue_ptr->queue_tail_ptr = pkt_ptr;
      }
      else 
      {
         queue_ptr->queue_tail_ptr->next = pkt_ptr;
         queue_ptr->queue_tail_ptr = pkt_ptr;
      } 
   } 
}


/* -------------------------------------------------------------------------- */

static netdev_packet_t* pkt_queue_pop( pkt_queue_t * queue_ptr ) 
{
   netdev_packet_t* pkt_ptr = 0;

   BUG_ON(! queue_ptr );

   if (! pkt_queue_empty(queue_ptr)) 
   {
      pkt_ptr = queue_ptr->queue_head_ptr;
      queue_ptr->queue_head_ptr = pkt_ptr->next;

      pkt_dec_cnt(queue_ptr);
   }

   return pkt_ptr;
}


/* -------------------------------------------------------------------------- */

static netdev_packet_t * pkt_queue_get_pkt( pkt_queue_t * queue_ptr ) 
{
   netdev_packet_t * pkt_ptr;

   BUG_ON(! queue_ptr );


   spin_lock( & queue_ptr->pool_lock ); 

   pkt_ptr = queue_ptr->pkt_pool;

   if (pkt_ptr) 
   {
      queue_ptr->pkt_pool = queue_ptr->pkt_pool->next;
   }

   spin_unlock( & queue_ptr->pool_lock ); 


   if ( ! pkt_ptr ) 
   { 
      printk(KERN_WARNING "%s: pool exhausted",  __FUNCTION__);
   }

   return pkt_ptr;
}


/* -------------------------------------------------------------------------- */

static void pkt_queue_release_pkt(
      pkt_queue_t * queue_ptr, 
      netdev_packet_t * pkt_ptr) 
{
   BUG_ON(! queue_ptr );

   if (! pkt_ptr) 
   {
      printk(KERN_WARNING 
            "%s: cannot free pkt_ptr while is NULL",  __FUNCTION__);
      return;
   }

   spin_lock( & queue_ptr->pool_lock ); 

   pkt_ptr->next = queue_ptr->pkt_pool;
   queue_ptr->pkt_pool = pkt_ptr;

   spin_unlock( & queue_ptr->pool_lock ); 
}


/* -------------------------------------------------------------------------- */
/* Module init and clean-up methods                                           */
/* -------------------------------------------------------------------------- */

static int __init module_vnddmgr_init(void) 
{
   int result;

   memset( &g_vnddmgr, 0, sizeof(g_vnddmgr));

   _vnddmgr_cdev_no = MKDEV(VNDDMGR_CDEV_MAJOR, VNDDMGR_CDEV_MINOR);

   result = VNDDMGR_CDEV_MAJOR == 0 ?
      alloc_chrdev_region(&_vnddmgr_cdev_no, 
            VNDDMGR_CDEV_MINOR, 1, VNDDMGR_CDEV_NAME) :
      register_chrdev_region(_vnddmgr_cdev_no, 1, VNDDMGR_CDEV_NAME); 

   if (result < 0) 
   {
      printk(KERN_WARNING "%s failed registering " 
            VNDDMGR_CDEV_NAME " chardev region\n", __FUNCTION__ );
   }
   else 
   {
      /* /sys/class */
      if (unlikely( (_vnddmgr_cl = class_create(THIS_MODULE, "chardrv")) == NULL))
      {
         printk(KERN_ERR "%s failed creating class for '" 
               VNDDMGR_CDEV_NAME "' \n", __FUNCTION__ );
         goto _cleanup_2;
      }

      /* /dev */
      if (unlikely(device_create(
               _vnddmgr_cl, 
               NULL, 
               _vnddmgr_cdev_no, 
               NULL, 
               VNDDMGR_CDEV_NAME) == NULL))
      {
         printk(KERN_ERR "%s failed creating device '" 
               VNDDMGR_CDEV_NAME "' \n", __FUNCTION__ );
         goto _cleanup_1;
      }

      if (unlikely(cdev_vnddmgr_setup( & g_vnddmgr )))
         goto _cleanup_1;

   }

   return result;

_cleanup_1:
         class_destroy(_vnddmgr_cl);
_cleanup_2:
         unregister_chrdev_region(_vnddmgr_cdev_no, MAX_CDEV_INSTANCES);

   return -1;
}


/* -------------------------------------------------------------------------- */

static void __exit module_vnddmgr_cleanup(void) 
{ 
   device_destroy(_vnddmgr_cl, _vnddmgr_cdev_no);
   class_destroy(_vnddmgr_cl);

   unregister_chrdev_region(_vnddmgr_cdev_no, MAX_CDEV_INSTANCES);

   hash_for_each( & g_vnddmgr.hash_tbl, hash_free_netdev );
   hash_destructor ( & g_vnddmgr.hash_tbl );
   pkt_queue_destructor( & (g_vnddmgr.cdev_pkt_queue) ); 
}


/* -------------------------------------------------------------------------- */

module_init(module_vnddmgr_init);
module_exit(module_vnddmgr_cleanup);


/* -------------------------------------------------------------------------- */

MODULE_LICENSE( VNDDMGR_LICENSE );
MODULE_AUTHOR ( VNDDMGR_AUTHOR );
MODULE_DESCRIPTION ( VNDDMGR_DESCRIPTION );
MODULE_VERSION ( VNDDMGR_VERSION );
MODULE_ALIAS( VNDDMGR_CDEV_NAME );

/* -------------------------------------------------------------------------- */
