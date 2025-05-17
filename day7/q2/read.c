
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char * argv[]) {
 char buf[32];
if(argc != 2) {
        printf("syntax: %s <devfile> <string>\n", argv[0]);
        return 1;
    }
int ret;
   
    int fd = open(argv[1], O_RDONLY);
    if(fd < 0) {
        perror("open() failed");
        _exit(1);
    }
    printf("open() done: %d\n", fd);
    memset(buf, 0, sizeof(buf));
    ret = read(fd, buf, sizeof(buf));
    printf("write() done: %d - %s\n", ret,buf); 

   
    close(fd);
    
    return 0;
}