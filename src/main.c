#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

const char* filename = "/dev/ina219_custom"; 
const double v_min = 3.0;
const double v_max = 4.2;

int main(){
    int fd = open(filename, O_RDONLY);
    if(fd == -1){
        err(1, "could not open file %s", filename);
        return 1;
    }

    while(1){
        // struct input_event evt;
        unsigned long busv;
        int num_cells = 1;

        ssize_t bytes = read(fd, &busv, sizeof(busv));
        if(bytes <= 0){
            printf("failed to read sensor data %zd \n", bytes);
            close(fd);
            return 1;
        }

        double v_cell = (double)busv/num_cells / 1000.0; 
        double soc = (v_cell - v_min)/(v_max - v_max)*100.0;

        if(soc < 0.0) soc = 0.0;
        if(soc > 100) soc = 100.0;

        printf("Battery SoC: %.1f %\n", soc);
        sleep(1);
    }

    close(fd);
    return 0;
}
