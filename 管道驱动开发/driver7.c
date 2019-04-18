#include <linux/init.h>
#include <linux/module.h>
#include <linux/param.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/wait.h>

/************defines*****************/
#define MAX_SIZE 1024
#define NUM_DEVICES 2



/***********module*******************/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("handsome777");


/*********gobal vailables*************/
struct class* current_class;
struct file_operations driver7_ops;
dev_t driver7_dev_t;//dev's number(32bit),high12 = main_num,low20 = minor_num
int driver7_major = 0;
int driver7_minor = 0;


//devices struct
struct driver7_dev
{
	struct semaphore sem;
	struct cdev cdev;
	void* data;
	int begin_position;
	int end_position;
	int curr_size;
	int size;
	wait_queue_head_t in_queue;
	wait_queue_head_t out_queue;
};
struct driver7_dev* driver7_devices;

/**********pre define**************/
static char* driver7_devnode(struct device *dev, umode_t *mode);
static void driver7_setup_cdev(struct driver7_dev *dev,int index);
int driver7_open(struct inode * inode, struct file *filp);
int driver7_release(struct inode *inode, struct file *filp);
ssize_t driver7_write(struct file * filp, const char __user *buf, size_t count, loff_t *f_pos);
ssize_t driver7_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);


struct file_operations driver7_ops = 
{
    .owner = THIS_MODULE,
    .open  = driver7_open,
    .read  = driver7_read,
    .write = driver7_write,
    .release = driver7_release
};

int driver7_open(struct inode * inode, struct file *filp)
{
	struct driver7_dev *dev;
	dev = container_of(inode->i_cdev, struct driver7_dev, cdev);//find begin addr
	filp->private_data = dev;
	return 0;
}

//__user* is buffer goto user space, loff_t: long long
ssize_t driver7_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct driver7_dev *dev = filp->private_data;
	if (down_interruptible(&dev->sem))//to get sem
		return -ERESTARTSYS;

	while(dev->curr_size == 0)//buffer is non
	{
		up(&dev->sem);//post sem
		if (filp->f_flags & O_NONBLOCK)//IO is nonblock,return errno
			return -EAGAIN;
		if (wait_event_interruptible(dev->in_queue,(dev->curr_size != dev->size)))
			//wait write to add buffer
			return -ERESTARTSYS;
		if (down_interruptible(&dev->sem))//to get sem
			return -ERESTARTSYS;
	}


	if(count > dev->curr_size)
		count = dev->curr_size;
	int err1 = 0;
	int already_finished = 0;
	if (dev->curr_size + dev->begin_position > dev->size)//data is at both sides
	{
		err1|=copy_to_user(buf,dev->data + dev->begin_position,dev->size - dev->begin_position);
		if (err1)
		{
			up(&dev->sem);//post sem
			return -EFAULT;
		}
		dev->curr_size -= (dev->size - dev->begin_position);
		already_finished += (dev->size - dev->begin_position);
		dev->begin_position = 0;
	}
	err1 |= copy_to_user(buf + already_finished,dev->data + dev->begin_position,count - already_finished);
	dev->curr_size -= (count - already_finished);
	dev->begin_position += (count - already_finished);
	already_finished += count - already_finished;

	up(&dev->sem);
	wake_up_interruptible(&dev->out_queue);

	printk(KERN_INFO "driver7: reading %d characters from driver7",already_finished);
	return already_finished;

}

ssize_t driver7_write(struct file * filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct driver7_dev *dev = filp->private_data;
	if (down_interruptible(&dev->sem))//get sem
	{
		return -ERESTARTSYS;
	}
	int err1 = 0;
	int already_finished = 0;

	while(dev->curr_size == dev->size)//buffer is full
	{
		up(&dev->sem);//post sem
		if (filp->f_flags & O_NONBLOCK)//IO is nonblock, if nonblock return error
			return -EAGAIN;
		//wait read to release some space
		if (wait_event_interruptible(dev->out_queue,(dev->curr_size!=0)))
			return -ERESTARTSYS;
		if (down_interruptible(&dev->sem))//get sem
			return -ERESTARTSYS;
	}

	if(count>dev->size - dev->curr_size)//only write to the full space
		count = dev->size - dev->curr_size;

	if(count + dev->end_position > dev->size)//data is at buffer's both sides
	{
		//get data from user space
		err1|=copy_from_user(dev->data + dev->end_position,buf,dev->size - dev->end_position);
		if(err1)//if fails
		{
			up(&dev->sem);//post sem
			return -EFAULT;
		}
		//adjust
		dev->curr_size += (dev->size-dev->end_position);
		already_finished += (dev->size-dev->end_position);
		dev->end_position = 0;
	}
	//read from user position
	err1 |= copy_from_user(dev->data + dev->end_position,buf + already_finished,count - already_finished);
	if(err1)
	{
		up(&dev->sem);//post sem
		wake_up_interruptible(&dev->in_queue);//awake read
		return -EFAULT;
	}
	dev->curr_size += (count-already_finished);//renew size of data in buffer

	dev->end_position += (count - already_finished);//renew end point
	already_finished += count - already_finished;
	up(&dev->sem);
	wake_up_interruptible(&dev->in_queue);
	printk(KERN_INFO "driver7: writing %d characters to driver7",already_finished);
	return already_finished;
}


int driver7_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int driver7_init(void)
{
	printk(KERN_INFO "bgein init driver7_devices");
	int result;

	//create class
	current_class = class_create(THIS_MODULE,"driver7");//THIS_MODULE is in module.h,define
	current_class->devnode = driver7_devnode;//set mode
	//get a num of dev,alloc sec-dev begin at 0,num of sec-dev,name of dev
	result = alloc_chrdev_region(&driver7_dev_t, 0, NUM_DEVICES,"/dev/driver7");
	printk(KERN_INFO "driver7: successfully alloc chrdev region: %d",result);
	driver7_major = MAJOR(driver7_dev_t);//get main num
	printk(KERN_INFO "driver7: driver7 use major %d", driver7_major);
	//alloc region of save
	driver7_devices = kmalloc(NUM_DEVICES * sizeof(struct driver7_dev), GFP_KERNEL);
	memset(driver7_devices, driver7_minor, NUM_DEVICES * sizeof(struct driver7_dev));//init new storage with every element 0
	printk(KERN_INFO "driver7: 1,2,3");
	int i = 0;
	for(;i < NUM_DEVICES; i++)
	{
		driver7_devices[i].data = kmalloc(MAX_SIZE * sizeof(char),GFP_KERNEL);
		driver7_devices[i].size = MAX_SIZE;
		driver7_devices[i].begin_position = 0;
		driver7_devices[i].end_position = 0;
		driver7_devices[i].curr_size = 0;
		sema_init(&driver7_devices[i].sem,1);
		init_waitqueue_head(&driver7_devices[i].in_queue);
		init_waitqueue_head(&driver7_devices[i].out_queue);
		driver7_setup_cdev(&driver7_devices[i], i);
	}
	printk(KERN_INFO "driver7: driver7 use major %d",driver7_major);
	return 0;
}

static void driver7_setup_cdev(struct driver7_dev *dev,int index)
{
	int error,dev_no = MKDEV(driver7_major,driver7_minor + index);
	cdev_init(&dev->cdev, &driver7_ops);//init cdev
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &driver7_ops;
	error = cdev_add(&dev->cdev,dev_no,1);//rigister dev
	device_create(current_class,NULL,driver7_dev_t + index, NULL, "driver7%d",index);//construct dev_node
	printk(KERN_INFO "driver7: sucessful setup cdev %d",index);
}


static char* driver7_devnode(struct device *dev, umode_t *mode)
{
	if(mode)
		*mode = 0666;//mode of pipe,write or read
	return kasprintf(GFP_KERNEL, "%s", dev_name(dev));
	//GFP_KERNEL is a define, a simbol of alloc kernel space
	//devname is to get dev name with id = dev
}


void driver7_exit(void)
{
	int i = 0;
	for(; i < NUM_DEVICES; i++)
	{
		printk(KERN_INFO "driver7: begin remove device %d", i);
		device_destroy(current_class,driver7_devices[i].cdev.dev);
		printk(KERN_INFO "driver7: device destoryed %d",i);
		cdev_del(&driver7_devices[i].cdev);
		kfree((void*)driver7_devices[i].data);
	}
	class_destroy(current_class);
	unregister_chrdev_region(driver7_major,NUM_DEVICES);
	kfree((void*)driver7_devices);
}

module_init(driver7_init);
module_exit(driver7_exit);
