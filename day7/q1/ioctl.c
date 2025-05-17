
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include "ioctl.h"
#include <stdlib.h>
int main(int argc, char *argv[]) {
    int ret, fd,a;
   
   if((strcmp(argv[2], "resize")) == 0)
   {
      a=atoi(argv[3]);
   if(argc != 4) {
        printf("syntax: %s <devfile> <command>\n", argv[0]);
        return 1;
    }
   }
   else{
   if(argc != 3) {
        printf("syntax: %s <devfile> <command>\n", argv[0]);
        return 1;
    }
}
   
    fd = open(argv[1], O_RDWR);
    if(fd < 0) {
        perror("open() failed");
        _exit(1);
    }
   
    if(strcmp(argv[2], "clear") == 0) {
        ioctl(fd, FIFO_CLEAR);
        printf("fifo clear command sent.\n");
    }
    
    else if(strcmp(argv[2], "info") == 0) {
        fifo_info_t info;
        ioctl(fd, FIFO_GET_INFO, &info);
        printf("%s info: size = %d, len = %d, avail = %d\n", argv[1], info.size, info.len, info.avail);
    }
    else if(strcmp(argv[2], "resize") == 0) {
        fifo_info_t info;
        ioctl(fd, FIFO_RESIZE, a);
       
    }

   
    else
        printf("invalid command: %s\n", argv[2]);
    close(fd);
    return 0;
}