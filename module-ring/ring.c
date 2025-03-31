#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/malloc.h>
#include <asm/semaphore.h>

#define BUFFERSIZE 1024
#define BUFFERS_COUNT 4

static char *buffer[BUFFERS_COUNT];
int buffercounts[BUFFERS_COUNT];
int start[BUFFERS_COUNT], end[BUFFERS_COUNT];
int usecounts[BUFFERS_COUNT];
struct semaphore sem = MUTEX;
struct wait_queue *read_queue[BUFFERS_COUNT], *write_queue[BUFFERS_COUNT];

int get_minor(struct inode *inode){
	int minor;
	minor = MINOR(inode->i_rdev);
	if (minor > 3){
		return 0;
	}
	return minor;
}

int ring_open(struct inode *inode, struct file *file)
{
	int minor = get_minor(inode);
	down(&sem);
	MOD_INC_USE_COUNT;
	usecounts[minor]++;
	if (usecounts[minor] == 1)
	{
		// kmalloc moze uspic proces - uwaga na synchronizacje
		buffer[minor] = kmalloc(BUFFERSIZE, GFP_KERNEL);
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
		if (start[minor] == BUFFERSIZE)
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
	for (i = 0; i < count; i++)
	{
		tmp = get_user(pB + i);
		while (buffercounts[minor] == BUFFERSIZE)
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
		if (end[minor] == BUFFERSIZE)
			end[minor] = 0;
		wake_up(&read_queue[minor]);
	}
	return count;
}

struct file_operations ring_ops = {
	read : ring_read,
	write : ring_write,
	open : ring_open,
	release : ring_release
};

#define RING_MAJOR 60

int ring_init(void)
{
	int i;
	for (i = 0; i<BUFFERS_COUNT; i++){
		init_waitqueue(&write_queue[i]);
		init_waitqueue(&read_queue[i]);
		usecounts[i] = 0;
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
