// Group: CDGG
// Authors:
// Claudio Demaria S5433737
// Gianluca Galvagni S5521188

#include "./../include/processA_utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <bmpfile.h>
#include <math.h>
#include <time.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

// Define the size of the shared memory
const int width = 1600;
const int height = 600;
const int depth = 4;

// Define the struct of the shared memory
struct shared
{
    int m[1600][600];
};

// Define the semaphores
sem_t *semaphore;
sem_t *semaphore2;

// Character buffer for the log file
char log_buffer[100];

// File descriptor for the log file
int log_fd;

// Function to draw the blue circle
void draw_blue_circle(int radius,int x,int y, bmpfile_t *bmp) {
    // Define the color of the circle
    rgb_pixel_t pixel = {255, 0, 0, 0};
    // Loop over the pixels of the circle
    for(int i = -radius; i <= radius; i++) {
        for(int j = -radius; j <= radius; j++) {
            // If distance is smaller, point is within the circle
            if(sqrt(i*i + j*j) < radius) {
                /*
                * Color the pixel at the specified (x,y) position
                * with the given pixel values
                */
                bmp_set_pixel(bmp, x*20 + i, y*20 + j, pixel);
            }
        }
    }
}

// Function to clear the blue circle
void cancel_blue_circle(int radius,int x,int y, bmpfile_t *bmp) {
    // Define the color of the circle
    rgb_pixel_t pixel = {255, 255, 255, 0};
    // Loop over the pixels of the circle
    for(int i = -radius; i <= radius; i++) {
        for(int j = -radius; j <= radius; j++) {
            // If distance is smaller, point is within the circle
            if(sqrt(i*i + j*j) < radius) {
                /*
                * Color the pixel at the specified (x,y) position
                * with the given pixel values
                */
                bmp_set_pixel(bmp,  x*20+i,y*20+  j, pixel);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    // Open the log file
    if ((log_fd = open("log/processA.log",O_WRONLY|O_APPEND|O_CREAT, 0666)) == -1)
    {
        // If the file could not be opened, print an error message and exit
        perror("Error opening command file");
        exit(1);
    }

    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;

    // Initialize UI
    init_console_ui();

    // Delcare circle radius
    int radius = 30;

    // Variable declaration in order to access to shared memory
    key_t          ShmKEY;
    int            ShmID;

    // Pointer to the struct of shared memory
    struct shared  *ShmPTR;

    // Get the key of the shared memory
    ShmKEY = ftok(".", 'x');

    // Get the ID of the shared memory
    ShmID = shmget(ShmKEY, sizeof(struct shared), IPC_CREAT | 0666);
    if (ShmID < 0) {
        printf("*** shmget error (server) ***\n");
        exit(1);
    }

    // Attach the shared memory to the pointer
    ShmPTR = (struct shared *) shmat(ShmID, NULL, 0);
    if ((int) ShmPTR == -1) {
        printf("*** shmat error (server) ***\n");
        exit(1);
    }

    // Create the bitmap
    bmpfile_t *bmp;
    bmp = bmp_create(width, height, depth);
    if (bmp == NULL) {
        printf("Error: unable to create bitmap\n");
        return 1;
    }

    // Open the semaphore 1
    semaphore = sem_open("/mysem", O_CREAT, 0666, 1);
    if(semaphore == (void*)-1)
    {
        perror("sem_open 1 failure");
        exit(1);
    }

    // Open the semaphore 2
    semaphore2 = sem_open("/mysem2", O_CREAT, 0666, 0);
    if(semaphore2 == (void*)-1)
    {
        perror("sem_open 2 failure");
        exit(1);
    }

    // Variable declaration in order to get the time
    time_t rawtime;
    struct tm *info;

    // Infinite loop
    while (TRUE){

        // Get current time
        time(&rawtime);
        info = localtime(&rawtime);

        // Get the mouse event
        int cmd = getch();

        // Get the position of the circle
        int x = circle.x;
        int y = circle.y;

        // If the user resize the window..
        if(cmd == KEY_RESIZE) {
            if(first_resize) {
                first_resize = FALSE;
            }
            else {
                reset_console_ui();
            }
        }

        // Else, if user presses print button..
        else if(cmd == KEY_MOUSE) {
            if(getmouse(&event) == OK) {
                if(check_button_pressed(print_btn, &event)) {
                    mvprintw(LINES - 1, 1, "Print button pressed");

                    // Write to the log file
                    sprintf(log_buffer, "<Process_A> Print button pressed: %s\n", asctime(info));
                    if (write(log_fd, log_buffer, strlen(log_buffer)) == -1)
                    {
                        // If the file could not be opened, print an error message and exit
                        perror("Error writing to log file");
                        exit(1);
                    }

                    // Save the bitmap
                    bmp_save(bmp, "out/image.bmp");

                    refresh();
                    sleep(1);
                    for(int j = 0; j < COLS - BTN_SIZE_X - 2; j++) {
                        mvaddch(LINES - 1, j, ' ');
                    }
                }
            }
        }
        
        // If input is an arrow key, move circle accordingly...
        else if(cmd == KEY_LEFT || cmd == KEY_RIGHT || cmd == KEY_UP || cmd == KEY_DOWN) 
        {
            
            // Wait for the semaphore
            sem_wait(semaphore);
            
            // Write to the log file
            sprintf(log_buffer, "<Process_A> Keyboard button pressed: %s\n", asctime(info));
            if (write(log_fd, log_buffer, strlen(log_buffer)) == -1)
            {
                // If the file could not be opened, print an error message and exit
                perror("Error writing to log file");
                exit(1);
            }  
            
            // Move  and draw the circle
            move_circle(cmd);
            draw_circle();
            
            // Cancel the circle
            cancel_blue_circle(radius,x,y, bmp);
            for (int i = 0; i < 1600; i++) {
                for (int j = 0; j < 600; j++) {
                ShmPTR->m[i][j] = 0;
                }
            }
            
            // Draw the new circle position and update the shared memory
            draw_blue_circle(radius,circle.x,circle.y, bmp);
            
            // Loop for checking all the pixels
            for (int i = 0; i < 1600; i++) {
                for (int j = 0; j < 600; j++) {
                    // Get the pixel
                    rgb_pixel_t *pixel = bmp_get_pixel(bmp, i, j);
                    
                    // If the pixel is blue, set the corresponding pixel in the shared memory to 1
                    if ((pixel->blue == 255) && (pixel->red == 0) && (pixel->green==0) && (pixel->alpha==0)) {
                        ShmPTR->m[i][j] = 1; 
                    }                    
                }
            }

            // Signal the semaphore 2
            sem_post(semaphore2);
        }
            
    }

    // Close and unlink the semaphore 1
    sem_close(semaphore);
    sem_unlink("/mysem");

    // Close and unlink the semaphore 2
    sem_close(semaphore2);
    sem_unlink("/mysem2");

    // Detach and remove the shared memory
    shmdt((void *) ShmPTR);
    shmctl(ShmID, IPC_RMID, NULL);

    // Close the bitmap
    bmp_destroy(bmp);

    endwin();

    // Close the log file
    close(log_fd);

    return 0;
}
