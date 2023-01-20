// AUTHOR: Demaria Claudio & Galvagni Gianluca
#include <stdio.h>
#include <string.h> 
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include "./../include/inspection_utilities.h"

// constanst for the max x position
int const MAX_X = 40;
// constanst for the min x position
int const MIN_X = 0;
// constanst for the max z position
int const MAX_Z = 20;
// constanst for the min z position
int const MIN_Z = 0;

// Variable to store the error
volatile int error = 0;

// Buffer for the log file
char log_buffer[100];
// File descriptor for the log file.
int log_fd;
int write_log(char *to_write, char type){
    // Log that in command_console process button has been pressed with time
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    // Create a string for the type of log
    char log_type[30];
    if (type == 'e'){
        strcpy(log_type, "error");
    }else if (type == 'b'){
        strcpy(log_type, "button pressed");
    }

    // Use snprintf to format the log message
    snprintf(log_buffer, sizeof(log_buffer), "%d:%d:%d: Inspection process %s: %s\n", tm.tm_hour, tm.tm_min, tm.tm_sec, log_type, to_write);

    // Write and check for errors
    if ((write(log_fd, log_buffer, strlen(log_buffer))) == -1){
        return 2;
    }

    return 0;
}

// Function to return maximum of two integers
int max(int a, int b)
{
    return (a > b) ? a : b;
}

// Function to randomly get a number between two integers
int random_between(int a, int b)
{
    return (rand() % (b - a + 1)) + a;
}

// Function to pick a random number between two integers
int pick_random(int a, int b)
{
    int num = random_between(a, b);

    // If num is closer to a, return a
    if (num - a < b - num)
    {
        return a;
    }
    // If num is closer to b, return b
    else
    {
        return b;
    }
}

int main(int argc, char const *argv[])
{
     // PIDs for sending Signals
    pid_t pid_motor_x = atoi(argv[1]);
    pid_t pid_motor_z = atoi(argv[2]);

    // create the log file where write all happenings to the program
    log_fd = open("./log/log_inspection.txt", O_WRONLY | O_APPEND | O_CREAT, 0666); 
    if (log_fd == -1) {
        // If error occurs while opening the log file
        exit(errno);
    }

    // Pipe descriptors
    int fd_x;
    int fd_z;

    // Pipe paths
    char * fifo_x = "/tmp/inspx";
	char * fifo_z = "/tmp/inspz";
    
    mkfifo(fifo_x, 0666);
	mkfifo(fifo_z, 0666);

    fd_x = open(fifo_x, O_RDWR);
    if(fd_x == -1)
    {
        // log error
        int retval = write_log("ERROR opening pipe", 'e');
        close(log_fd);
        if (retval){ // value different from 0 means error
            // If error occurs while writing to the log file
            exit(errno);
        }

        exit(1);
    }

	fd_z = open(fifo_z, O_RDWR);
    if(fd_z == -1)
    {
        // log error
        int retval = write_log("ERROR opening pipe", 'e');
        close(log_fd);
        close(fd_x);
        if (retval){
            // If error occurs while writing to the log file
            exit(errno);
        }

        exit(1);
    }

    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;

    // End-effector coordinates
    float ee_x = 0.0;
    float ee_z = 0.0;

    // Initialize User Interface 
    init_console_ui();

    // Infinite loop
    while(TRUE)
	{	
        // Get mouse/resize commands in non-blocking mode...
        int cmd = getch();

        // If user resizes screen, re-draw UI
        if(cmd == KEY_RESIZE) {

            if(first_resize) {

                first_resize = FALSE;
            }
            else {

                reset_console_ui();
            }
        }

        // Else if mouse has been pressed
        else if(cmd == KEY_MOUSE) {

            // Check which button has been pressed...
            if(getmouse(&event) == OK) {

                // STOP button pressed
                if(check_button_pressed(stp_button, &event)) {

                    // send signal to the motor processess
                    if (kill(pid_motor_x, SIGUSR1) || kill(pid_motor_z, SIGUSR1)){
                        // if error occurs while sending the signal
                        error = 1;
                        goto error_exit;
                    }
                    // write on the log file
                    int retval = write_log("STOP button pressed", 'b');
                    if (retval){
                        // If error occurs while writing to the log file
                        exit(errno);
                    }

                }

                // RESET button pressed
                else if(check_button_pressed(rst_button, &event)) {

                    // send signal to the motor processess
                    if (kill(pid_motor_x, SIGUSR2) || kill(pid_motor_z, SIGUSR2)){
                        // if error occurs while sending the signal
                        error = 1;
                        goto error_exit;
                    }
                    // write on the log file
                    int retval = write_log("RESET button pressed", 'b');
                    if (retval){
                        // If error occurs while writing to the log file
                        exit(errno);
                    }
                }
            }
        }
        
        // Variables to store the read values
        char real_pos_X[20];
        char real_pos_Z[20];

        // Setting parameters for select function
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd_x, &readfds);
        FD_SET(fd_z, &readfds);
        int max_fd = max(fd_x, fd_z) + 1;

        // Setting timeout for select function
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100 ms

        // Wait for the file descriptor to be ready
        int ready = select(max_fd, &readfds, NULL, NULL, &timeout);

        if (ready < 0)
        {
            error = 3;
            goto error_exit;
        }
        else if (ready > 0)
        {
            // Check if both file descriptors are ready to be read
            if (FD_ISSET(fd_x, &readfds) && FD_ISSET(fd_z, &readfds)){

                // Randomly choose which file descriptor to read
                if (pick_random(fd_x, fd_z) == fd_x) {
                    
                    // Read the real position 
                    if (read(fd_x, real_pos_X, 20) < 0)
                    {
                        error = 3;
                        goto error_exit;
                    }
                    else{
                        ee_x = atof(real_pos_X);
                    }                

                    // Store the value read from the pipe into ee_x
                    sscanf(real_pos_X, "%f", &ee_x);

                    // Check if the end-effector is out of the workspace
                    if (ee_x > MAX_X){
                        ee_x = MAX_X;
                    }
                    else if (ee_x < 1){
                        ee_x = 0;
                    }                      
                }
                else
                {
                    // Read the real position
                    if (read(fd_z, real_pos_Z, 20) < 0)
                    {
                        error = 3;
                        goto error_exit;
                    }
                    else{
                        ee_z = atof(real_pos_Z);
                    }                                      

                    // Store the old value of ee_z
                    sscanf(real_pos_Z, "%f", &ee_z);
                    
                    // Check if the end-effector is out of the workspace
                    if (ee_z > MAX_Z){
                        ee_z = MAX_Z;
                    }
                }

            } 
            else if (FD_ISSET(fd_x, &readfds)) { // Check if only the file descriptor for x is ready to be read
                // Read the real position 
                if (read(fd_x, real_pos_X, 20) < 0)
                {
                    error = 3;
                    goto error_exit;
                }
                else{
                    ee_x = atof(real_pos_X);
                }

                // Store the old value of ee_x
                sscanf(real_pos_X, "%f", &ee_x);

                // Check if the end-effector is out of the workspace
                if (ee_x > MAX_X){
                    ee_x = MAX_X;
                }
                else if (ee_x < 1){
                    ee_x = 0;
                }              
            } 
            else if (FD_ISSET(fd_z, &readfds)) { // Check if only the file descriptor for z is ready to be read
                // Read the real position
                if (read(fd_z, real_pos_Z, 20) < 0)
                {
                    error = 3;
                    goto error_exit;
                }
                else{
                    ee_z = atof(real_pos_Z);
                }

                // Store the old value of ee_z
                sscanf(real_pos_Z, "%f", &ee_z);

                // Check if the end-effector is out of the workspace
                if (ee_z > MAX_Z){
                    ee_z = MAX_Z;
                }
            }
        }
        
        // Update UI
        update_console_ui(&ee_x, &ee_z);
	}

    error_exit:
        // close the FIFOs
        close(fd_x);
        close(fd_z);

        // Terminate
        endwin();

        if (error == 1){
            // If error occurs while pressing the buttons
            // Log the error
            int retval = write_log("ERROR: sending signal to the motor processess", 'e');
            if (retval){
                // If error occurs while writing to the log file
                exit(errno);
            }
        }
        
        if (error == 3){
            // If error occurs while reading from the pipe
            // Log the error
            int retval = write_log("ERROR: reading signal from the pipe", 'e');
            if (retval){
                // If error occurs while writing to the log file
                exit(errno);
            }
        }

        // Close the log file
        close(log_fd);

        if (error == 2){
            // If error occurs while writing to the log file
            exit(errno);
        }
          
        exit(0);
    // ------------------------------------------------------------
    // TEMINATE PROGRAM without error
    close(fd_x);
	close(fd_z);

    endwin();

    close(log_fd);

    if (error == 2){
        // If error occurs while writing to the log file
        exit(errno);
    }
     
    exit(0);
}