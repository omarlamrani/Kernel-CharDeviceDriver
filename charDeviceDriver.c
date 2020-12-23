#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <charDeviceDriver.h>
#include <linux/slab.h>
#include <linux/list.h>


MODULE_LICENSE("GPL");

typedef struct node{
	const char* mesg;
	int msgLen;
	struct node* next;
} node;

typedef struct list{
  node * top;
} list;

DEFINE_MUTEX  (devLock);

static int counter = 0;

static int totalSize = 0;

static list *kernelMsg = NULL;

static int amount = 0;

int push(list *l, const char* input){

	node* new = kmalloc(sizeof(struct node), GFP_KERNEL);
	
	if(new == NULL){
		return -1;
	}

	new->mesg = input;
	new->next = l->top;
	new->msgLen = strlen(input);
	l->top = new;
	(amount)++;
  
	return 0;
}

int pop(list *l){

	node* tempPop = l->top;

	if(l->top == NULL) {
		return -1;
	}
	
	kfree((l->top)->mesg);
	kfree(l->top);
	l->top = tempPop->next;
	(amount)--;


	return 0;
}

int init_module(void)
{
    Major = register_chrdev(0, DEVICE_NAME, &fops);

    if (Major < 0) {
      printk(KERN_ALERT "Registering char device failed with %d\n", Major);
      return Major;
    }

    printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
    printk(KERN_INFO "the driver, create a dev file with\n");
    printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
    printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
    printk(KERN_INFO "the device file.\n");
    printk(KERN_INFO "Remove the device file and module when done.\n");
    kernelMsg = kmalloc(sizeof(struct list), GFP_KERNEL);

    return SUCCESS;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
    /*  Unregister the device */
    
    unregister_chrdev(Major, DEVICE_NAME);
    kfree(kernelMsg);
}

/* 
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{
    mutex_lock (&devLock);
    if(Device_Open){
    	mutex_unlock (&devLock);
    return -EBUSY;
    }
    Device_Open++;
    mutex_unlock (&devLock);
    // sprintf("this: %s\n", (kernelMsg->top)->mesg);
    printk(KERN_ALERT "DEVICE OPENED");
    try_module_get(THIS_MODULE);
    

    return SUCCESS;
}

/* Called when a process closes the device file. */
static int device_release(struct inode *inode, struct file *file)
{
    mutex_lock (&devLock);
    Device_Open--;      /* We're now ready for our next caller */
    mutex_unlock (&devLock);
    /* 
     * Decrement the usage count, or else once you opened the file, you'll
     * never get get rid of the module. 
     */
    printk(KERN_ALERT "DEVICE CLOSED");
    module_put(THIS_MODULE);

   
    return 0;
}

/* 
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 * This is just an example about how to copy a message in the user space
 */
static ssize_t device_read(struct file *filp,   /* see include/linux/fs.h   */
               char *buffer,    /* buffer to fill with data */
               size_t length,   /* length of the buffer     */
               loff_t * offset)
{
	mutex_lock (&devLock);
	if (amount > 0){
		// printk(KERN_INFO "reading message: %s", (kernelMsg->top)->mesg);
		// printk(KERN_INFO "message length: %d", (kernelMsg->top)->msgLen);
		ssize_t result;
		// char* bet = (char*)kmalloc(sizeof((kernelMsg->top)->mesg), GFP_KERNEL);
		// char *ptr; 
		// ptr = strchr(bet,'\0');
		// *ptr = '\0';
        result = copy_to_user(buffer,(kernelMsg->top)->mesg,length);
    if (result != 0) {
       return -EFAULT;
    }
    	pop(kernelMsg);
    	mutex_unlock (&devLock);
    	// printk(KERN_INFO "current_size: %d\n", amount);    	
    } else {
    	mutex_unlock (&devLock);
    	// printk(KERN_INFO "stack empty");
    	return -EAGAIN;
    }
        

    return length;
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello  */
static ssize_t
device_write(struct file *fixlp, const char *buff, size_t len, loff_t * off)
{	
	mutex_lock (&devLock);
	if (sizeof(buff) > 6*1024) {
    	printk(KERN_ALERT "Sorry, this message is too big.\n");
    	mutex_unlock (&devLock);
    	return -EINVAL;
	} else if ((totalSize + (sizeof(buff))) > 4*1024*1024){
		printk(KERN_ALERT "Sorry, no more space is available.\n");
		mutex_unlock (&devLock);
		return -EAGAIN;
	} else {

		totalSize = totalSize + sizeof(buff);
		printk(KERN_INFO "%s\n", buff);
		// char *ptr; 
		// ptr = strchr(buff,'\n');
		// *ptr = '\0';
		// printk(KERN_INFO "pushed message: %s", buff);
		// printk(KERN_INFO "message size: %d", strlen(buff));
		char *temp = kmalloc(sizeof(buff), GFP_KERNEL);
		ssize_t result;
		
        result = copy_from_user(temp, buff, len);
    if (result != 0) {
    	mutex_unlock (&devLock);
       return -EFAULT;
    }
		
		push(kernelMsg, temp);
		// printk(KERN_INFO "pushed message: %s", (kernelMsg->top)->mesg);

		mutex_unlock (&devLock);
		// if (kernelMsg->top == NULL) {
		// 	printk(KERN_ALERT "NOT PUSHED");
		// } else { 
		// 	printk(KERN_ALERT "PUSHED");
		// 	printk(KERN_INFO "current_size: %d", amount);
		// }
		// printk(KERN_INFO "current_length: %d\n", len);
		// printk(KERN_INFO "current_size: %d\n", totalSize);
		// printk(KERN_INFO "top message: %s\n", (kernelMsg->top)->mesg);

		return len;
	}
}
