#include "./../include/processA_utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <bmpfile.h>
#include <math.h>
#include <time.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

// Define the size of the shared memory
#define WIDTH 1600
#define HEIGHT 600
#define DEPTH 4

// Define the struct of the shared memory
struct shared
{
    int x;
    int y;
    int m[WIDTH][HEIGHT];
};

// Set the color of the circle (0 - 255)
const u_int8_t RED = 0;
const u_int8_t GREEN = 0;
const u_int8_t BLUE = 255;
const u_int8_t ALPHA = 0;

// Delcare circle radius
const int RADIUS = 30;

// Define the semaphores
sem_t *semaphore;
sem_t *semaphore2;

// Character buffer for the log file
char log_buffer[100];

// File descriptor for the log file
int log_fd;

// Function to draw a circle
void draw_my_circle(int radius, int x, int y, bmpfile_t *bmp, rgb_pixel_t color) {
    // Define the center of the circle
    int centerX = x * 20;
    int centerY = y * 20;

    // Loop over the pixels of the circle
    for (int i = centerX - radius; i <= centerX + radius; i++) {
        for (int j = centerY - radius; j <= centerY + radius; j++) {
            if (pow(i - centerX, 2) + pow(j - centerY, 2) <= pow(radius, 2)) {
                // Color the pixel at the specified (x,y) position with the given pixel values
                bmp_set_pixel(bmp, i, j, color);
            }
        }
    }
}

// Function to clear the circle
void clear_circle(int radius, int x, int y, bmpfile_t *bmp) {
    // Define the center of the circle
    int centerX = x * 20;
    int centerY = y * 20;
    // Define the color of the circle
    rgb_pixel_t color = {255, 255, 255, 0}; // White

    // Loop over the pixels of the circle
    for (int i = centerX - radius; i <= centerX + radius; i++) {
        for (int j = centerY - radius; j <= centerY + radius; j++) {
            // If the pixel is inside the circle..
            if (pow(i - centerX, 2) + pow(j - centerY, 2) <= pow(radius, 2)) {
                // Color the pixel at the specified (x,y) position with the given pixel values
                bmp_set_pixel(bmp, i, j, color);
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

    // File descriptor for the shared memory
    int shm_fd;

    // Pointer to the struct of shared memory
    struct shared *shm_ptr;

    // Open the shared memory
    shm_fd = shm_open("my_shm", O_RDWR | O_CREAT, 0666);
    if (shm_fd == -1) {
        perror("Error in shm_open");
        exit(1);
    }

    // Set the size of the shared memory
    ftruncate(shm_fd, sizeof(struct shared));

    // Map the shared memory to the memory space of the process
    shm_ptr = mmap(NULL, sizeof(struct shared), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("Error in mmap");
        exit(1);
    }

    // Use the shared memory data to determine where to draw circles on the image

    //
    
    // Create the bitmap
    bmpfile_t *bmp;
    bmp = bmp_create(WIDTH, HEIGHT, DEPTH);
    if (bmp == NULL) {
        printf("Error: unable to create bitmap\n");
        return 1;
    }
    
    // Open the semaphores
    semaphore = sem_open("/my_sem1", O_CREAT, 0666, 1);
    if (semaphore == SEM_FAILED) {
        perror("Error in sem_open");
        exit(1);
    }
    semaphore2 = sem_open("/my_sem2", O_CREAT, 0666, 1);
    if (semaphore2 == SEM_FAILED) {
        perror("Error in sem_open");
        exit(1);
    }

    // Variable declaration in order to get the time
    time_t rawtime;
    struct tm *info;

    while (TRUE) {

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
            sprintf(log_buffer, "PROCESS_A: Keyboard button pressed: %d - %d (%s)\n", circle.x, circle.y, asctime(info));
            if (write(log_fd, log_buffer, strlen(log_buffer)) == -1)
            {
                // If the file could not be opened, print an error message and exit
                perror("Error writing to log file");
                exit(1);
            }  
            
            // Move the circle
            move_circle(cmd);

            // Draw the circle
            draw_circle();
 
            // Cancel the circle
            clear_circle(RADIUS,x,y, bmp);

            // Set the shared memory to 0 for the pixels of the circle
            memset(shm_ptr->m, 0, sizeof(shm_ptr->m));
            
            // Choose the circle color
            rgb_pixel_t color = {RED, GREEN, BLUE, ALPHA};

            // Draw the new circle position and update the shared memory
            draw_my_circle(RADIUS,circle.x,circle.y, bmp, color);
            shm_ptr->x = circle.x;
            shm_ptr->y = circle.y;

            // Loop through the pixels of the bitmap
            for (int i = 0; i < WIDTH; i++) {
                for (int j = 0; j < HEIGHT; j++) {
                    // Get the pixel
                    rgb_pixel_t *pixel = bmp_get_pixel(bmp, i, j);
                    
                    // Set the corresponding pixel in the shared memory to 1
                    if ((pixel->blue == BLUE) && (pixel->red == RED) && (pixel->green == GREEN) 
                            && (pixel->alpha == ALPHA)) {
                        shm_ptr->m[i][j] = 1; 
                    }                    
                }
            }

            // Signal the semaphore 2
            sem_post(semaphore2);
        }

        /*
        // Wait for the semaphore
        sem_wait(semaphore);

        // Check the shared memory for the coordinates of the circle
        int x = shm_ptr->m[0][0];
        int y = shm_ptr->m[0][1];

        // Draw the circle on the image
        draw_my_circle(RADIUS, x, y, bmp, (rgb_pixel_t){RED, GREEN, BLUE, ALPHA});

        // Post the semaphore2 to signal that the operation is done
        sem_post(semaphore2);
        */
    }

    // Close the semaphores and unlink the shared memory
    // Close the bitmap
    bmp_destroy(bmp);
    sem_close(semaphore);
    sem_close(semaphore2);
    sem_unlink("my_sem1");
    sem_unlink("my_sem2");
    munmap(shm_ptr, sizeof(struct shared));
    shm_unlink("my_shm");

    return 0;
}

