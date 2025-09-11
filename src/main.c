#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

const char* filename = "/dev/ina219_custom"; 

int main(){
    int fd = open(filename, O_RDONLY);
    if(fd == -1){
        err(1, "could not open file %s", filename);
        return 1;
    }

    while(1){
        // struct input_event evt;
        unsigned long val;
        ssize_t bytes = read(fd, &val, sizeof(val));
        if(bytes <= 0){
            printf("failed to read sensor data %zd \n", bytes);
            close(fd);
            return 1;
        }

        printf("data from sensor %lu\n", val);
        sleep(1);
    }

    close(fd);
    return 0;
}
