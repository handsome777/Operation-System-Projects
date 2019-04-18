#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#define BUFFER_LENGTH 1024
char buffer[BUFFER_LENGTH];
int main(void){
    int fd;
    if ((fd = open("/dev/driver70",O_RDONLY))<0)
        {
            printf("open error\n");
            return 0;
        }
    while(1)
    {
        usleep(100000);
        printf("attempting to read from driver70 (testing block)\n");
        int length = read(fd,buffer,BUFFER_LENGTH-24);
        printf("%s (read from driver70 length %d)\n",buffer,length);
        memset(buffer,0,BUFFER_LENGTH*sizeof(char));
    }
    return 0;
}
