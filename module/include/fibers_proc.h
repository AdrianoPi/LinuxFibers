#ifndef FIBERS_PROC
#define FIBERS_PROC



#include "fibers.h"
#include <linux/pid.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#define DUMP_SIZE 1024




union proc_op {
	int (*proc_get_link)(struct dentry *, struct path *);
	int (*proc_show)(struct seq_file *m,
		struct pid_namespace *ns, struct pid *pid,
		struct task_struct *task);
};

struct pid_entry {
	const char *name;
	unsigned int len;
	umode_t mode;
	const struct inode_operations *iop;
	const struct file_operations *fop;
	union proc_op op;
};

struct proc_inode {
				struct pid *pid;
				unsigned int fd;
				union proc_op op;
				struct proc_dir_entry *pde;
				struct ctl_table_header *sysctl;
				struct ctl_table *sysctl_entry;
				struct hlist_node sysctl_inodes;
				const struct proc_ns_operations *ns_ops;
				struct inode vfs_inode;
};

static inline struct proc_inode *PROC_I(const struct inode *inode)
{
    return container_of(inode, struct proc_inode, vfs_inode);
}

static inline struct pid *proc_pid(struct inode *inode)
{
    return PROC_I(inode)->pid;
}

ssize_t fiber_read(struct file *filp, char __user *buf, size_t len, loff_t *off);

struct dentry* fiber_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);

int fiber_readdir(struct file *file, struct dir_context *ctx);




typedef int (*proc_pident_readdir_t)(struct file *file, struct dir_context *ctx, const struct pid_entry *ents, unsigned int nents);
typedef struct dentry * (*proc_pident_lookup_t)(struct inode *dir, struct dentry *dentry, const struct pid_entry *ents, unsigned int nents);

extern proc_pident_readdir_t readdir;
extern proc_pident_lookup_t lookup;

#endif
