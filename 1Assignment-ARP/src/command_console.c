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
#include "./../include/command_utilities.h"

// The CONTROL(X) function is usefull to write on shell whenever a system call return an error.
// It's a macro that takes a system caal as input and if it returns -1, it prints the error message and exits the program.
// Otherwise, it returns the value of the system call.
#define CONTROL(X) (                                                        \
{                                                                           \
    int __val = (X);                                                        \
    (__val == -1 ? (                                                        \
        {                                                                   \
            fprintf(stderr, "ERROR in line: %d of file %s : %s\n",          \
                __LINE__, __FILE__, strerror(errno));                       \
            exit(EXIT_FAILURE);                                             \
        })                                                                  \
    : __val);                                                               \
})

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
    snprintf(log_buffer, sizeof(log_buffer), "%d:%d:%d: Command process %s: %s\n", tm.tm_hour, tm.tm_min, tm.tm_sec, log_type, to_write);

    // Write and check for errors
    if ((write(log_fd, log_buffer, strlen(log_buffer))) == -1){
        return 2;
    }

    return 0;
}

int send_velocity(int *fd, char *velocity)
{
    // write on the pipe
    char buffer[2];
    sprintf(buffer, "%s", velocity);       
    int n = write(*fd, buffer, strlen(buffer));
    if (n < 0){
        // Log that there was an error writing to the pipe
        perror("ERROR writing to pipe");
        if(write_log("ERROR writing to pipe", 'e') == -1){
            // If there was an error writing to the log file, exit
            return -1;
        }

        return -1;
    }

    return 0;
}

int main(int argc, char const *argv[])
{

    // Open the log file
    log_fd = open("./log/log_command.txt", O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (log_fd < 0){
        perror("ERROR opening log file");
        return -1;
    }

    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;

    // Initialize User Interface 
    init_console_ui();

    char * Vp = "i";
    char * Vm = "d";
    char * Vzero = "s";

    // Pipe paths
    int fd_vel_x; 
    int fd_vel_z;

    // FIFO file path and creating the named file(FIFO)
    char * myfifox = "/tmp/velx";
    char * myfifoz = "/tmp/velz";
    mkfifo(myfifox, 0666);
    mkfifo(myfifoz, 0666);

    fd_vel_x = open(myfifox, O_WRONLY);
    if (fd_vel_x < 0){
        // Log that there was an error opening the pipe
        int err = write_log("ERROR opening pipe", 'e');
        close(log_fd);
        if(err == -1){
            perror("ERROR writing to log file");
            exit(errno);
        }
        exit(-1);
    }

    fd_vel_z = open(myfifoz, O_WRONLY);
    if (fd_vel_z < 0){
        // Log that there was an error opening the pipe
        int err = write_log("ERROR opening pipe", 'e');
        close(log_fd);
        if(err == -1){
            perror("ERROR writing to log file");
            exit(errno);
        }
        exit(-1);
    }

    // Variable to store error code
    int err = 0;

    // INFINITE LOOP TO HANDLE USER INPUT
    while(1) {

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
                // Vx-- button pressed
                if(check_button_pressed(vx_decr_btn, &event)) {
                    mvprintw(LINES - 1, 1, "Horizontal Speed Decreased");
                    refresh();
                    
                    // log the event
                    err = write_log("Horizontal Speed Decreased", 'b');
                    if(err == -1){
                        perror("ERROR writing to log file");
                        goto error;
                    }

                    write(fd_vel_x, Vm, strlen(Vm)+1);
                    sleep(1);
                    for(int j = 0; j < COLS; j++) {
                        mvaddch(LINES - 1, j, ' ');
                    }
                }

                // Vx++ button pressed
                else if(check_button_pressed(vx_incr_btn, &event)) {
                    mvprintw(LINES - 1, 1, "Horizontal Speed Increased");
                    refresh();

                    // log the event
                    int err = write_log("Horizontal Speed Increased", 'b');
                    if(err == -1){
                        perror("ERROR writing to log file");
                        goto error;
                    }

                    write(fd_vel_x, Vp, strlen(Vp)+1);
                    sleep(1);
                    for(int j = 0; j < COLS; j++) {
                        mvaddch(LINES - 1, j, ' ');
                    }
                }

                // Vx stop button pressed
                else if(check_button_pressed(vx_stp_button, &event)) {
                    mvprintw(LINES - 1, 1, "Horizontal Motor Stopped");
                    refresh();

                    // log the event
                    int err = write_log("Horizontal Motor Stopped", 'b');
                    if(err == -1){
                        perror("ERROR writing to log file");
                        goto error;
                    }

                    write(fd_vel_x, Vzero, strlen(Vzero)+1);
                    sleep(1);
                    for(int j = 0; j < COLS; j++) {
                        mvaddch(LINES - 1, j, ' ');
                    }
                }

                // Vz-- button pressed
                else if(check_button_pressed(vz_decr_btn, &event)) {
                    mvprintw(LINES - 1, 1, "Vertical Speed Decreased");
                    refresh();

                    // log the event
                    int err = write_log("Vertical Speed Decreased", 'b');
                    if(err == -1){
                        perror("ERROR writing to log file");
                        goto error;
                    }
                    
                    write(fd_vel_z, Vm, strlen(Vm)+1);
                    sleep(1);
                    for(int j = 0; j < COLS; j++) {
                        mvaddch(LINES - 1, j, ' ');
                    }
                }

                // Vz++ button pressed
                else if(check_button_pressed(vz_incr_btn, &event)) {
                    mvprintw(LINES - 1, 1, "Vertical Speed Increased");
                    refresh();

                    // log the event
                    int err = write_log("Vertical Speed Increased", 'b');
                    if(err == -1){
                        perror("ERROR writing to log file");
                        goto error;
                    }
                    
                    write(fd_vel_z, Vp, strlen(Vp)+1);
                    sleep(1);
                    for(int j = 0; j < COLS; j++) {
                        mvaddch(LINES - 1, j, ' ');
                    }
                }

                // Vz stop button pressed
                else if(check_button_pressed(vz_stp_button, &event)) {
                    mvprintw(LINES - 1, 1, "Vertical Motor Stopped");
                    refresh();

                    // log the event
                    int err = write_log("Vertical Motor Stopped", 'b');
                    if(err == -1){
                        perror("ERROR writing to log file");
                        goto error;
                    }
                    
                    write(fd_vel_z, Vzero, strlen(Vzero)+1);
                    sleep(1);
                    for(int j = 0; j < COLS; j++) {
                        mvaddch(LINES - 1, j, ' ');
                    }
                }
            }
        }
	}

    error:
        close(fd_vel_x);
        close(fd_vel_z);
        close(log_fd);
        endwin();
        return -1;

    // END OF THE PROGRAM
    close(fd_vel_x);
    close(fd_vel_z);
    close(log_fd);
    
    // Terminate
    endwin();
    return 0;
}
