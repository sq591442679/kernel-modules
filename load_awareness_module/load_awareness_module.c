/**
 * there is 1 kprobes:
 * 1. before callint ip_output() in ip_output.c
 * 		check current queue occupation and might send netlink message to user space
 * 
 * the definition of netlink sockets and its functionality is embedded in kernel source code,
 * because it must be called when the ipv4 module is being initiate,
 * so it is hard to implement through loadable kernel module (LKM)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/socket.h>
#include <linux/netlink.h>
#include <linux/netdevice.h>
#include <linux/types.h>

#include <net/net_namespace.h>
#include <net/netlink.h>
#include <net/sock.h>
#include <net/sch_generic.h>

#define NETLINK_RECV_PACKET	30	
// NOTE: user-specifed netlink type should be smaller than MAX_LINKS in /inlcude/uapi/linux/netlink.h
#define LOAD_AWARENESS_PID	258258

const char interface_names[4][5] = {"eth1", "eth2", "eth3", "eth4"};

static struct kprobe kp_ip_output = {
    .symbol_name = "ip_output",
};

static int __kprobes handler_pre_ip_output(struct kprobe *p, struct pt_regs *regs)
{
    struct net *net = (struct net *)regs->di;
    // struct sock *sk = (struct sock *)regs->si;
    // struct sk_buff *skb = (struct sk_buff *)regs->dx;
	__u32 qlen_list[4] = {0};	// qlen of all 4 eths, -1 for shutdown.
	int len = sizeof(qlen_list);
	struct sk_buff *nl_skb;
	struct nlmsghdr *nlh;
	int i;
	bool should_change_cost = false;
	struct Qdisc *qdisc;

	/** 
	 * @sqsq 
	 * query qlen of 4 interfaces
	 * and send netlink message to user space
	 * */
	for (i = 0; i < 4; ++i) {
		struct net_device *dev = dev_get_by_name(net, interface_names[i]);
		u32 handle = 0;
		if (dev == NULL) {
			qlen_list[i] = -1;
		}
		else {
			rcu_read_lock();
			qdisc = rcu_dereference(dev->qdisc);
			handle = qdisc->handle;
			qlen_list[i] = qdisc_qlen_sum(qdisc);
			// qlen_list[i] = handle;
			rcu_read_unlock();
			dev_put(dev);
		}
		pr_info("%s handle:%x qlen_list[%d]:[%d]\n", __func__, handle, i, qlen_list[i]);
	}

	for (i = 0; i < 4; ++i) {
		if (qlen_list[i] == -1) {
			net->last_time_qlen[i] = 0;
		}
		else {
			__u32 qlen_amplitude = abs(qlen_list[i] - net->last_time_qlen[i]);
			if (qlen_amplitude >= net->qlen_amplitude_threshold) {
				net->last_time_qlen[i] = qlen_list[i];
				should_change_cost = true;
			}
		}
	}

	if (should_change_cost) {
		nl_skb = nlmsg_new(len, GFP_ATOMIC);
		if (nl_skb == NULL) {
			pr_err("%s nlmsg_new failed\n", __func__);
		}

		nlh = nlmsg_put(nl_skb, 0, 0, NETLINK_RECV_PACKET, len, 0);
		if (nlh == NULL) {
			nlmsg_free(nl_skb);
			pr_err("%s nlmsg_put failed\n", __func__);
		}

		memcpy(NLMSG_DATA(nlh), qlen_list, len);
		if (net->recv_packet_nl_sock == NULL) {
			pr_err("%s recv_packet_nl_sock == NULL!\n", __func__);
		}
		else {
			netlink_unicast(net->recv_packet_nl_sock, nl_skb, LOAD_AWARENESS_PID, MSG_DONTWAIT);
		}	

		nlmsg_free(nl_skb);
	}
    return 0;   
}

static int __init load_awareness_module_init(void)
{
    int ret;

    kp_ip_output.pre_handler = handler_pre_ip_output;
    ret = register_kprobe(&kp_ip_output);
	if (ret < 0) {
        pr_err("register_kprobe ip_output failed\n");
    }
    else {
        pr_info("planted kprobe at %p\n", kp_ip_output.addr);
    }

    return 0;
} 

static void __exit load_awareness_module_exit(void)
{
	unregister_kprobe(&kp_ip_output);
    pr_info("unregisterd kprobe\n");
}

module_init(load_awareness_module_init);
module_exit(load_awareness_module_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("sqsq");