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
#include <linux/tty.h>
#include <linux/kd.h>
#include <linux/console.h>
#include <linux/vt.h>

#define MORSE_MAJOR 61
#define DEVICES_COUNT 8
#define DEFAULT_BUFFER_SIZE 256
#define MIN_BUFFER_SIZE 0
#define MAX_BUFFER_SIZE 1024

#define DOT_DURATION 200  // milliseconds
#define DASH_DURATION 600 // milliseconds
#define SYMBOL_PAUSE 200  // milliseconds
#define LETTER_PAUSE 600  // milliseconds
#define WORD_PAUSE 1400	  // milliseconds

// IOCTL commands
#define MORSE_IOC_SET_DOT_DURATION _IOW(MORSE_MAJOR, 1, int)
#define MORSE_IOC_SET_DASH_DURATION _IOW(MORSE_MAJOR, 2, int)
#define MORSE_IOC_SET_SYMBOL_PAUSE _IOW(MORSE_MAJOR, 3, int)
#define MORSE_IOC_SET_LETTER_PAUSE _IOW(MORSE_MAJOR, 4, int)
#define MORSE_IOC_SET_WORD_PAUSE _IOW(MORSE_MAJOR, 5, int)
#define MORSE_IOC_SET_BUFFER_SIZE _IOW(MORSE_MAJOR, 6, int)
#define MORSE_IOC_GET_BUFFER_SIZE _IOR(MORSE_MAJOR, 7, int *)

// Morse code patterns (. = dot, - = dash)
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

// Helper function to get the minor device number
static int get_minor(struct inode *inode)
{
	int minor = MINOR(inode->i_rdev);
	if (minor >= DEVICES_COUNT)
	{
		return -ENODEV;
	}
	return minor;
}

// Function to set console color for signaling
static void set_signal(int minor, int state)
{
	struct tty_struct *tty;
	struct console *con;

	// Get current console
	tty = current->tty;
	if (tty && tty->driver.type == TTY_DRIVER_TYPE_CONSOLE)
	{
		if (state)
		{
			// Signal ON - set bright white on red background
			tty->driver.write(tty, 0, "\033[1;37;41m \033[0m", 13);
		}
		else
		{
			// Signal OFF - restore normal color and space
			tty->driver.write(tty, 0, "\033[0m ", 5);
		}
		// Move cursor back to beginning
		tty->driver.write(tty, 0, "\r", 1);
	}

	signal_state[minor] = state;
}

// Timer function to handle Morse code transmission
static void morse_timer_function(unsigned long data)
{
	int minor = (int)data;

	// If we're done with the current character's code
	if (current_code[minor] == NULL || *current_code[minor] == '\0')
	{
		// Move to the next character in the buffer
		if (buffer_count[minor] > 0)
		{
			current_char[minor] = buffer[minor][buffer_tail[minor]];
			buffer_tail[minor] = (buffer_tail[minor] + 1) % buffer_size[minor];
			buffer_count[minor]--;

			// Convert character to Morse code
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
				// Space - special case for word pause
				set_signal(minor, 0);
				morse_timer[minor].expires = jiffies + (word_pause[minor] * HZ / 1000);
				add_timer(&morse_timer[minor]);
				return;
			}
			else
			{
				// Ignore other characters and process the next one immediately
				morse_timer[minor].expires = jiffies + 1;
				add_timer(&morse_timer[minor]);
				return;
			}

			code_position[minor] = 0;

			// Start with the first symbol
			if (current_code[minor][0] == '.')
			{
				set_signal(minor, 1);
				morse_timer[minor].expires = jiffies + (dot_duration[minor] * HZ / 1000);
			}
			else
			{ // must be '-'
				set_signal(minor, 1);
				morse_timer[minor].expires = jiffies + (dash_duration[minor] * HZ / 1000);
			}
			code_position[minor]++;
		}
		else
		{
			// Buffer is empty, transmission complete
			is_transmitting[minor] = 0;
			set_signal(minor, 0);
			return;
		}
	}
	else
	{
		// We're in the middle of transmitting a character
		if (signal_state[minor])
		{
			// Signal was on, turn it off and wait for symbol pause
			set_signal(minor, 0);
			morse_timer[minor].expires = jiffies + (symbol_pause[minor] * HZ / 1000);
		}
		else
		{
			// Signal was off
			if (current_code[minor][code_position[minor]] == '\0')
			{
				// End of character, wait for letter pause
				morse_timer[minor].expires = jiffies + (letter_pause[minor] * HZ / 1000);
				current_code[minor] = NULL; // Marks end of current character
			}
			else if (current_code[minor][code_position[minor]] == '.')
			{
				// Send a dot
				set_signal(minor, 1);
				morse_timer[minor].expires = jiffies + (dot_duration[minor] * HZ / 1000);
				code_position[minor]++;
			}
			else
			{ // must be '-'
				// Send a dash
				set_signal(minor, 1);
				morse_timer[minor].expires = jiffies + (dash_duration[minor] * HZ / 1000);
				code_position[minor]++;
			}
		}
	}

	add_timer(&morse_timer[minor]);
}

// Open the device
static int morse_open(struct inode *inode, struct file *file)
{
	int minor = get_minor(inode);
	if (minor < 0)
	{
		return minor;
	}

	down(&sem[minor]);


	if (device_in_use[minor] == 0)
	{
		// Initialize the device on first open
		buffer[minor] = kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
		if (buffer[minor] == NULL)
		{
			up(&sem[minor]);
			return -ENOMEM;
		}

		buffer_count[minor] = 0;
		buffer_head[minor] = 0;
		buffer_tail[minor] = 0;

		// Set default timings
		dot_duration[minor] = DOT_DURATION;
		dash_duration[minor] = DASH_DURATION;
		symbol_pause[minor] = SYMBOL_PAUSE;
		letter_pause[minor] = LETTER_PAUSE;
		word_pause[minor] = WORD_PAUSE;

		is_transmitting[minor] = 0;
		signal_state[minor] = 0;
	}

	device_in_use[minor]++;
	up(&sem[minor]);

	return 0;
}

// Close the device
static void morse_release(struct inode *inode, struct file *file)
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
	}

	up(&sem[minor]);
	MOD_DEC_USE_COUNT;
}

// Write to the device
static int morse_write(struct inode *inode, struct file *file, const char *buf, int count)
{
	int minor = get_minor(inode);
	int i, bytes_written = 0;
	char ch;

	if (minor < 0)
	{
		return minor;
	}

	down(&sem[minor]);

	for (i = 0; i < count; i++)
	{
		// If buffer is full, we can't write more
		if (buffer_count[minor] >= buffer_size[minor])
		{
			break;
		}

		// Get user space character
		ch = get_user(buf + i);

		// Store in buffer
		buffer[minor][buffer_head[minor]] = ch;
		buffer_head[minor] = (buffer_head[minor] + 1) % buffer_size[minor];
		buffer_count[minor]++;
		bytes_written++;
	}

	// Start transmission if not already going
	if (!is_transmitting[minor] && buffer_count[minor] > 0)
	{
		is_transmitting[minor] = 1;

		// Initialize and start the timer
		init_timer(&morse_timer[minor]);
		morse_timer[minor].function = morse_timer_function;
		morse_timer[minor].data = minor;
		morse_timer[minor].expires = jiffies + 1;
		add_timer(&morse_timer[minor]);
	}

	up(&sem[minor]);

	return bytes_written;
}

// Read is not supported since this is a write-only device
static int morse_read(struct inode *inode, struct file *file, char *buf, int count)
{
	return -EINVAL; // Operation not permitted
}

// IOCTL to control timing and buffer size
static int morse_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int minor = get_minor(inode);
	int value, err;
	char *new_buffer;
	int i, old_size, new_size;

	if (minor < 0)
	{
		return minor;
	}

	// Verify user space for read operations
	if (_IOC_DIR(cmd) & _IOC_READ)
	{
		int len = _IOC_SIZE(cmd);
		if ((err = verify_area(VERIFY_WRITE, (void *)arg, len)) < 0)
			return err;
	}

	down(&sem[minor]);

	switch (cmd)
	{
	case MORSE_IOC_SET_DOT_DURATION:
		value = (int)arg;
		if (value <= 0)
		{
			up(&sem[minor]);
			return -EINVAL;
		}
		dot_duration[minor] = value;
		break;

	case MORSE_IOC_SET_DASH_DURATION:
		value = (int)arg;
		if (value <= 0)
		{
			up(&sem[minor]);
			return -EINVAL;
		}
		dash_duration[minor] = value;
		break;

	case MORSE_IOC_SET_SYMBOL_PAUSE:
		value = (int)arg;
		if (value <= 0)
		{
			up(&sem[minor]);
			return -EINVAL;
		}
		symbol_pause[minor] = value;
		break;

	case MORSE_IOC_SET_LETTER_PAUSE:
		value = (int)arg;
		if (value <= 0)
		{
			up(&sem[minor]);
			return -EINVAL;
		}
		letter_pause[minor] = value;
		break;

	case MORSE_IOC_SET_WORD_PAUSE:
		value = (int)arg;
		if (value <= 0)
		{
			up(&sem[minor]);
			return -EINVAL;
		}
		word_pause[minor] = value;
		break;

	case MORSE_IOC_SET_BUFFER_SIZE:
		new_size = (int)arg;
		if (new_size < MIN_BUFFER_SIZE || new_size > MAX_BUFFER_SIZE)
		{
			up(&sem[minor]);
			return -EINVAL;
		}

		if (new_size == buffer_size[minor])
		{
			up(&sem[minor]);
			return 0;
		}

		if (new_size < buffer_count[minor])
		{
			up(&sem[minor]);
			return -EBUSY;
		}

		// Allocate new buffer
		if (new_size > 0)
		{
			new_buffer = kmalloc(new_size, GFP_KERNEL);
			if (new_buffer == NULL)
			{
				up(&sem[minor]);
				return -ENOMEM;
			}
		}
		else
		{
			new_buffer = NULL;
		}

		// Copy data to new buffer
		old_size = buffer_size[minor];
		if (new_size > 0 && buffer_count[minor] > 0)
		{
			for (i = 0; i < buffer_count[minor]; i++)
			{
				new_buffer[i] = buffer[minor][(buffer_tail[minor] + i) % old_size];
			}
		}

		// Update buffer pointers
		kfree(buffer[minor]);
		buffer[minor] = new_buffer;
		buffer_size[minor] = new_size;
		buffer_head[minor] = buffer_count[minor] % new_size;
		buffer_tail[minor] = 0;

		break;

	case MORSE_IOC_GET_BUFFER_SIZE:
		put_user(buffer_size[minor], (int *)arg);
		break;

	default:
		up(&sem[minor]);
		return -EINVAL;
	}

	up(&sem[minor]);
	return 0;
}

static struct file_operations morse_fops = {
	read : morse_read,
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
		device_in_use[i] = 0;
		buffer_size[i] = DEFAULT_BUFFER_SIZE;
		sem[i] = MUTEX;
	}
    return register_chrdev(MORSE_MAJOR, "morse", &morse_fops);
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

void cleanup_module(){
    unregister_chrdev(MORSE_MAJOR, "morse");
}