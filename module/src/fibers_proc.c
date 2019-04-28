#include "fibers_proc.h"

struct file_operations fiber_fops = {
				read: fiber_read,
};

struct dentry* fiber_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags){

	struct dentry      *ret;

	struct task_struct *task = get_pid_task(proc_pid(dir), PIDTYPE_PID);
	struct process     *process;

	int                 bkt, fiber_i = 0;
	struct fiber       *fiber_p;

	unsigned long       nents;
	struct pid_entry   *fiber_entries;


	if(task == NULL )
		return ERR_PTR(-ENOENT);

	process = get_process_by_id(task->tgid);

	if (process==NULL) return 0;

	nents = atomic_read(&(process->last_fid));

	fiber_entries = kmalloc(nents * sizeof(struct pid_entry), GFP_KERNEL);
	memset(fiber_entries, 0, nents * sizeof(struct pid_entry));

	hash_for_each_rcu(process->fibers, bkt, fiber_p, fnext){

		fiber_entries[fiber_i].name = fiber_p->name;
		fiber_entries[fiber_i].len = strlen(fiber_entries[fiber_i].name);
		fiber_entries[fiber_i].mode = (S_IFREG|(S_IRUGO));
		fiber_entries[fiber_i].iop = NULL;
		fiber_entries[fiber_i].fop = &fiber_fops;

		fiber_i++;

	}

	ret = lookup(dir, dentry, fiber_entries, nents);
	kfree(fiber_entries);
	return ret;

}

int fiber_readdir(struct file *file, struct dir_context *ctx){

	int                 ret;

	struct task_struct *task = get_pid_task(proc_pid(file_inode(file)), PIDTYPE_PID);
	struct process     *process;

	int                 bkt, fiber_i = 0;
	struct fiber       *fiber_p;

	unsigned long       nents;
	struct pid_entry   *fiber_entries;


	if(task == NULL)
		return -ENOENT;

	process = get_process_by_id(task->tgid);

	if (process==NULL) return 0;

	nents = atomic_read(&(process->last_fid));

	fiber_entries = kmalloc(nents * sizeof(struct pid_entry), GFP_KERNEL);
	memset(fiber_entries, 0, nents * sizeof(struct pid_entry));

	hash_for_each_rcu(process->fibers, bkt, fiber_p, fnext){

		fiber_entries[fiber_i].name = fiber_p->name;
		fiber_entries[fiber_i].len = strlen(fiber_entries[fiber_i].name);
		fiber_entries[fiber_i].mode = (S_IFREG|(S_IRUGO));
		fiber_entries[fiber_i].iop = NULL;
		fiber_entries[fiber_i].fop = &fiber_fops;

		fiber_i++;

	}

	ret = readdir(file, ctx, fiber_entries, nents);
	kfree(fiber_entries);
	return ret;

}


ssize_t fiber_read(struct file *filp, char __user *buf, size_t buf_size, loff_t *offset){
	
	char fiber_dump[DUMP_SIZE];
	int dump_len,byte_to_copy;

	struct process *p;
	struct fiber   *f;

	unsigned long fiber_id=0;
	int active_pid;

	struct task_struct *task = get_pid_task(proc_pid(file_inode(filp)), PIDTYPE_PID);

	kstrtoul(filp->f_path.dentry->d_name.name, 10, &fiber_id);

	p = get_process_by_id(task->tgid);
	f = get_fiber_by_id(fiber_id,p);

	active_pid = atomic_read(&(f->active_pid));

	snprintf(fiber_dump, DUMP_SIZE,
		"Currently Running: %s\n"\
		"Start Address: 0x%016lx\n"\
		"Created From: %d\n"\
		"Tot Activations: %lu\n"\
		"Tot Failed Activations: %ld\n"\
		"Total Execution Time: %lu\n\n",
			(active_pid >0) ? "yes" : "no",
			(unsigned long)f->entry_point,
			f->parent,
			f->activations,
			atomic64_read(&(f->failed_activations)),
			f->total_running_time);

	dump_len = strnlen(fiber_dump, DUMP_SIZE);
	if (*offset >= dump_len)
		return 0;

	byte_to_copy =
		(buf_size<(dump_len-*offset-1)) ? buf_size : dump_len-(*offset)-1;

	if (copy_to_user(buf, fiber_dump+*offset, byte_to_copy)) {
				return -EFAULT;
	}
	*offset += byte_to_copy;
	return byte_to_copy;
}
