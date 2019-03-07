#include "common.h"
#include "fibers_driver.h"
#include "driver.h"
#include "fibers.h"

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>


long int device_ioctl(
    struct file *file,
    unsigned int ioctl_num, 
    unsigned long ioctl_param) 
{
    struct fiber_args fargs;
    
    struct fls_args flsargs;

    switch (ioctl_num) {
        
        case IOCTL_ConvertThreadToFiber:
            return kernelConvertThreadToFiber(current->tgid, current->pid);
            break;
        
        case IOCTL_CreateFiber:
            
            if(!access_ok(VERIFY_READ,ioctl_param,sizeof(struct fiber_args))){
                log("CreateFiber, invalid ioctl_param\n");
                return ERROR;
            }

            if(copy_from_user(&fargs, (void*) ioctl_param, sizeof(struct fiber_args))){
                log("CreateFiber, error Unable to copy_from_user");
                return ERROR;
            }
 
            return  kernelCreateFiber(
                fargs.user_fn,
                fargs.fn_params,
                current->tgid,
                current->pid,
                fargs.stack_base,
                fargs.stack_size);

            break;

        case IOCTL_SwitchToFiber:
            return kernelSwitchToFiber(current->tgid, current->pid, (pid_t) ioctl_param );
            break;
            
            
        case IOCTL_FlsAlloc:
            return kernelFlsAlloc(current->tgid, current->pid);
            break;
            
        case IOCTL_FlsFree:
            return kernelFlsFree(current->tgid, current->pid, (long) ioctl_param );
            break;
            
        case IOCTL_FlsGetValue:
            return kernelFlsGetValue(current->tgid, current->pid, (long) ioctl_param );
            break;
            
        case IOCTL_FlsSetValue:
        
            if(!access_ok(VERIFY_READ,ioctl_param,sizeof(struct fls_args))){
                log("FlsSetValue, invalid ioctl_param\n");
                return ERROR;
            }

            if(copy_from_user(&flsargs, (void*) ioctl_param, sizeof(struct fls_args))){
                log("FlsSetValue, error Unable to copy_from_user");
                return ERROR;
            }
            
            return kernelFlsSetValue(current->tgid, current->pid, flsargs.index, flsargs.value );
            break;
  }

  return SUCCESS;

}

static int device_open(struct inode *inode, 
                       struct file *file)
{
    if(!try_module_get(THIS_MODULE)) return ERROR;
    return SUCCESS;
}

static int device_release(struct inode *inode, 
                          struct file *file)
{  
    kernelProcCleanup(current->tgid);

    module_put(THIS_MODULE); 
    return SUCCESS;
}

/* This function is called whenever a process which 
 * has already opened the device file attempts to 
 * read from it. */
static ssize_t device_read(
    struct file *file,
    char *buffer, /* The buffer to fill with the data */   
    size_t length,     /* The length of the buffer */
    loff_t *offset) /* offset to the file */
{
    return 0;
}


/* This structure will hold the functions to be called
 * when a process does something to the device we
 * created. Since a pointer to this structure is kept in
 * the devices table, it can't be local to
 * init_module. NULL is for unimplemented functions. */

static struct file_operations Fops = {
  .read    = device_read, 
  .unlocked_ioctl   = device_ioctl,   
  .open    = device_open,
  .release =device_release 
};


struct class *cl;

char * class_devnode(struct device * dev, umode_t *m){
    if (m) *m=0666;
    return kasprintf(GFP_KERNEL,"%s",dev_name(dev));
}



int init_driver(void)
{
    int ret;
    void*pret;

    log("Registering Driver.\n");    
    ret = register_chrdev(
            MAJOR_NUM, 
            DRIVER_NAME,
            &Fops);

    if(ret<0){
        log("Error registering Driver.\n");
        return ERROR;
    }

    log("Creating /sys class.\n");
    cl = class_create(THIS_MODULE,DRIVER_NAME);
    if(IS_ERR(cl)) {
        log("Error creating class.");
        unregister_chrdev(MAJOR_NUM,DRIVER_NAME);
        return ERROR;
    }  
    cl->devnode=class_devnode;

    log("Creating /sys/class device.\n");
    pret = (void*)device_create(
            cl,
            NULL,
            MKDEV(MAJOR_NUM,0),
            NULL,
            DRIVER_NAME);
    if(IS_ERR(pret)){
        log("Error creating device.\n");
        class_destroy(cl);
        unregister_chrdev(MAJOR_NUM,DRIVER_NAME);
        return ERROR;
    }


    return SUCCESS;
}

void destroy_driver(void){

    device_destroy(cl,MKDEV(MAJOR_NUM,0));
    class_destroy(cl);
    unregister_chrdev(MAJOR_NUM, DRIVER_NAME);

}
