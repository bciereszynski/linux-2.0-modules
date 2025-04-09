/*
MORSE CODE TRANSMITTER LINUX 2.0 MODULE
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
#include <linux/timer.h>
#include <asm/semaphore.h>

#include "console_struct.h"

#define MORSE_MAJOR 61
#define DEVICES_COUNT 8
#define DEFAULT_BUFFER_SIZE 256
#define MIN_BUFFER_SIZE 0
#define MAX_BUFFER_SIZE 1024

#define DOT_DURATION 200 // milliseconds
#define DASH_DURATION 600
#define SYMBOL_PAUSE 200
#define LETTER_PAUSE 600
#define WORD_PAUSE 1400

#define MORSE_IOC_SET_DOT_DURATION _IOW(MORSE_MAJOR, 1, int)
#define MORSE_IOC_SET_DASH_DURATION _IOW(MORSE_MAJOR, 2, int)
#define MORSE_IOC_SET_SYMBOL_PAUSE _IOW(MORSE_MAJOR, 3, int)
#define MORSE_IOC_SET_LETTER_PAUSE _IOW(MORSE_MAJOR, 4, int)
#define MORSE_IOC_SET_WORD_PAUSE _IOW(MORSE_MAJOR, 5, int)
#define MORSE_IOC_SET_BUFFER_SIZE _IOW(MORSE_MAJOR, 6, int)
#define MORSE_IOC_GET_BUFFER_SIZE _IOR(MORSE_MAJOR, 7, int *)

// Morse code patterns
static const char *morse_codes[] = {
	".-",	// A
	"-...", // B
	"-.-.", // C
	"-..",	// D
	".",	// E
	"..-.", // F
	"--.",	// G
	"....", // H
	"..",	// I
	".---", // J
	"-.-",	// K
	".-..", // L
	"--",	// M
	"-.",	// N
	"---",	// O
	".--.", // P
	"--.-", // Q
	".-.",	// R
	"...",	// S
	"-",	// T
	"..-",	// U
	"...-", // V
	".--",	// W
	"-..-", // X
	"-.--", // Y
	"--.."	// Z
};

static const char *morse_digits[] = {
	"-----", // 0
	".----", // 1
	"..---", // 2
	"...--", // 3
	"....-", // 4
	".....", // 5
	"-....", // 6
	"--...", // 7
	"---..", // 8
	"----."	 // 9
};

// Timing configuration
static int dot_duration[DEVICES_COUNT];
static int dash_duration[DEVICES_COUNT];
static int symbol_pause[DEVICES_COUNT];
static int letter_pause[DEVICES_COUNT];
static int word_pause[DEVICES_COUNT];

// Buffer management
static char *buffer[DEVICES_COUNT];
static int buffer_size[DEVICES_COUNT];
static int buffer_count[DEVICES_COUNT];
static int buffer_head[DEVICES_COUNT];
static int buffer_tail[DEVICES_COUNT];
static int device_in_use[DEVICES_COUNT];

// Synchronization and state management
static struct semaphore sem[DEVICES_COUNT];
static struct timer_list morse_timer[DEVICES_COUNT];
static int is_transmitting[DEVICES_COUNT];
static char current_char[DEVICES_COUNT];
static const char *current_code[DEVICES_COUNT];
static int code_position[DEVICES_COUNT];
static int signal_state[DEVICES_COUNT]; // 0 = off, 1 = on
struct wait_queue *write_queue[DEVICES_COUNT];

int get_minor(struct inode *inode)
{
	int minor = MINOR(inode->i_rdev);
	if (minor >= DEVICES_COUNT)
	{
		return -ENODEV;
	}
	return minor;
}

void set_signal(int minor, int state)
{
	unsigned long *screen;
	int currcons = fg_console;

	screen = (unsigned long *)origin;

	if (state)
	{
		screen[0] = (0x4 << 12) | (0x4 << 8) | ' ';
	}
	else
	{
		screen[0] = (0x0 << 12) | (0x0 << 8) | ' ';
	}

	signal_state[minor] = state;
}

void morse_timer_function(unsigned long data)
{
	int minor = (int)data;

	if (current_code[minor] == NULL)
	{
		if (buffer_count[minor] > 0)
		{
			current_char[minor] = buffer[minor][buffer_tail[minor]];
			buffer_tail[minor] = (buffer_tail[minor] + 1) % buffer_size[minor];
			buffer_count[minor]--;
			wake_up(&write_queue[minor]);

			if (current_char[minor] >= 'A' && current_char[minor] <= 'Z')
			{
				current_code[minor] = morse_codes[current_char[minor] - 'A'];
			}
			else if (current_char[minor] >= 'a' && current_char[minor] <= 'z')
			{
				current_code[minor] = morse_codes[current_char[minor] - 'a'];
			}
			else if (current_char[minor] >= '0' && current_char[minor] <= '9')
			{
				current_code[minor] = morse_digits[current_char[minor] - '0'];
			}
			else if (current_char[minor] == ' ')
			{
				// Word pause
				set_signal(minor, 0);
				morse_timer[minor].expires = jiffies + (word_pause[minor] * HZ / 1000);
				add_timer(&morse_timer[minor]);
				return;
			}

			code_position[minor] = 0;
			morse_timer[minor].expires = jiffies + 1;
		}
		else
		{
			is_transmitting[minor] = 0;
			set_signal(minor, 0);
			down(&sem[minor]);
			if (device_in_use[minor] == 0)
			{
				kfree(buffer[minor]);
				MOD_DEC_USE_COUNT;
			}
			up(&sem[minor]);
			return;
		}
	}
	else
	{
		if (signal_state[minor])
		{
			// Signal was on, symbol pause
			set_signal(minor, 0);
			morse_timer[minor].expires = jiffies + (symbol_pause[minor] * HZ / 1000);
		}
		else
		{
			// Signal was off
			if (current_code[minor][code_position[minor]] == '\0')
			{
				// Letter pause
				morse_timer[minor].expires = jiffies + (letter_pause[minor] * HZ / 1000);
				current_code[minor] = NULL;
			}
			else if (current_code[minor][code_position[minor]] == '.')
			{
				// Send a dot
				set_signal(minor, 1);
				morse_timer[minor].expires = jiffies + (dot_duration[minor] * HZ / 1000);
				code_position[minor]++;
			}
			else
			{
				// Send a dash
				set_signal(minor, 1);
				morse_timer[minor].expires = jiffies + (dash_duration[minor] * HZ / 1000);
				code_position[minor]++;
			}
		}
	}

	add_timer(&morse_timer[minor]);
	return;
}

int morse_open(struct inode *inode, struct file *file)
{
	int minor = get_minor(inode);
	if (minor < 0)
	{
		return minor;
	}

	down(&sem[minor]);

	device_in_use[minor]++;
	MOD_INC_USE_COUNT;
	if (device_in_use[minor] == 1 && !is_transmitting[minor])
	{
		buffer[minor] = kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
		if (buffer[minor] == NULL)
		{
			up(&sem[minor]);
			return -ENOMEM;
		}

		buffer_count[minor] = 0;
		buffer_head[minor] = 0;
		buffer_tail[minor] = 0;

		is_transmitting[minor] = 0;
		signal_state[minor] = 0;
	}

	up(&sem[minor]);

	return 0;
}

void morse_release(struct inode *inode, struct file *file)
{
	int minor = get_minor(inode);
	if (minor < 0)
	{
		return;
	}

	down(&sem[minor]);
	device_in_use[minor]--;
	if (device_in_use[minor] == 0 && !is_transmitting[minor])
	{
		// Only free the buffer if not transmitting and no more users
		kfree(buffer[minor]);
		MOD_DEC_USE_COUNT;
	}
	up(&sem[minor]);
}

int morse_write(struct inode *inode, struct file *file, const char *buf, int count)
{
	int minor = get_minor(inode);
	int i, bytes_written = 0;
	char ch;

	if (minor < 0)
	{
		return minor;
	}

	for (i = 0; i < count; i++)
	{
		ch = get_user(buf + i);
		while (buffer_count[minor] == buffer_size[minor])
		{
			interruptible_sleep_on(&write_queue[minor]);
			if (current->signal & ~current->blocked)
			{
				if (i == 0)
					return -ERESTARTSYS;
				return i;
			}
		}

		buffer[minor][buffer_head[minor]] = ch;
		buffer_head[minor] = (buffer_head[minor] + 1) % buffer_size[minor];
		buffer_count[minor]++;
		bytes_written++;
	}

	down(&sem[minor]);

	if (!is_transmitting[minor] && buffer_count[minor] > 0)
	{
		is_transmitting[minor] = 1;

		init_timer(&morse_timer[minor]);
		morse_timer[minor].function = morse_timer_function;
		morse_timer[minor].data = minor;
		morse_timer[minor].expires = jiffies + 1;
		add_timer(&morse_timer[minor]);
	}

	up(&sem[minor]);

	return bytes_written;
}

int morse_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int minor = get_minor(inode);
	int value, err;
	char *new_buffer;
	int i, old_size, new_size;

	if (minor < 0)
	{
		return minor;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
	{
		int len = _IOC_SIZE(cmd);
		if ((err = verify_area(VERIFY_WRITE, (void *)arg, len)) < 0)
			return err;
	}

	switch (cmd)
	{
	case MORSE_IOC_SET_DOT_DURATION:
		value = (int)arg;
		if (value <= 0)
		{
			return -EINVAL;
		}
		dot_duration[minor] = value;
		break;

	case MORSE_IOC_SET_DASH_DURATION:
		value = (int)arg;
		if (value <= 0)
		{
			return -EINVAL;
		}
		dash_duration[minor] = value;
		break;

	case MORSE_IOC_SET_SYMBOL_PAUSE:
		value = (int)arg;
		if (value <= 0)
		{
			return -EINVAL;
		}
		symbol_pause[minor] = value;
		break;

	case MORSE_IOC_SET_LETTER_PAUSE:
		value = (int)arg;
		if (value <= 0)
		{
			return -EINVAL;
		}
		letter_pause[minor] = value;
		break;

	case MORSE_IOC_SET_WORD_PAUSE:
		value = (int)arg;
		if (value <= 0)
		{
			return -EINVAL;
		}
		word_pause[minor] = value;
		break;

	case MORSE_IOC_SET_BUFFER_SIZE:
		new_size = (int)arg;
		if (new_size < MIN_BUFFER_SIZE || new_size > MAX_BUFFER_SIZE)
		{
			return -EINVAL;
		}

		if (new_size == buffer_size[minor])
		{
			return 0;
		}

		if (new_size < buffer_count[minor])
		{
			return -EBUSY;
		}

		down(&sem[minor]);
		new_buffer = kmalloc(new_size, GFP_KERNEL);
		if (new_buffer == NULL)
		{
			up(&sem[minor]);
			return -ENOMEM;
		}

		old_size = buffer_size[minor];
		if (buffer_count[minor] > 0)
		{
			for (i = 0; i < buffer_count[minor]; i++)
			{
				new_buffer[i] = buffer[minor][(buffer_tail[minor] + i) % old_size];
			}
		}

		kfree(buffer[minor]);
		buffer[minor] = new_buffer;
		buffer_size[minor] = new_size;
		buffer_head[minor] = buffer_count[minor] % new_size;
		buffer_tail[minor] = 0;
		up(&sem[minor]);

		if (buffer_count[minor] < new_size)
			wake_up(&write_queue[minor]);

		break;

	case MORSE_IOC_GET_BUFFER_SIZE:
		put_user(buffer_size[minor], (int *)arg);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

struct file_operations morse_ops = {
	write : morse_write,
	ioctl : morse_ioctl,
	open : morse_open,
	release : morse_release
};

int morse_init(void)
{
	int i;
	for (i = 0; i < DEVICES_COUNT; i++)
	{
		init_waitqueue(&write_queue[i]);
		device_in_use[i] = 0;
		buffer_size[i] = DEFAULT_BUFFER_SIZE;
		sem[i] = MUTEX;

		dot_duration[i] = DOT_DURATION;
		dash_duration[i] = DASH_DURATION;
		symbol_pause[i] = SYMBOL_PAUSE;
		letter_pause[i] = LETTER_PAUSE;
		word_pause[i] = WORD_PAUSE;
	}
	return register_chrdev(MORSE_MAJOR, "morse", &morse_ops);
}

int init_module()
{
	int result = morse_init();
	if (!result)
	{
		printk("Morse device initialized!\n");
	}

	return result;
}

void cleanup_module()
{
	unregister_chrdev(MORSE_MAJOR, "morse");
}