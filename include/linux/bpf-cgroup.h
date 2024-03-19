/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _BPF_CGROUP_H
#define _BPF_CGROUP_H

#include <linux/jump_label.h>
#include <uapi/linux/bpf.h>

struct sock;
struct sockaddr;
struct cgroup;
struct sk_buff;
struct bpf_sock_ops_kern;

#ifdef CONFIG_CGROUP_BPF

extern struct static_key_false cgroup_bpf_enabled_key;
#define cgroup_bpf_enabled static_branch_unlikely(&cgroup_bpf_enabled_key)

struct cgroup_bpf {
	/*
	 * Store two sets of bpf_prog pointers, one for programs that are
	 * pinned directly to this cgroup, and one for those that are effective
	 * when this cgroup is accessed.
	 */
	struct bpf_prog *prog[MAX_BPF_ATTACH_TYPE];
	struct bpf_prog __rcu *effective[MAX_BPF_ATTACH_TYPE];
	bool disallow_override[MAX_BPF_ATTACH_TYPE];
};

void cgroup_bpf_put(struct cgroup *cgrp);
void cgroup_bpf_inherit(struct cgroup *cgrp, struct cgroup *parent);

int __cgroup_bpf_update(struct cgroup *cgrp, struct cgroup *parent,
			struct bpf_prog *prog, enum bpf_attach_type type,
			bool overridable);

int __cgroup_bpf_query(struct cgroup *cgrp, const union bpf_attr *attr,
		       union bpf_attr __user *uattr);

/* Wrapper for __cgroup_bpf_update() protected by cgroup_mutex */
int cgroup_bpf_update(struct cgroup *cgrp, struct bpf_prog *prog,
		      enum bpf_attach_type type, bool overridable);

int __cgroup_bpf_run_filter_skb(struct sock *sk,
				struct sk_buff *skb,
				enum bpf_attach_type type);

int __cgroup_bpf_run_filter_sk(struct sock *sk,
			       enum bpf_attach_type type);

int __cgroup_bpf_run_filter_sock_addr(struct sock *sk,
				      struct sockaddr *uaddr,
				      enum bpf_attach_type type);

int __cgroup_bpf_run_filter_sock_ops(struct sock *sk,
				     struct bpf_sock_ops_kern *sock_ops,
				     enum bpf_attach_type type);

int cgroup_bpf_query(struct cgroup *cgrp, const union bpf_attr *attr,
		     union bpf_attr __user *uattr);

			
/* Wrappers for __cgroup_bpf_run_filter_skb() guarded by cgroup_bpf_enabled. */
#define BPF_CGROUP_RUN_PROG_INET_INGRESS(sk, skb)			      \
({									      \
	int __ret = 0;							      \
	if (cgroup_bpf_enabled)						      \
		__ret = __cgroup_bpf_run_filter_skb(sk, skb,		      \
						    BPF_CGROUP_INET_INGRESS); \
									      \
	__ret;								      \
})

#define BPF_CGROUP_RUN_PROG_INET_EGRESS(sk, skb)			       \
({									       \
	int __ret = 0;							       \
	if (cgroup_bpf_enabled && sk && sk == skb->sk) {		       \
		typeof(sk) __sk = sk_to_full_sk(sk);			       \
		if (sk_fullsock(__sk))					       \
			__ret = __cgroup_bpf_run_filter_skb(__sk, skb,	       \
						      BPF_CGROUP_INET_EGRESS); \
	}								       \
	__ret;								       \
})

#define BPF_CGROUP_RUN_PROG_INET_SOCK(sk)				       \
({									       \
	int __ret = 0;							       \
	if (cgroup_bpf_enabled && sk) {					       \
		__ret = __cgroup_bpf_run_filter_sk(sk,			       \
						 BPF_CGROUP_INET_SOCK_CREATE); \
	}								       \
	__ret;								       \
})

#define BPF_CGROUP_RUN_SA_PROG(sk, uaddr, type)				       \
({									       \
	int __ret = 0;							       \
	if (cgroup_bpf_enabled)						       \
		__ret = __cgroup_bpf_run_filter_sock_addr(sk, uaddr, type);    \
	__ret;								       \
})

#define BPF_CGROUP_RUN_SA_PROG_LOCK(sk, uaddr, type)			       \
({									       \
	int __ret = 0;							       \
	if (cgroup_bpf_enabled)	{					       \
		lock_sock(sk);						       \
		__ret = __cgroup_bpf_run_filter_sock_addr(sk, uaddr, type);    \
		release_sock(sk);					       \
	}								       \
	__ret;								       \
})

#define BPF_CGROUP_RUN_PROG_INET4_BIND(sk, uaddr)			       \
	BPF_CGROUP_RUN_SA_PROG(sk, uaddr, BPF_CGROUP_INET4_BIND)

#define BPF_CGROUP_RUN_PROG_INET6_BIND(sk, uaddr)			       \
	BPF_CGROUP_RUN_SA_PROG(sk, uaddr, BPF_CGROUP_INET6_BIND)

#define BPF_CGROUP_PRE_CONNECT_ENABLED(sk) (cgroup_bpf_enabled && \
					    sk->sk_prot->pre_connect)

#define BPF_CGROUP_RUN_PROG_INET4_CONNECT(sk, uaddr)			       \
	BPF_CGROUP_RUN_SA_PROG(sk, uaddr, BPF_CGROUP_INET4_CONNECT)

#define BPF_CGROUP_RUN_PROG_INET6_CONNECT(sk, uaddr)			       \
	BPF_CGROUP_RUN_SA_PROG(sk, uaddr, BPF_CGROUP_INET6_CONNECT)

#define BPF_CGROUP_RUN_PROG_INET4_CONNECT_LOCK(sk, uaddr)		       \
	BPF_CGROUP_RUN_SA_PROG_LOCK(sk, uaddr, BPF_CGROUP_INET4_CONNECT)

#define BPF_CGROUP_RUN_PROG_INET6_CONNECT_LOCK(sk, uaddr)		       \
	BPF_CGROUP_RUN_SA_PROG_LOCK(sk, uaddr, BPF_CGROUP_INET6_CONNECT)

#define BPF_CGROUP_RUN_PROG_SOCK_OPS(sock_ops)				       \
({									       \
	int __ret = 0;							       \
	if (cgroup_bpf_enabled && (sock_ops)->sk) {	       \
		typeof(sk) __sk = sk_to_full_sk((sock_ops)->sk);	       \
		if (__sk && sk_fullsock(__sk))				       \
			__ret = __cgroup_bpf_run_filter_sock_ops(__sk,	       \
								 sock_ops,     \
							 BPF_CGROUP_SOCK_OPS); \
	}								       \
	__ret;								       \
})
#else

struct cgroup_bpf {};
static inline void cgroup_bpf_put(struct cgroup *cgrp) {}
static inline void cgroup_bpf_inherit(struct cgroup *cgrp,
				      struct cgroup *parent) {}

#define BPF_CGROUP_PRE_CONNECT_ENABLED(sk) (0)
#define BPF_CGROUP_RUN_PROG_INET_INGRESS(sk,skb) ({ 0; })
#define BPF_CGROUP_RUN_PROG_INET_EGRESS(sk,skb) ({ 0; })
#define BPF_CGROUP_RUN_PROG_INET_SOCK(sk) ({ 0; })
#define BPF_CGROUP_RUN_PROG_INET4_BIND(sk, uaddr) ({ 0; })
#define BPF_CGROUP_RUN_PROG_INET6_BIND(sk, uaddr) ({ 0; })
#define BPF_CGROUP_RUN_PROG_INET4_CONNECT(sk, uaddr) ({ 0; })
#define BPF_CGROUP_RUN_PROG_INET4_CONNECT_LOCK(sk, uaddr) ({ 0; })
#define BPF_CGROUP_RUN_PROG_INET6_CONNECT(sk, uaddr) ({ 0; })
#define BPF_CGROUP_RUN_PROG_INET6_CONNECT_LOCK(sk, uaddr) ({ 0; })
#define BPF_CGROUP_RUN_PROG_SOCK_OPS(sock_ops) ({ 0; })

#endif /* CONFIG_CGROUP_BPF */

#endif /* _BPF_CGROUP_H */
