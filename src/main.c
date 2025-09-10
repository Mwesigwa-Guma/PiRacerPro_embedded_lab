#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

const char* filename = "/dev/ina219_custom"; 

int main(){
    int fd = open(filename, O_RDWR);
    if(fd == -1){
        err(1, "could not open file %s", filename);
        return 1;
    }

    while(1){
        // struct input_event evt;
        char buffer[128];
        ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
        if(bytes <= 0){
            printf("failed to read sensor data %zd \n", bytes);
            close(fd);
            return 1;
        }

        uint16_t shunt_voltage = buffer[0];
        shunt_voltage |= shunt_voltage << 8;
        shunt_voltage |= buffer[1];

        printf("data from sensor %u\n", (unsigned int)shunt_voltage);
        sleep(1);
    }

    close(fd);
    return 0;
}
