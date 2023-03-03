# Assignment 3

## Description of the programs
The code to design, develop, test and deploy is a modified version of Assignment 2, including client/server features. We refer to this as "application". In the modified application, process B, shared memory and the second ncurses window are unchanged. Process A includes two new features:
1. client connection toward a similar application running on a different machine in the network.
2. server connection for a similar application running on a different machine in the network.

Therefore the application, when launched, asks for one execution modality:
1. normal, as assignment 2
2. server: the application does not use the keyboard for input: it receives input from another application (on a different machine) running in client mode
3. client: the application runs normally as assignment 2 and sends its keyboard input to another application (on a different machine) running in server mode
When selecting modes 2 and 3 the application obviously asks address and port of the companion application.

IP protocol: TCP

data: a byte stream of char, one per key pressed.

(flush data if necessary).

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
