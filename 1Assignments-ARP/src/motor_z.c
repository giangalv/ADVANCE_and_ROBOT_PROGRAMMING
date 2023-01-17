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

// Flag to check if stop or reset handlers were called
int stop_flag = 0;
int reset_flag = 0;

// Constants to store the minimum and maximum x position
const float Z_MIN = 0.0;
const float Z_MAX = 20.0;

// File descriptor for the log file
int log_fd;

// File descriptors for pipes
int fd_vz, fdz_pos;

// Variables to store position and velocity
float z_pos = 0.0;
int vz = 0;

// Constant to set the abs of velocity of the end-effector
int const VELOCITY = 2;

// Buffer to store the log message
char log_buffer[100];

// Variable to store the errors
// 0 = no error
// 1 = system call error
// 2 = error while writing on log file
volatile int error = 0;

// Signal handlers
void myhandler(int signo);

// Function to write on log
int write_log(char *to_write, char type)
{
    // Log that in command_console process button has been pressed with date and time
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    // If type is 'e' then it is an error
    if (type == 'e')
    {
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <mz_process> error: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }
    // If type is 'i' then it is an info
    else if (type == 'i')
    {
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <mz_process> new speed: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }
    // If type is 's' then it is a signal
    else if (type == 's')
    {
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <mz_process> signal received: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }

    int m = write(log_fd, log_buffer, strlen(log_buffer));
    // Check for errors
    if (m == -1 || m != strlen(log_buffer))
    {
        return 2;
    }

    return 0;
}

// Function to handle the interrupt signal
void myhandler(int signo){
    if (signo == SIGUSR1){

        // Setting stop_flag to true
        stop_flag = 1;
        // Setting reset_flag to false
        reset_flag = 0;

        if (error = write_log("STOP", 's')){
            return;
        }

        vz = 0;
    }
    else if (signo == SIGUSR2){

        // Setting reset_flag to true
        reset_flag = 1;
        // Setting stop_flag to false
        stop_flag = 0;

        if (error = write_log("RESET", 's')){
            return;
        }
    }

    // listen for signals
    if (signal(SIGUSR1, myhandler) == SIG_ERR || signal(SIGUSR2, myhandler) == SIG_ERR){
        // If error occurs while setting the signal handler
        // Log the error
        error = write_log(strerror(errno), 'e');
        return;
    }
}

int main(int argc, char const *argv[])
{
    // variable to know if the velocity is changed: we confront directly this string with the one read from the pipe
    char * Vp = "i";
    char * Vm = "d";
    char * Vzero = "s";

    // Open the log file
    if ((log_fd = open("log/log_motor_z.txt", O_WRONLY | O_APPEND | O_CREAT, 0666)) == -1)
    {
        // If error occurs while opening the log file
        exit(errno);
    }

    // Create the FIFOs
    char *vz_fifo = "/tmp/velz";
    char *z_pos_fifo = "/tmp/z_world";
    mkfifo(vz_fifo, 0666);
    mkfifo(z_pos_fifo, 0666);

    // Open the FIFOs
    if ((fd_vz = open(vz_fifo, O_RDWR)) == -1) // O_RDWR is needed to avoid receiving EOF in select
    {
        // If error occurs while opening the FIFO
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close the log file
        close(log_fd);
        if (ret)
        {
            // If error occurs while writing to the log file
            exit(errno);
        }

        exit(1);
    }

    if ((fdz_pos = open(z_pos_fifo, O_WRONLY)) == -1)
    {
        // If error occurs while opening the FIFO
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close file descriptors
        close(fd_vz);
        close(log_fd);
        if (ret)
        {
            // If error occurs while writing to the log file
            exit(errno);
        }

        exit(1);
    }

    // Listen for signals
    if (signal(SIGUSR1, myhandler) == SIG_ERR || signal(SIGUSR2, myhandler) == SIG_ERR)
    {
        // If error occurs while setting the signal handlers
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close file descriptors
        close(fd_vz);
        close(fdz_pos);
        close(log_fd);
        if (ret)
        {
            // If error occurs while writing to the log file
            exit(errno);
        }

        exit(1);
    }

    // Loop until handler_error is set to true
    while (!error)
    {
        // Unique flag for reset and stop
        int flag = 0;

        // Boolean to check if the position has changed
        int pos_changed = 0;

        // Set the file descriptors to be monitored
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd_vz, &readfds);

        // Set the timeout
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

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

            str[nbytes - 1] = '\0';

            // Check the meaning of the pipe and change the velocity accordingly
            if (strcmp(str, Vp) == 0){
                vz += 1;
                if (vz > VELOCITY){
                    vz = +VELOCITY;
                }
            } 
            else if (strcmp(str, Vm) == 0){
                vz -= 1;
                if (vz < -VELOCITY){
                    vz = -VELOCITY;
                }
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
        float delta_z = vz * 0.5;
        float new_z_pos = z_pos + delta_z;

        // Check if the position is out of bounds
        if (new_z_pos < Z_MIN)
        {
            new_z_pos = Z_MIN;

            // Stop the motor
            vz = 0;
        }
        else if (new_z_pos > Z_MAX)
        {
            new_z_pos = Z_MAX;

            // Stop the motor
            vz = 0;
        }

        // Check if the position has changed
        if (new_z_pos != z_pos)
        {
            // Update the position
            z_pos = new_z_pos;
            pos_changed = 1;
        }

        // Write the position to the FIFO if it has changed
        if (pos_changed)
        {
            // Write the position
            char z_pos_str[10];
            sprintf(z_pos_str, "%f", z_pos);

            int m = write(fdz_pos, z_pos_str, strlen(z_pos_str) + 1);
            if (m == -1 || m != strlen(z_pos_str) + 1)
            {
                // If error occurs while writing to the FIFO
                error = 1;
                break;
            }
        }
        // If reset signal was received,
        if (reset_flag)
        {
            // Reset the position
            while (z_pos > Z_MIN)
            {
                vz = -VELOCITY;
                z_pos += vz * 0.5;

                // Write the position
                char z_pos_str[10];
                sprintf(z_pos_str, "%f", z_pos);

                int m = write(fdz_pos, z_pos_str, strlen(z_pos_str) + 1);
                if (m == -1 || m != strlen(z_pos_str) + 1)
                {
                    // If error occurs while writing to the FIFO
                    error = 1;
                    break;
                }
            }
            
            vz = 0;
            z_pos = 0.0;
            // Skip writing to the FIFO
            continue;
        }

        // If stop signal was received
        if (stop_flag)
        {
            vz = 0;
            // Skip writing to the FIFO
            continue;
        }
    }

    // Close the FIFOs
    close(fd_vz);
    close(fdz_pos);

    if (error == 1)
    {
        // If error occurs while reading the position
        // Log the error
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
