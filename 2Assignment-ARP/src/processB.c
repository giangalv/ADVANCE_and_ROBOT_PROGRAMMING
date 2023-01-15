// Group: CDGG
// Authors:
// Claudio Demaria S5433737
// Gianluca Galvagni S5521188

#include "./../include/processB_utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <bmpfile.h>
#include <math.h>
#include <semaphore.h>

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

// Buffer to store the string to write to the log file
char log_buffer[100];

// File descriptor for the log file
int log_fd;

// Function to draw a blue circle
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

// Function to cancel the blue circle
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

int main(int argc, char const *argv[])
{
    // Open the log file
    if ((log_fd = open("log/processB.log",O_WRONLY|O_APPEND|O_CREAT, 0666)) == -1)
    {
        // If the file could not be opened, print an error message and exit
        perror("Error opening command file");
        exit(1);
    }

    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;

    // Initialize UI
    init_console_ui();

    // Variable declaration in order to access to shared memory
    key_t          ShmKEY;
    int            ShmID;

    // Pointer to the struct of shared memory
    struct shared  *ShmPTR;

    // Get the key of the shared memory
    ShmKEY = ftok(".", 'x');

    // Get the ID of the shared memory
    ShmID = shmget(ShmKEY, sizeof(struct shared), 0666);
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
        exit(1);
    }

    // Variable declaration in order to store the coordinates of the circles
    int center_cord = 0;
    int x_cord[600];
    int y_cord[600];
    int y;
    int y_old;
    int x_old;

    // Flag
    int flag ;

    // Open the semaphore 1
    semaphore = sem_open("/mysem", 0);
    if (semaphore == (void*) -1)
    {
        perror("sem_open failure");
        exit(1);
    }

    // Open the semaphore 2
    semaphore2 = sem_open("/mysem2", 0);
    if (semaphore2 == (void*) -1)
    {
        perror("sem_open failure");
        exit(1);
    }

    // Variable declaration in order to get the time
    time_t rawtime;
    struct tm *info;

    // Infinite loop
    while (TRUE) {

        // Get current time
        time(&rawtime);
        info = localtime(&rawtime);

        // Get the mouse event
        int cmd = getch();
               
        // If user resizes screen, re-draw UI...
        if(cmd == KEY_RESIZE) {
            if(first_resize) {
                first_resize = FALSE;
            }
            else {
                reset_console_ui();
            }
        }

        else
        {

            mvaddch(LINES/2, COLS/2, '0');

            refresh();

            // Wait for the semaphore 2
            sem_wait(semaphore2);

            // Initialize the coordinates of the circles
            for (int i=0;i<600;i++)
            {
                x_cord[i]=0;
                y_cord[i]=0;
            }
            center_cord = 0;

            int i, j;
            y = 0;
            flag = 0;
            
            // Get the coordinates of the circles
            for (i = 0; i < 1600; i++) {
                
                // If the flag is 1, break the loop
                if (flag == 1) {
                    break;
                }
                for (j = 0; j < 600; j++) {

                    // Get the coordinates of the circles from the shared memory
                    if (ShmPTR->m[i][j] == 1)
                    {
                        x_cord[y] = j;
                        y_cord[y] = i;

                        if (x_cord[y] > x_cord[y-1]) {
                            flag = 1;
                            break;
                        }

                        y++;
                        break;
                    }
                }
            }

            // Update the position of the center
            center_cord = x_cord[y-1] + 30;

            // Write the position of the center in the log file
            sprintf(log_buffer, "<Process_B> Position of center updated: %s\n", asctime(info));
            if (write(log_fd, log_buffer, strlen(log_buffer)) == -1)
            {
                perror("Error writing to log file");
                exit(1);
            }

            // Draw the circles
            mvaddch(floor((int)(center_cord/20)),floor((int)(y_cord[y-1]/20)), '0');

            refresh();

            // Signal the semaphore 1
            sem_post(semaphore);

            // Cancel the circle with the coordinates of the previous loop
            cancel_blue_circle(30,y_old,x_old,bmp);

            // Draw the circle with the coordinates of the current loop
            draw_blue_circle(30,y_cord[y-1],center_cord,bmp);   

            // Update the (previous) coordinates for the next loop
            y_old = y_cord[y-1];
            x_old = center_cord;           
        }
         
    }

    // Close the semaphores
    sem_close(semaphore);
    sem_close(semaphore2);

    // Detach and remove the shared memory
    shmdt((void *) ShmPTR);
    bmp_destroy(bmp);

    endwin();

    // Close the log file
    close(log_fd);

    return 0;
}
