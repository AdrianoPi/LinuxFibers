#ifndef FIBERS_COMMON 
#define FIBERS_COMMON

#include <linux/init.h>      // Macros e.g., __init __exit
//#include <asm/uaccess.h>   // for get_user and put_user
#include <linux/kernel.h>    // We're doing kernel programming
#include <linux/module.h>    // Secifically, a module

#define log(fmt,...) \
    printk(KERN_INFO "\e[1;33mFIBERS\e[0m: " fmt , ##__VA_ARGS__)

#define SUCCESS 0
#define ERROR  -1


#define DEBUG                // Undefine to decrease verbosity

#ifdef DEBUG
# define dbg(fmt,...) \
    printk(KERN_INFO "\e[1;33mFIBERS - dbg\e[0m: " fmt , ##__VA_ARGS__)
#else
# define dbg(fmt)
#endif


#endif
