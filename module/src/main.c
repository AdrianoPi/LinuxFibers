#include "common.h"
#include "driver.h"
#include "fibers_driver.h"


MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("Be-P");
MODULE_DESCRIPTION("A simple Linux driver for the BBB.");  
MODULE_VERSION("0.1");        


static int __init ex0_init(void){
	
    log("Hello %s from kernel space!\n",name);
    init_driver();
    
	return SUCCESS;
}

static void __exit ex0_exit(void){

    destroy_driver();
	log("Goodbye %s from kernel space!\n",name);
   
}


module_init(ex0_init);
module_exit(ex0_exit);


