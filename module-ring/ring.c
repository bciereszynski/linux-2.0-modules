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
int buffersize[BUFFERS_COUNT];
int buffercount[BUFFERS_COUNT];
int start[BUFFERS_COUNT], end[BUFFERS_COUNT];
int usecount[BUFFERS_COUNT];
struct semaphore sem[BUFFERS_COUNT];

struct wait_queue *read_queue[BUFFERS_COUNT], *write_queue[BUFFERS_COUNT];

int get_minor(struct inode *inode)
{
	int minor;
	minor = MINOR(inode->i_rdev);
	if (minor > BUFFERS_COUNT - 1)
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
	down(&sem[minor]);
	MOD_INC_USE_COUNT;
	usecount[minor]++;
	if (usecount[minor] == 1)
	{
		buffer[minor] = kmalloc(BUFFERSIZE, GFP_KERNEL);
		if (buffer[minor] == NULL)
		{
			usecount[minor]--;
			up(&sem[minor]);
			return -ENOMEM;
		}

		buffersize[minor] = BUFFERSIZE;
		buffercount[minor] = 0;
		start[minor] = 0;
		end[minor] = 0;
	}
	up(&sem[minor]);
	return 0;
}

void ring_release(struct inode *inode, struct file *file)
{
	int minor = get_minor(inode);
	if (minor < 0)
	{
		return;
	}
	usecount[minor]--;
	if (usecount[minor] == 0)
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
		while (buffercount[minor] == 0)
		{
			if (usecount[minor] == 1)
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
		if (start[minor] == buffersize[minor])
			start[minor] = 0;
		buffercount[minor]--;
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
		while (buffercount[minor] == buffersize[minor])
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
		buffercount[minor]++;
		end[minor]++;
		if (end[minor] == buffersize[minor])
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

		if (new_size == buffersize[minor])
			return 0;

		if (new_size < buffercount[minor])
			return -EBUSY;

		down(&sem[minor]);

		old_start = start[minor];
		old_end = end[minor];
		old_count = buffercount[minor];
		old_size = buffersize[minor];

		new_buffer = kmalloc(new_size, GFP_KERNEL);
		if (new_buffer == NULL)
		{
			up(&sem[minor]);
			return -ENOMEM;
		}

		for (i = 0; i < old_count && i < new_size; i++)
		{
			int old_index = (old_start + i) % old_size;
			new_buffer[i] = buffer[minor][old_index];
		}

		start[minor] = 0;
		end[minor] = i % new_size;
		buffercount[minor] = (i < new_size) ? i : new_size;

		kfree(buffer[minor]);
		buffer[minor] = new_buffer;
		buffersize[minor] = new_size;

		up(&sem[minor]);

		if (buffercount[minor] < new_size)
			wake_up(&write_queue[minor]);

		return 0;

	case RING_IOC_GETBUFSIZE:
		put_user(buffersize[minor], (int *)arg);
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
		usecount[i] = 0;
		buffersize[i] = BUFFERSIZE;
		sem[i] = MUTEX;
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

	return result;
}
void cleanup_module()
{
	unregister_chrdev(RING_MAJOR, "ring");
}
