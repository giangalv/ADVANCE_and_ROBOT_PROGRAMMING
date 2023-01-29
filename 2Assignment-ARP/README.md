# ARP-Assignment2

Group: CDGG  
Authors:  
Claudio Demaria (S5433737)  
Gianluca Galvagni (S5521188)  
GitHub repository: https://github.com/claudioDema99/ARP-Assignment2.git

## Description of the programs
The project consists in the implementation of the simulated vision system through shared memory, according to the requirements specified in the PDF file of the assignment.

The two processes involved in the simulation of the vision system, implemented as simple *ncurses windows*, are called **processA** and **processB** and they communicate each others with an inter-process communication pipeline, that is the shared memory (SHM).  
The user can control the *marker* in the **processA** window using the keyboard arrow, and in the **processB** window are printed the trajectory of the movement.  
There is also a **print button**: when the user click on it with the mouse, the program prints a *.png* image of the current position of the *marker*, and put this image into the *out* folder.

There is also a **master** process already prepared for you, responsible of spawning the entire simulation.

## How to compile and run it
I added two file .sh in order to simplify compiling and running all the processes.  
To compile it: execute ```compile.sh```;  
To run it: execute ```run.sh```.  
To correctly run the programs, you also need to install the *libbitmap* library (see the following paragraph).

## *libbitmap* installation and usage
To work with the bitmap library, you need to follow these steps:
1. Download the source code from [this GitHub repo](https://github.com/draekko/libbitmap.git) in your file system.
2. Navigate to the root directory of the downloaded repo and run the configuration through command ```./configure```. Configuration might take a while.  While running, it prints some messages telling which features it is checking for.
3. Type ```make``` to compile the package.
4. Run ```make install``` to install the programs and any data files and documentation (sudo permission is required).
5. Upon completing the installation, check that the files have been properly installed by navigating to ```/usr/local/lib```, where you should find the ```libbmp.so``` shared library ready for use.
6. In order to properly compile programs which use the *libbitmap* library, you first need to notify the **linker** about the location of the shared library. To do that, you can simply add the following line at the end of your ```.bashrc``` file:      
```export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"```
