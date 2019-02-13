#include "common.h"
#include "fibers_driver.h"
#include "driver.h"
#include <linux/fs.h>
#include <linux/device.h>


long int device_ioctl(
    struct file *file,
    unsigned int ioctl_num,    /* The number of the ioctl */
    long unsigned int ioctl_param) /* The parameter to it */
{
    log("Called ioctl! with msg %d.\n",ioctl_num);

    switch (ioctl_num) {
        
        case IOCTL_PRINT_MSG:
            log("IOCTL Received!\n");
            break;

  }

  return SUCCESS;

}

static int device_open(struct inode *inode, 
                       struct file *file)
{
    return SUCCESS;
}

static int device_release(struct inode *inode, 
                          struct file *file)
{
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

    log("Creating /sys/class device.\n");
    pret = (void*)device_create(
            cl,
            NULL,
            MKDEV(100,0),
            NULL,
            DRIVER_NAME);
    if(IS_ERR(pret)){
        log("Error creating device.\n");
        class_destroy(cl);
        unregister_chrdev(MAJOR_NUM,DRIVER_NAME);
        return ERROR;
    }


    log ("%s The major device number is %d.\n",
              "Registeration is a success", 
              MAJOR_NUM);
    log ("To print, IOCTL:%d\n",IOCTL_PRINT_MSG);

    return SUCCESS;
}

void destroy_driver(void){

    device_destroy(cl,MKDEV(100,0));
    class_destroy(cl);
    unregister_chrdev(MAJOR_NUM, DRIVER_NAME);

}
