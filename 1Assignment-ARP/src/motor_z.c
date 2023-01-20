// AUTHOR: Demaria Claudio & Galvagni Gianluca
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>

// Unique flag for RESET and STOP
// 0 = no flag
// 1 = reset flag
// 2 = stop flag
int flag = 0;

// Variable to store the errors
// 0 = no error
// 1 = system call error
// 2 = error while writing on log file
volatile int error = 0;

// Constant to store the minimum x position
const float Z_MIN = 0.0;
// Constant to store the maximum x position
const float Z_MAX = 9.0;
// Constant to store the abs velocity
int const VELOCITY = 2;

// File descriptor for log file
int log_fd;
// File descriptor for pipes
int fd_vz, fdz_pos;

// Variable to store position
float z_pos = 0.0;
// Variable to store velocity
int vz = 0;

// Buffer to store the log message
char log_buffer[100];
// Function to write on log
int write_log(char *to_write, char type){
    // Log that in command_console process button has been pressed with time
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    // Create a string for the type of log
    char log_type[30];
    if (type == 'e'){
        strcpy(log_type, "error");
    }else if (type == 'i'){
        strcpy(log_type, "info");
    }else if (type == 's'){
        strcpy(log_type, "signal received");
    }else if (type == 'c'){
        strcpy(log_type, "velocity change");
    }else if (type == 'm'){
        strcpy(log_type, "velocity bounds");
    }

    // Use snprintf to format the log message
    snprintf(log_buffer, sizeof(log_buffer), "%d:%d:%d: Motor Z process %s: %s\n", tm.tm_hour, tm.tm_min, tm.tm_sec, log_type, to_write);

    // Write and check for errors
    if ((write(log_fd, log_buffer, strlen(log_buffer))) == -1){
        return 2;
    }

    return 0;
}


// Function to handle the interrupt signal
void myhandler(int signo){
    if (signo == SIGUSR1){
        // Setting FLAG for STOP the motor
        flag = 2;
        
        if (error = write_log("STOP", 's')){
            return;
        }
    }
    else if (signo == SIGUSR2){
        // Setting FLAG for RESET the motor
        flag = 1;

        if (error = write_log("RESET", 's')){
            return;
        }
    }

    // listen for signals
    if (signal(SIGUSR1, myhandler) == SIG_ERR || signal(SIGUSR2, myhandler) == SIG_ERR){
        // If error occurs while setting the signal handler, log the error
        error = write_log(strerror(errno), 'e');
        return;
    }
}

// Function to reset the machine after a signal
void reset_for_signal(int flag_sig){
    // If 'reset' signal, reset the position and velocity
    if (flag_sig == 1){
        flag = 0;
        vz = 0;
        z_pos = 0.0;
    }
    // If 'stop' signal, stop the motor
    else if (flag_sig == 2){
        flag = 0;
        vz = 0;
    }
}

// Function to check max e min velocity
int check_velocity(int velocity){
    if (velocity > VELOCITY){
        // Write on log that the velocity is max
        char write_to[100] = "MAXIMUM SPEED";
        int ret = write_log(write_to, 'm');
        return VELOCITY;
    }
    else if (velocity < -VELOCITY){
        // Write on log that the velocity is min
        char write_to[100] = "MINIMUM SPEED";
        int ret = write_log(write_to, 'm');
        return -VELOCITY;
    }
    else{
        return velocity;
    }
}

// Function to check max e min position
float check_position(float position){
    if (position > Z_MAX){
        vz = 0;
        return Z_MAX;
    }
    else if (position < Z_MIN){
        vz = 0;
        return Z_MIN;
    }
    else{
        return position;
    }
}

// Function to open a file and log the error
int open_and_log_on_error(const char* file, int flags, const char* operation){
    int fd = open(file, flags);
    if (fd == -1){
        int ret = write_log(strerror(errno), 'e');
        close(log_fd);
        if (ret){
            exit(errno);
        }
        exit(1);
    }
    return fd;
}

// Function to signal and log the error
void signal_and_log_on_error(int signum, void (*handler)(int), const char* operation){
    if (signal(signum, handler) == SIG_ERR ){
        int ret = write_log(strerror(errno), 'e');
        close(log_fd);
        close(fd_vz);
        close(fdz_pos);
        if (ret){
            exit(errno);
        }
        exit(1);
    }
}

int main(int argc, char const *argv[]){
    // variable to know if the velocity is changed: we confront directly this string with the one read from the pipe
    char * Vp = "i";
    char * Vm = "d";
    char * Vzero = "s";

    // Open the log file
    if ((log_fd = open("log/log_motor_z.txt", O_WRONLY | O_APPEND | O_CREAT, 0666)) == -1){
        // If error occurs while opening the log file
        exit(errno);
    }

    // Create the FIFOs
    char *vz_fifo = "/tmp/velz";
    char *z_pos_fifo = "/tmp/z_world";
    mkfifo(vz_fifo, 0666);
    mkfifo(z_pos_fifo, 0666);

    // Open the FIFOs
    int fd_vz = open_and_log_on_error(vz_fifo, O_RDWR, "opening vz_fifo");
    int fdz_pos = open_and_log_on_error(z_pos_fifo, O_WRONLY, "opening z_pos_fifo");
    // Set the signal handlers
    signal_and_log_on_error(SIGUSR1, myhandler, "setting signal handler for SIGUSR1");
    signal_and_log_on_error(SIGUSR2, myhandler, "setting signal handler for SIGUSR2");

    // Variable to change the velocity/position
    float delta_z;
    float new_z_pos;
    // Loop until handler_error is set to true
    while (!error){
        // Set the file descriptors to be monitored
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd_vz, &readfds);

        // Set the timeout
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100 ms

        // Wait for the file descriptor to be ready
        int ready = select(fd_vz + 1, &readfds, NULL, NULL, &timeout);

        // Check if the file descriptor is ready
        if (ready > 0)
        {
            // Read the velocity increment
            char str[2];
            int nbytes;
            if ((nbytes = read(fd_vz, str, 2)) == -1)
            {
                // If error occurs while reading from the FIFO
                error = 1;
                break;
            }
            // Need to add the null character at the end of the string to be able to compare it
            str[nbytes - 1] = '\0';

            // Check the meaning of the pipe and change the velocity accordingly
            if (strcmp(str, Vp) == 0){
                vz += 1;
                vz = check_velocity(vz);
                // If velocity is changed, log the change
                char write_to[100] = "SPEED INCREASING";
                int ret = write_log(write_to, 'c');
            } 
            else if (strcmp(str, Vm) == 0){
                vz -= 1;
                vz = check_velocity(vz);
                // If velocity is changed, log the change
                char write_to[100] = "SPEED DECREASING";
                int ret = write_log(write_to, 'c');
            } 
            else if (strcmp(str, Vzero) == 0){
                vz = 0;
            }
            else{
                // If the string is not recognized
                error = 1;
                break;
            }
        }
        // Error handling
        else if (ready < 0 && errno != EINTR)
        {
            // If error occurs while waiting for the file descriptor to be ready
            error = 1;
            break;
        }

        // Update the position
        delta_z = vz * 0.25;
        new_z_pos = z_pos + delta_z;

        // Check if the position is out of bounds
        new_z_pos = check_position(new_z_pos);

        // Check if the position has changed
        if (new_z_pos != z_pos)
        {
            // Update the position
            z_pos = new_z_pos;
            // Write the position
            char z_pos_str[10];
            sprintf(z_pos_str, "%f", z_pos);
            if ((write(fdz_pos, z_pos_str, strlen(z_pos_str) + 1)) == -1)
            {
                // If error occurs while writing to the FIFO
                error = 1;
                break;
            }
        }

        // If reset signal was received,
        if (flag == 1){
            // char to send to the FIFO
            char z_pos_str[10];
            // Reset the position
            while (z_pos > Z_MIN){
                vz = -VELOCITY;
                z_pos += vz * 0.1;
                // do not allow the position to go below X_MIN 
                // we do not want to write to the FIFO
                if (z_pos < Z_MIN){
                    break;
                }

                // Write the position
                sprintf(z_pos_str, "%f", z_pos);
                if ((write(fdz_pos, z_pos_str, strlen(z_pos_str) + 1)) == -1){
                    // If error occurs while writing to the FIFO
                    error = 1;
                    break;
                }

                // Wait for 0.6 seconds to simulate the motor
                sleep(0.6);
            }

            // Reset the machine and write to the pipe
            reset_for_signal(flag);
            // Write the position
            sprintf(z_pos_str, "%f", z_pos);
            if ((write(fdz_pos, z_pos_str, strlen(z_pos_str) + 1)) == -1){
                // If error occurs while writing to the FIFO
                error = 1;
                break;
            }
        }

        // If stop signal was received
        if (flag == 2){
            // Reset the machine
            reset_for_signal(flag);
        }
    }

    // Close the FIFOs
    close(fd_vz);
    close(fdz_pos);

    if (error == 1)
    {
        // If error occurs while reading the position, log the error
        int ret = write_log(strerror(errno), 'e');
        // Close the log file
        close(log_fd);
        if (ret)
        {
            // If error occurs while writing on log file
            exit(errno);
        }
        exit(1);
    }
    
    // Close the log file
    close(log_fd);

    if(error == 2){
        // If error occurs while writing on log file
        exit(errno);
    }

    exit(0);
}
