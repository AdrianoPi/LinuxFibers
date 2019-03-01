#include "common.h"
#include "driver.h"
#include "fibers_driver.h"


MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("AdrianoPi & Be-P");
MODULE_DESCRIPTION("Linux Fibers module.");  
MODULE_VERSION("0.1");        


static int __init ex0_init(void){
	
    log("Hello from kernel space!\n");
    init_driver();
    
	return SUCCESS;
}

static void __exit ex0_exit(void){

    destroy_driver();
    log("Goodbye from kernel space!\n");
   
}


module_init(ex0_init);
module_exit(ex0_exit);


