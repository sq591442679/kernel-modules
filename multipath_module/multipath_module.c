/**
 * there is 1 kprobe:
 * 1. before calling fib_select_multipath() in fib_semantics.c
 * 		change the logic to find proper nexthop,
 * 		while overriding original fib_select_multipath()
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/socket.h>
#include <linux/rcupdate.h>

#include <net/ip_fib.h>
#include <net/nexthop.h>


static struct kprobe kp_fib_select_multipath = {
    .symbol_name = "fib_select_multipath",
};

void sqsq_fib_select_multipath(struct fib_result *res, const struct flowi4 *fl4)
{
	if (likely(res->fi->nh)) {
		struct nh_info *nhi;
		struct nexthop *nh;

		// nh = nexthop_select_path(res->fi->nh, hash);

		if (!res->fi->nh->is_group) {
			nh = res->fi->nh;
		}
		else {
			struct nh_group *nhg = rcu_dereference(res->fi->nh->nh_grp);
			int i = 0;
			struct nh_grp_entry *min_weight_nhge = NULL;
			for (i = 0; i < nhg->num_nh; ++i) {
				struct nh_grp_entry *nhge = &nhg->nh_entries[i];
				if (i == 0) {
					min_weight_nhge = nhge;
				}
				else {
					struct nh_info *info = rcu_dereference(nhge->nh->nh_info);
					if (nhge->weight < min_weight_nhge->weight && info->fib_nhc.nhc_oif != fl4->__fl_common.flowic_iif) {
						min_weight_nhge = nhge;
					}
				}
			}
			if (min_weight_nhge) {
				nh = min_weight_nhge->nh;
			}
			else {
				nh = NULL;
			}
		}

		if (nh == NULL) {
			pr_info("%s  nh == NULL!!!!!\n", __func__);	
		}
		// else {
		// 	u32 dst_ip = htonl((192 << 24) | (168 << 16) | (46 << 8) | (2));
		// 	nhi = rcu_dereference(nh->nh_info);
		// 	if (fl4->daddr == dst_ip) {
		// 		pr_info("%s nexthop: %pI4\n", __func__, &(nhi->fib_nhc.nhc_gw.ipv4));
		// 	}
		// }

		nhi = rcu_dereference(nh->nh_info);
		res->nhc = &nhi->fib_nhc;
		return;
	}

	else {
		pr_err("%s res->fi->nh is NULL\n",  __func__);
	}
}

/**
 * TODO
 * add __kprobes identifier
 */
static int handler_pre_fib_select_multipath(struct kprobe *p, struct pt_regs *regs)
{
	struct fib_result *res = (struct fib_result *)regs->di;
	const struct flowi4 *fl4 = (const struct flowi4 *)regs->si;

	sqsq_fib_select_multipath(res, fl4);

    return -1;
}

static int __init multipath_module_init(void)
{
    int ret = 0;
    
    kp_fib_select_multipath.pre_handler = handler_pre_fib_select_multipath;
    ret = register_kprobe(&kp_fib_select_multipath);
    if (ret < 0) {
        pr_err("register_kprobe kp_fib_select_multipath failed\n");
    }
    else {
        pr_info("planted kprobe at %p\n", kp_fib_select_multipath.addr);
    }


    return 0;
}

static void __exit multipath_module_exit(void)
{
    unregister_kprobe(&kp_fib_select_multipath);
    pr_info("unregisterd kprobe\n");
}

module_init(multipath_module_init);
module_exit(multipath_module_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("sqsq");