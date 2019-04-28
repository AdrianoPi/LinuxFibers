#ifndef FIBERS_PROBES
#define FIBERS_PROBES

#include "fibers_proc.h"
#include "fibers.h"

#include <linux/kallsyms.h>
#include <linux/kprobes.h>
#include <linux/fs.h>

int fiber_readdir(struct file *file, struct dir_context *ctx);
struct dentry* fiber_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);


struct pid_entry;

typedef int (*proc_pident_readdir_t)(struct file *file, struct dir_context *ctx, const struct pid_entry *ents, unsigned int nents);
typedef struct dentry * (*proc_pident_lookup_t)(struct inode *dir, struct dentry *dentry, const struct pid_entry *ents, unsigned int nents);
typedef int (*pid_getattr_t)(const struct path *, struct kstat *, u32, unsigned int);
typedef int (*proc_setattr_t)(struct dentry *dentry, struct iattr *attr);




int register_fiber_kretprobe(void);
int unregister_fiber_kretprobe(void);

int entry_handler_readdir(struct kretprobe_instance *k, struct pt_regs *regs);
int handler_readdir(struct kretprobe_instance *k, struct pt_regs *regs);
int handler_lookup(struct kretprobe_instance *k, struct pt_regs *regs);
int entry_handler_lookup(struct kretprobe_instance *k, struct pt_regs *regs);

#define NOD(NAME, MODE, IOP, FOP, OP) {     \
				.name = (NAME),         \
				.len  = sizeof(NAME) - 1,     \
				.mode = MODE,         \
				.iop  = IOP,          \
				.fop  = FOP,          \
				.op   = OP,         \
}

#define DIR(NAME, MODE, iops, fops) \
				NOD(NAME, (S_IFDIR|(MODE)), &iops, &fops, {} )



#endif
