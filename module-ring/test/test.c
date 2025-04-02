#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
 
#define WR_VALUE _IOW(60, 1, int)
#define RD_VALUE _IOR(60, 2, int*)
 
int main()
{
        int fd;
        int value;
        int number;
 
	int err;
 
        printf("\nOpening Driver\n");
        fd = open("/dev/ring", O_WRONLY);
        if(fd < 0) {
                printf("Cannot open device file...\n");
                return 0;
        } 

        // printf("Reading Value from Driver\n");
        // ioctl(fd, RD_VALUE, (int*) &value);
        // printf("Value is %d\n", value);

        printf("Enter the Value to send\n");
        scanf("%d",&number);
        printf("Writing Value to Driver\n");
	
	err = ioctl(fd, WR_VALUE, (int) number); 
 
	if (err != 0){
	    printf("Error: %d \n", err);
	}
 
        printf("Closing Driver\n");
        close(fd);
}