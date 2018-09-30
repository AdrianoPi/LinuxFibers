#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>
#include <linux/fs.h>             // Header for the Linux file system support
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h> /* for put_user */
#include <linux/sched.h>
#include <linux/ptrace.h>

#include "chardev.h"
#include "_chardev.h"

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Gabrio Tognozzi");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A Linux Fibers implementation");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users

/* Global variables are declared as static, so are global within the file. */
static int Major;

static struct file_operations fops = {
    .write = NULL,
    .read = NULL,
    .open = device_open,
    .unlocked_ioctl = device_ioctl,
    .release = device_release

};

int init_module(void){
	printk("trying to register with MAJOR:%d\n",MAJOR_NUM);

        Major = register_chrdev(MAJOR_NUM,DEVICE_NAME,&fops);

        if(Major<0){
                printk("Registering the character device failed with %d\n",
                        Major);
                return Major;
        }

        printk("<1>I was assigned major number %d. To talk to\n", Major);
        printk("<1>the driver, create a dev file with\n");
        printk("'mknod /dev/hello c %d 0'.\n", Major);
        printk("<1>Try various minor numbers. Try to cat and echo to\n");
        printk("the device file.\n");
        printk("<1>Remove the device file and module when done.\n");

        return 0;
}



void cleanup_module(void){
	printk("Removing the chardev module\n");
        unregister_chrdev(Major,DEVICE_NAME);
}




/********************** FILE METHODS **********/

static int device_open(struct inode*inode, struct file *file){
        printk("Device opened from pid:%ld, heppy hacking!\n",
                (long int)(current->pid)); 
        
        return SUCCESS;
}


/* Called when a process closes the device file.
*/
static int device_release(struct inode *inode, struct file *file)
{
        return SUCCESS;
}


static long device_ioctl(
    struct file* file ,
    unsigned int ioctl_num,
    unsigned long ioctl_param)
{
    
   struct pt_regs *pt;

   printk("Issued ioctl:%d\n",ioctl_num);

   switch(ioctl_num){
        case IOCTL_PRINT_HELLO:
            printk("Hello from ioctl!, param is:%lu\n",ioctl_param);  
            break;
        case IOCTL_CHANGE_IP:
		printk("hello from change ip, param is:%lu\n",ioctl_param);
	        pt = current_pt_regs();
                pt->ip = ioctl_param;
            break;
        default:
            printk("Error! unknown code of issued ioctl\n");break;

    }
    
    return SUCCESS;
}


