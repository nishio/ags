#define SANDBOX_MAGIC_PRIORITY 1764
#define DISALLOW_SETSID 0

#ifdef DECLARE_HOOK

DECLARE_HOOK(execve)
DECLARE_HOOK(fork)
DECLARE_HOOK(vfork)
DECLARE_HOOK(clone)
#if DISALLOW_SETSID
DECLARE_HOOK(setpgid)
DECLARE_HOOK(setsid)
#endif
DECLARE_HOOK(socketcall)
DECLARE_HOOK(getpriority)
DECLARE_HOOK(setpriority)

#define DECLARE_HOOK_32(name)                   \
    DECLARE_HOOK(name)                          \
    DECLARE_HOOK(name ## 32)
DECLARE_HOOK_32(setuid)
DECLARE_HOOK_32(setreuid)
DECLARE_HOOK_32(setresuid)
DECLARE_HOOK_32(setfsuid)
DECLARE_HOOK_32(setgid)
DECLARE_HOOK_32(setregid)
DECLARE_HOOK_32(setresgid)
DECLARE_HOOK_32(setfsgid)

#else

#include <asm/unistd_32.h>
#include <linux/in.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/net.h>
#include <linux/pid.h>
#include <linux/pid_namespace.h>
#include <linux/sched.h>
#include <linux/socket.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");

// grep sys_call_table /boot/System.map-2.6.26-2-xen-686
#define SYS_CALL_TABLE ((void**)0xc02d355c)

#define PID_MAX ((int*)0xc035df8c)
#define PID_MAX_MIN ((int*)0xc035df90)
#define PID_MAX_MAX ((int*)0xc035df94)

#define GOLF_USER 1000
#define MAX_SOCK_ADDR 128

#define DECLARE_HOOK(syscall)                   \
    static int syscall ## _cnt;
#include "sandbox.c"
#undef DECLARE_HOOK

#define DEFINE_HOOK(name, args)                 \
    asmlinkage int (*orig_ ## name) args ;      \
    asmlinkage static int hook_ ## name args

#define IS_NOT_ROOT current_uid() != 0 /* || current->euid != 0 */

#define NUM_ALLOWED_FORK 2000

DEFINE_HOOK(execve,
            (const char *filename, char *const argv[], char *const envp[])) {
    if (IS_NOT_ROOT) {
        execve_cnt++;
    }
    return orig_execve(filename, argv, envp);
}

DEFINE_HOOK(fork, (void)) {
    if (IS_NOT_ROOT) {
        if (fork_cnt >= NUM_ALLOWED_FORK) {
            return -EPERM;
        }
        fork_cnt++;
    }
    return orig_fork();
}

DEFINE_HOOK(vfork, (void)) {
    if (IS_NOT_ROOT) {
        if (fork_cnt >= NUM_ALLOWED_FORK) {
            return -EPERM;
        }
        fork_cnt++;
    }
    return orig_vfork();
}

DEFINE_HOOK(clone,
            (int (*fn)(void *), void* child_stack, int flags, void* arg,
             pid_t* ptid, struct user_desc* tls, pid_t* ctid)) {
    if (IS_NOT_ROOT) {
        if (fork_cnt >= NUM_ALLOWED_FORK) {
            return -EPERM;
        }
        fork_cnt++;
    }
    return orig_clone(fn, child_stack, flags, arg, ptid, tls, ctid);
}

#if DISALLOW_SETSID

DEFINE_HOOK(setpgid, (pid_t pid, pid_t pgid)) {
    if (IS_NOT_ROOT) {
        if (pid == 0 || pgid == 0) {
            setpgid_cnt++;
            printk(KERN_INFO "setpgid(%d, %d) %d\n",
                   pid, pgid, current->euid);
            return -EPERM;
        }
    }
    return orig_setpgid(pid, pgid);
}

DEFINE_HOOK(setsid, (void)) {
    if (IS_NOT_ROOT) {
        setsid_cnt++;
        printk(KERN_INFO "setsid()\n");
        return -EPERM;
    }
    return orig_setsid();
}

#endif

DEFINE_HOOK(socketcall, (int call, unsigned long* args)) {
    if (IS_NOT_ROOT &&
        (call == SYS_CONNECT || call == SYS_SENDTO || call == SYS_RECVFROM ||
         call == SYS_SENDMSG || call == SYS_RECVMSG)) {
        if (call == SYS_CONNECT) {
            unsigned long a[3];
            struct sockaddr __user* useraddr;
            struct sockaddr kernaddr;
            int addrlen;

            if (copy_from_user(a, args, 3 * sizeof(unsigned long))) {
                printk(KERN_INFO "connect(%p) EFAULT\n", args);
                return -EFAULT;
            }

            useraddr = (struct sockaddr __user*)a[1];
            addrlen = a[2];
            if (addrlen > sizeof(struct sockaddr)) {
                addrlen = sizeof(struct sockaddr);
            }

            memset(&kernaddr, 0, sizeof(struct sockaddr));
            if (copy_from_user(&kernaddr, useraddr, addrlen)) {
                printk(KERN_INFO "connect(%ld, %ld, %ld) EFAULT\n",
                       a[0], a[1], a[2]);
                return -EFAULT;
            }

            if (kernaddr.sa_family == AF_UNIX) {
                goto ok;
            } else if (kernaddr.sa_family == AF_INET) {
                struct sockaddr_in* sock = (struct sockaddr_in*)&kernaddr;
                int port;
                unsigned char* addr;
                port = sock->sin_port;
                port = ((port << 8) & 0xff) | (port >> 8);
                addr = (unsigned char*)&sock->sin_addr.s_addr;
                printk(KERN_INFO "connect(%ld, %d, %d, %d.%d.%d.%d)\n",
                       a[0], sock->sin_family, port,
                       addr[0], addr[1], addr[2], addr[3]);
            }
        } else {
            printk(KERN_INFO "socketcall(%d, %p)\n", call, args);
        }
        socketcall_cnt++;
        return -EPERM;
    }
ok:
    return orig_socketcall(call, args);
}

#define DEFINE_DISABLE_HOOK(name, args, cond, fmt, ...)          \
    DEFINE_HOOK(name, args) {                                    \
        if (IS_NOT_ROOT && cond) {                               \
            name ## _cnt++;                                      \
            printk(KERN_INFO #name "(" fmt ")\n", __VA_ARGS__);  \
            return -EPERM;                                       \
        }                                                        \
        return orig_ ## name (__VA_ARGS__);                      \
    }

#define DEFINE_DISABLE_HOOK_32(name, args, cond, fmt, ...)          \
    DEFINE_DISABLE_HOOK(name, args, cond, fmt, __VA_ARGS__)         \
    DEFINE_DISABLE_HOOK(name ## 32, args, cond, fmt, __VA_ARGS__)

#define CHECK_ID_1(id1) (id1 != GOLF_USER)
#define CHECK_ID_2(id1, id2) (CHECK_ID_1(id1) || CHECK_ID_1(id2))
#define CHECK_ID_3(id1, id2, id3) (CHECK_ID_2(id1, id2) || CHECK_ID_1(id3))

DEFINE_DISABLE_HOOK_32(setuid, (uid_t uid),
                       CHECK_ID_1(uid), "%d", uid)
DEFINE_DISABLE_HOOK_32(setreuid, (uid_t uid, uid_t euid),
                       CHECK_ID_2(uid, euid), "%d, %d", uid, euid)
DEFINE_DISABLE_HOOK_32(setresuid, (uid_t uid, uid_t euid, uid_t suid),
                       CHECK_ID_3(uid, euid, suid),
                       "%d, %d, %d", uid, euid, suid);
DEFINE_DISABLE_HOOK_32(setfsuid, (uid_t uid),
                       CHECK_ID_1(uid), "%d", uid)
DEFINE_DISABLE_HOOK_32(setgid, (gid_t gid),
                       CHECK_ID_1(gid), "%d", gid);
DEFINE_DISABLE_HOOK_32(setregid, (gid_t gid, gid_t egid),
                       CHECK_ID_2(gid, egid), "%d, %d", gid, egid)
DEFINE_DISABLE_HOOK_32(setresgid, (gid_t gid, gid_t egid, gid_t sgid),
                       CHECK_ID_3(gid, egid, sgid),
                       "%d, %d, %d", gid, egid, sgid);
DEFINE_DISABLE_HOOK_32(setfsgid, (uid_t uid),
                       CHECK_ID_1(uid), "%d", uid)

DEFINE_HOOK(getpriority, (int which, int who)) {
    if (which == SANDBOX_MAGIC_PRIORITY) {
        switch (who) {
#define DECLARE_HOOK(syscall)                   \
            case __NR_ ## syscall:              \
                return syscall ## _cnt;
#include "sandbox.c"
#undef DECLARE_HOOK
        default:
            printk(KERN_INFO "getpriority(%d, %d)\n", which, who);
        }
    }
    return orig_getpriority(which, who);
}

DEFINE_HOOK(setpriority, (int which, int who, int prio)) {
    if (which == SANDBOX_MAGIC_PRIORITY) {
        if (who == __NR_setpriority) {
            printk(KERN_INFO "setpriority for setpriority is not permitted\n");
            return -EPERM;
        } else if (who == __NR_getpid) {
            struct pid_namespace* pid_ns;

            if (prio <= *PID_MAX_MIN ||
                prio >= *PID_MAX ||  prio >= *PID_MAX_MAX) {
                return -EINVAL;
            }

            pid_ns = task_active_pid_ns(current);
            pid_ns->last_pid = prio;
            printk(KERN_INFO "setpid(%d) max_min=%d max_max=%d max=%d\n",
                   prio, *PID_MAX_MIN, *PID_MAX_MAX, *PID_MAX);
            return 0;
        }

        switch (who) {
#define DECLARE_HOOK(syscall)                   \
            case __NR_ ## syscall: {            \
                setpriority_cnt++;              \
                syscall ## _cnt = prio;         \
                return 0;                       \
            }
#include "sandbox.c"
#undef DECLARE_HOOK
        default:
            printk(KERN_INFO "setpriority(%d, %d, %d)\n", which, who, prio);
        }
    }
    return orig_setpriority(which, who, prio);
}

int sandbox_init(void) {
    printk(KERN_INFO "sandbox_init called !\n");

#define DECLARE_HOOK(syscall)                               \
    syscall ## _cnt = 0;                                    \
    orig_ ## syscall = SYS_CALL_TABLE[__NR_ ## syscall];    \
    SYS_CALL_TABLE[__NR_ ## syscall] = hook_ ## syscall;
#include "sandbox.c"
#undef DECLARE_HOOK

    return 0;
}

void sandbox_exit(void) {
    printk(KERN_INFO "sandbox_exit called !\n");

#define DECLARE_HOOK(syscall)                                    \
        SYS_CALL_TABLE[__NR_ ## syscall] = orig_ ## syscall;
#include "sandbox.c"
#undef DECLARE_HOOK
}

module_init(sandbox_init);
module_exit(sandbox_exit);

#endif
