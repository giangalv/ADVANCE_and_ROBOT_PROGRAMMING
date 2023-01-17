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
const float X_MIN = 0.0;
const float X_MAX = 40.0;

// File descriptor for the log file
int log_fd;

// File descriptors for pipes
int fd_vx, fdx_pos;

// Variables to store position and velocity
float x_pos = 0.0;
int vx = 0;

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
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <mx_process> error: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }
    // If type is 'i' then it is an info
    else if (type == 'i')
    {
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <mx_process> new speed: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }
    // If type is 's' then it is a signal
    else if (type == 's')
    {
        sprintf(log_buffer, "%d-%d-%d %d:%d:%d: <mx_process> signal received: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, to_write);
    }

    int m = write(log_fd, log_buffer, strlen(log_buffer));
    // Check for errors
    if (m == -1 || m != strlen(log_buffer))
    {
        return 2;
    }

    return 0;
}

// Stop signal handler
void myhandler(int signo){
    if (signo == SIGUSR1){

        // Setting stop_flag to true
        stop_flag = 1;
        // Setting reset_flag to false
        reset_flag = 0;

        if (error = write_log("STOP", 's')){
            return;
        }

        vx = 0;
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
    if ((log_fd = open("log/log_motor_x.txt", O_WRONLY | O_APPEND | O_CREAT, 0666)) == -1)
    {
        // If error occurs while opening the log file
        exit(errno);
    }

    // Create the FIFOs
    char *vx_fifo = "/tmp/velx";
    char *x_pos_fifo = "/tmp/x_world";
    mkfifo(vx_fifo, 0666);
    mkfifo(x_pos_fifo, 0666);

    // Open the FIFOs
    if ((fd_vx = open(vx_fifo, O_RDWR)) == -1) // O_RDWR is needed to avoid receiving EOF in select
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

    if ((fdx_pos = open(x_pos_fifo, O_WRONLY)) == -1)
    {
        // If error occurs while opening the FIFO
        // Log the error
        int ret = write_log(strerror(errno), 'e');
        // Close file descriptors
        close(fd_vx);
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
        close(fd_vx);
        close(fdx_pos);
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
        // Setting signal falgs to false
        stop_flag = 0;
        reset_flag = 0;

        // Boolean to check if the position has changed
        int pos_changed = 0;

        // Set the file descriptors to be monitored
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd_vx, &readfds);

        // Set the timeout
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        // Wait for the file descriptor to be ready
        int ready = select(fd_vx + 1, &readfds, NULL, NULL, &timeout);

        // Check if the file descriptor is ready
        if (ready > 0)
        {
            // Read the velocity increment
            char str[2];
            int nbytes;
            if ((nbytes = read(fd_vx, str, 2)) == -1)
            {
                // If error occurs while reading from the FIFO
                error = 1;
                break;
            }

            str[nbytes - 1] = '\0';

            
            if (strcmp(str, Vp) == 0){
                vx += 1;
                if (vx > VELOCITY){
                    vx = +VELOCITY;
                }
            } 
            else if (strcmp(str, Vm) == 0){
                vx -= 1;
                if (vx < -VELOCITY){
                    vx = -VELOCITY;
                }
            } 
            else if (strcmp(str, Vzero) == 0){
                vx = 0;
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
        float delta_x = vx * 0.5;
        float new_x_pos = x_pos + delta_x;

        // Check if the position is out of bounds
        if (new_x_pos < X_MIN)
        {
            new_x_pos = X_MIN;

            // Stop the motor
            vx = 0;
        }
        else if (new_x_pos > X_MAX)
        {
            new_x_pos = X_MAX;

            // Stop the motor
            vx = 0;
        }

        // Check if the position has changed
        if (new_x_pos != x_pos)
        {
            // Update the position
            x_pos = new_x_pos;
            pos_changed = 1;
        }

        // Write the position to the FIFO if it has changed
        if (pos_changed)
        {
            // Write the position
            char x_pos_str[10];
            sprintf(x_pos_str, "%f", x_pos);

            int m = write(fdx_pos, x_pos_str, strlen(x_pos_str) + 1);
            if (m == -1 || m != strlen(x_pos_str) + 1)
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
            while (x_pos > X_MIN)
            {
                vx = -VELOCITY;
                x_pos += vx * 0.5;

                // Write the position
                char x_pos_str[10];
                sprintf(x_pos_str, "%f", x_pos);

                int m = write(fdx_pos, x_pos_str, strlen(x_pos_str) + 1);
                if (m == -1 || m != strlen(x_pos_str) + 1)
                {
                    // If error occurs while writing to the FIFO
                    error = 1;
                    break;
                }
            }

            vx = 0;
            x_pos = 0.0;
            // Skip writing to the FIFO
            continue;
        }

        // If stop signal was received
        if (stop_flag)
        {
            vx = 0;
            // Skip writing to the FIFO
            continue;
        }
    }

    // Close the FIFOs
    close(fd_vx);
    close(fdx_pos);

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
