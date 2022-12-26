# ADVANCE_and_ROBOT_PROGRAMMING

AUTHOR: Galvagni Gianluca & Demaria CLaudio

1° ASSIGNMENT -> overhead crane simulation. 

The code to design, develop, test and deploy is an interactive simulator  of hoist with 2 d.o.f, in which two different consoles allow the user to activate the hoist.
In the octagonal box there are two motors mx and mz, which displace the hoist along the two respective axes. Motions along axes have their bounds, say 0 - max_x and 0 - max_z.

The simulator requires (at least) the following 5 processes:

watchdog: it checks the previous 4 processes periodically, and sends a reset (like the R button) in case all processes did nothing (no computation, no motion, no input/output) for a certain time, say, 60 seconds.

command console: reading the 6 commands, using keyboard keys.

inspection console: receiving from motors the hoist positions while moving, and reporting on the screen somehow (free choice); the inspection console manages the S ad R buttons as well (simulated in a free way using the keyboard).

motor x: simulating the motion along x axis, receiving command and sending back the real time position including simulated errors.

motor z: similar to motor x.

RUN IT on the console with:

-> sh compile.sh and then -> sh run.sh
