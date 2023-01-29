## Description of the programs
The code to design, develop, test and deploy is an interactive simulator  of hoist with 2 d.o.f, in which two different consoles allow the user to activate the hoist.
In the octagonal box there are two motors mx and mz, which displace the hoist along the two respective axes. Motions along axes have their bounds, say 0 - max_x and 0 - max_z.

The simulator requires (at least) the following 5 processes:

> watchdog: it checks the previous 4 processes periodically, and sends a reset (like the R button) in case all processes did nothing (no computation, no motion, no input/output) for a certain time, say, 60 seconds.

> command console: reading the 6 commands, using mouse to click push buttons.

> motor x: simulating the motion along x axis, receiving command and sending to the world the position x.

> motor z: similar to motor x.

> world: receiving the position x and z and sending the real time position including simulated errors.

> inspection console: receiving from world the hoist positions while moving, and reporting on the screen; the inspection console manages the S ad R buttons as well (simulated with red and orange buttons).

## How to compile and run it
I added two file .sh in order to simplify compiling and running all the processes.  
To compile it: execute ```compile.sh```;  
To run it: execute ```run.sh```. 
