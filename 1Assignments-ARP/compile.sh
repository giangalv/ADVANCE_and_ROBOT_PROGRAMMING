#Compile the inspection program
gcc src/inspection_console.c -lncurses -lm -o bin/inspection_console &

#Compile the command program
gcc src/command_console.c -lncurses -o bin/command_console &

#Compile the master program
gcc src/master.c -o bin/master &

#Compile the motor x program
gcc src/motor_x.c -o bin/motor_x &

#Compile the motor z program
gcc src/motor_z.c -o bin/motor_z &

#Compile the real coordinates program
gcc src/world.c -o bin/world
