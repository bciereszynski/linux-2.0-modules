/*
RING BUFFER LINUX 2.0 MODULE
Authors:
Ciereszyński Bartosz
Drażba Filip
Zajączkowski Piotr
*/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/malloc.h>
#include <linux/ioctl.h>
#include <asm/semaphore.h>

#define BUFFERSIZE 1024
#define BUFFERS_COUNT 4
#define MIN_BUFFER_SIZE 256
#define MAX_BUFFER_SIZE 16384

#define RING_MAJOR 60
#define RING_IOC_SETBUFSIZE _IOW(RING_MAJOR, 1, int)
#define RING_IOC_GETBUFSIZE _IOR(RING_MAJOR, 2, int *)

static char *buffer[BUFFERS_COUNT];
int buffersizes[BUFFERS_COUNT];
int buffercounts[BUFFERS_COUNT];
int start[BUFFERS_COUNT], end[BUFFERS_COUNT];
int usecounts[BUFFERS_COUNT];
struct semaphore sem = MUTEX;
struct wait_queue *read_queue[BUFFERS_COUNT], *write_queue[BUFFERS_COUNT];

int get_minor(struct inode *inode)
{
	int minor;
	minor = MINOR(inode->i_rdev);
	if (minor > 3)
	{
		return -ENODEV;
	}
	return minor;
}

int ring_open(struct inode *inode, struct file *file)
{
	int minor = get_minor(inode);
	if (minor < 0)
	{
		return minor;
	}
	down(&sem);
	MOD_INC_USE_COUNT;
	usecounts[minor]++;
	if (usecounts[minor] == 1)
	{
		// kmalloc moze uspic proces - uwaga na synchronizacje
		buffer[minor] = kmalloc(BUFFERSIZE, GFP_KERNEL);
		if (buffer[minor] == NULL){
			usecounts[minor]--;
			up(&sem);
			return -ENOMEM;
		}

		buffersizes[minor] = BUFFERSIZE;
		buffercounts[minor] = 0;
		start[minor] = 0;
		end[minor] = 0;
	}
	up(&sem);
	return 0;
}

void ring_release(struct inode *inode, struct file *file)
{
	int minor = get_minor(inode);
	if (minor < 0)
	{
		return;
	}
	usecounts[minor]--;
	if (usecounts[minor] == 0)
		kfree(buffer[minor]);
	MOD_DEC_USE_COUNT;
}

int ring_read(struct inode *inode, struct file *file, char *pB, int count)
{
	int i;
	char tmp;
	int minor = get_minor(inode);
	if (minor < 0)
	{
		return minor;
	}
	for (i = 0; i < count; i++)
	{
		while (buffercounts[minor] == 0)
		{
			if (usecounts[minor] == 1)
				return i;

			interruptible_sleep_on(&read_queue[minor]);

			if (current->signal & ~current->blocked)
			{
				if (i == 0)
					return -ERESTARTSYS;
				return i;
			}
		}

		tmp = buffer[minor][start[minor]];
		start[minor]++;
		if (start[minor] == buffersizes[minor])
			start[minor] = 0;
		buffercounts[minor]--;
		wake_up(&write_queue[minor]);
		put_user(tmp, pB + i);
	}
	return count;
}

int ring_write(struct inode *inode, struct file *file, const char *pB, int count)
{
	int i;
	char tmp;
	int minor = get_minor(inode);
	if (minor < 0)
	{
		return minor;
	}
	for (i = 0; i < count; i++)
	{
		tmp = get_user(pB + i);
		while (buffercounts[minor] == buffersizes[minor])
		{
			interruptible_sleep_on(&write_queue[minor]);
			if (current->signal & ~current->blocked)
			{
				if (i == 0)
					return -ERESTARTSYS;
				return i;
			}
		}
		buffer[minor][end[minor]] = tmp;
		buffercounts[minor]++;
		end[minor]++;
		if (end[minor] == buffersizes[minor])
			end[minor] = 0;
		wake_up(&read_queue[minor]);
	}
	return count;
}

int ring_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int new_size, i;
	char *new_buffer;
	int old_start, old_end, old_count, old_size;
	int minor = get_minor(inode);
	if (minor < 0)
	{
		return minor;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
	{
		/*
		 * Have to validate the address given by the process.
		 */
		int err, len;

		len = _IOC_SIZE(cmd);

		if ((err = verify_area(VERIFY_WRITE, (void *)arg, len)) < 0)
			return err;
	}

	switch (cmd)
	{
	case RING_IOC_SETBUFSIZE:
		new_size = (int)arg;

		if (new_size < MIN_BUFFER_SIZE || new_size > MAX_BUFFER_SIZE)
			return -EINVAL;

		if (new_size == buffersizes[minor])
			return 0;

		down(&sem);

		old_start = start[minor];
		old_end = end[minor];
		old_count = buffercounts[minor];
		old_size = buffersizes[minor];

		new_buffer = kmalloc(new_size, GFP_KERNEL);
		if (new_buffer == NULL){
			up(&sem);
			return -ENOMEM;
		}

		for (i = 0; i < old_count && i < new_size; i++)
		{
			int old_index = (old_start + i) % old_size;
			new_buffer[i] = buffer[minor][old_index];
		}

		start[minor] = 0;
		end[minor] = i % new_size;
		buffercounts[minor] = (i < new_size) ? i : new_size;

		kfree(buffer[minor]);
		buffer[minor] = new_buffer;
		buffersizes[minor] = new_size;

		up(&sem);

		if (buffercounts[minor] < new_size)
			wake_up(&write_queue[minor]);

		return 0;

	case RING_IOC_GETBUFSIZE:
		put_user(buffersizes[minor], (int *)arg);
		return 0;

	default:
		return -EINVAL;
	}
}

struct file_operations ring_ops = {
	read : ring_read,
	write : ring_write,
	open : ring_open,
	release : ring_release,
	ioctl : ring_ioctl
};

int ring_init(void)
{
	int i;
	for (i = 0; i < BUFFERS_COUNT; i++)
	{
		init_waitqueue(&write_queue[i]);
		init_waitqueue(&read_queue[i]);
		usecounts[i] = 0;
		buffersizes[i] = BUFFERSIZE;
	}
	return register_chrdev(RING_MAJOR, "ring", &ring_ops);
}

int init_module()
{
	int result = ring_init();
	if (!result)
	{
		printk("Ring device initialized!\n");
	}
	else
	{
		printk("Ring device was NOT initialized!\n");
	}

	return result;
}
void cleanup_module()
{
	unregister_chrdev(RING_MAJOR, "ring");
}
