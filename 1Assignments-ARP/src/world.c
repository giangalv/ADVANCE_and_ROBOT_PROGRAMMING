// AUTHOR: Dmaria Claudio & Galvagni Gianluca
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

// Define random error
const float RANDOM_ERROR = 0.003; // 0.3%

// Define maximum and minimum positions
const float MIN_X_POS = 0.0;
const float MAX_X_POS = 40.0;
const float MIN_Z_POS = 0.0;
const float MAX_Z_POS = 10.0;

// Buffer to store the log message
char log_buffer[100];

// File descriptor for log file
int log_fd;

// Variable to store the error
volatile int error = 0;

// Function to write on log file
int write_log(char *to_write, char type){
    // Log that in command_console process button has been pressed with date and time
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    // If type is 'e' write error
    if (type == 'e'){
        sprintf(log_buffer, "%d:%d:%d: WORLD proces error: %s\n", tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }
    // If type is 'b' write button pressed
    else if (type == 'i'){
        sprintf(log_buffer, "%d:%d:%d: WORLD preces position: %s\n", tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }

    // Write on log file
    int m = write(log_fd, log_buffer, strlen(log_buffer));

    // Check for errors
    if (m == -1 || m != strlen(log_buffer)){
        return 2; // Error writing on log file
    }

    return 0; // Success
}

// Function to return maximum of two integers
int max(int a, int b){
    return (a > b) ? a : b;
}

// Function to randomly get a number between two integers
int random_between(int a, int b){
    return (rand() % (b - a + 1)) + a;
}

// Function to pick a random number between two integers
int pick_random(int a, int b){
    int num = random_between(a, b);

    // If num is closer to a, return a
    if (num - a < b - num){
        return a;
    }
    // If num is closer to b, return b
    else{
        return b;
    }
}

// Function to add the random error to the position
float add_error(float pos){
    // Calculate the error
    float error = pos * RANDOM_ERROR;

    // Calculate the minimum and maximum values
    float min = pos - error;
    float max = pos + error;

    // Return a random value between the minimum and maximum
    return (float)random_between((int)(min * 100), (int)(max * 100)) / 100; // Multiply by 100 to avoid floating point errors
}

// Function to read and return the real position
float read_real_pos(int *fd, char axis){
    // Variable to store the position
    char pos[10];

    // Read the position from the file
    if (read(*fd, pos, 10) == -1){
        // If error return -1.0
        return -1.0; // return a float to avoid errors
    }

    // Store the value in the real position variable and add the random error
    float real_pos = add_error(atof(pos));

    // Check if the position is out of bounds
    if (axis == 'x'){

        if (real_pos < MIN_X_POS){
            real_pos = MIN_X_POS;
        }
        else if (real_pos > MAX_X_POS){
            real_pos = MAX_X_POS;
        }
    }
    else if (axis == 'z'){

        if (real_pos < MIN_Z_POS){
            real_pos = MIN_Z_POS;
        }
        else if (real_pos > MAX_Z_POS){
            real_pos = MAX_Z_POS;
        }
    }

    // Return the position
    return real_pos;
}

int main(int argc, char const *argv[]){
    // Open log file
    if ((log_fd = open("log/log_world.txt", O_WRONLY | O_APPEND | O_CREAT, 0666)) == -1){
        // If error occurs while opening the log file
        exit(errno);
    }

    // Variables to store the real values of the positions
    float real_x_pos = 0.0;
    float real_z_pos = 0.0;

    // File descriptors
    int fdx_pos, fdz_pos, fd_inspx, fd_inspz;

    // FIFOs locations
    char *x_pos_fifo = "/tmp/x_world";
    char *z_pos_fifo = "/tmp/z_world";
	char * fifo_inspx = "/tmp/inspx";
	char * fifo_inspz = "/tmp/inspz";

    // Create the FIFOs
    mkfifo(x_pos_fifo, 0666);
    mkfifo(z_pos_fifo, 0666);
	mkfifo(fifo_inspx, 0666);
	mkfifo(fifo_inspz, 0666);

    // Open the FIFOs in O_RDWR mode to avoid receiving EOF
    if ((fdx_pos = open(x_pos_fifo, O_RDWR)) == -1){
        // If error occurs while opening the FIFO
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close the log file
        close(log_fd);
        if (ret){

            // If error occurs while writing on log file
            exit(errno);
        }
        exit(1);
    }

    if ((fdz_pos = open(z_pos_fifo, O_RDWR)) == -1){
        // If error occurs while opening the FIFO
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close the file descriptors
        close(fdx_pos);
        close(log_fd);
        if (ret){
            // If error occurs while writing on log file
            exit(errno);
        }
        exit(1);
    }

    if((fd_inspx = open(fifo_inspx, O_WRONLY)) == -1){
        // If error occurs while opening the FIFO
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close the file descriptors
        close(fdx_pos);
        close(fdz_pos);
        close(log_fd);
        if (ret){
            // If error occurs while writing on log file
            exit(errno);
        }
        exit(1);
    }

    if((fd_inspz = open(fifo_inspz, O_WRONLY)) == -1){
        // If error occurs while opening the FIFO
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close the file descriptors
        close(fdx_pos);
        close(fdz_pos);
        close(log_fd);
        if (ret){
            // If error occurs while writing on log file
            exit(errno);
        }
        exit(1);
    }

    // Variable to store the number of loops
    int loops = 0;

    // Infinite loop
    while (1){
        // Variable to store the read values
        char x_pos[10];
        char z_pos[10];

        // Setting parameters for select function
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fdx_pos, &readfds);
        FD_SET(fdz_pos, &readfds);
        int max_fd = max(fdx_pos, fdz_pos) + 1;

        // Setting timeout for select function
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 250000;

        // Wait for the file descriptor to be ready
        int ready = select(max_fd, &readfds, NULL, NULL, &timeout);

        // Check if the file descriptor is ready
        if (ready < 0){
            // If error occurs while waiting for the file descriptor
            error = 1;
            break;
        }
        else if (ready > 0){
            // Increment loops
            loops++;

            // If both file descriptors are ready
            if (FD_ISSET(fdx_pos, &readfds) && FD_ISSET(fdz_pos, &readfds)){

                // Randomly select one of the file descriptors
                if (pick_random(fdx_pos, fdz_pos) == fdx_pos){

                    // Read and store the value in the x position variable
                    real_x_pos = read_real_pos(&fdx_pos, 'x');
                }
                else{

                    // Read and store the value in the z position variable
                    real_z_pos = read_real_pos(&fdz_pos, 'z');
                }
            }
            // If only the x position file descriptor is ready
            else if (FD_ISSET(fdx_pos, &readfds)){

                // Read and store the value in the x position variable
                real_x_pos = read_real_pos(&fdx_pos, 'x');
            }
            // If only the z position file descriptor is ready
            else if (FD_ISSET(fdz_pos, &readfds)){

                // Read and store the value in the z position variable
                real_z_pos = read_real_pos(&fdz_pos, 'z');
            }

            // If error occurs while reading the position
            if (real_x_pos == -1.0 || real_z_pos == -1.0){

                error = 1;
                break;
            }
            
            // print the position of the end-effector
		    printf("x: %f --- z: %f\n", real_x_pos, real_z_pos);
		    fflush(stdout);

            char x_pos[20];
            char z_pos[20];
            sprintf(x_pos, "%f", real_x_pos);
            sprintf(z_pos, "%f", real_z_pos);

            // Write the real position to the FIFO
            int m = write(fd_inspx, x_pos, strlen(x_pos));
            if (m == -1 || m != strlen(x_pos)){
                // If error occurs while writing on the FIFO
                error = 1;
                break;
            }
            int n = write(fd_inspz, z_pos, strlen(z_pos));
            if (n == -1 || n != strlen(z_pos)){
                // If error occurs while writing on the FIFO
                error = 1;
                break;
            }
        }
    }

    // Close the FIFOs
    close(fdx_pos);
    close(fdz_pos);
    close(fd_inspx);
    close(fd_inspz);

    if (error == 1){
        // If error occurs while reading the position
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close the log file
        close(log_fd);
        if (ret){
            // If error occurs while writing on log file
            exit(errno);
        }
        exit(1);
    }

    // Close the log file
    close(log_fd);

    if (error == 2){
        // If error occurs while writing on log file
        exit(errno);
    }

    exit(0);
}
