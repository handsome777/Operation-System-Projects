#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
char test_str[50]="handsome777 is the most handsome man in the world";

int main(void){
    int fd;
    if ((fd=open("/dev/driver70",O_RDWR))<0)
        {
            printf("open has an error \n");
            return -1;
        }
    while(1)
    {
        printf("write once(test block)\n");
        usleep(500000);
        write(fd,test_str,49);
    }
    return 0;
}
