// AUTHOR: Dmaria Claudio & Galvagni Gianluca
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

// Variable to the time of the whatchdog
int const MAX_SLEEP = 60; // 60 seconds

// The CONTROL(X) function is usefull to write on shell whenever a system call return an error.
// It's a macro that takes a system call as input and if it returns -1, it prints the error message and exits the program.
// exit(EXIT_FAILURE) is used to exit the program with a failure status. It means 
// Otherwise, it returns the value of the system call.
#define CONTROL(X) (                                                          \
{                                                                             \
      int __val = (X);                                                        \
  (__val == -1 ? (                                                            \
    {                                                                         \
            fprintf(stderr, "ERROR in line: %d of file %s\n",                 \
      __LINE__, __FILE__,                                                     \
      strerror(errno));                                                       \
            exit(EXIT_FAILURE);                                               \
        })                                                                    \
      : __val);                                                               \
})

  // PIDs variables
  pid_t pid_cmd;
  pid_t pid_motor_x;
  pid_t pid_motor_z;
  pid_t pid_world;
  pid_t pid_insp;
  pid_t pid_mstr;

  // Variable for the status of the child processes
  int status;

// function to fork and create a child process
int spawn(const char *program, char *arg_list[]) {

  pid_t child_pid = fork();

  // if fork() return -1, it means that the system call failed
  if(child_pid < 0) {
    perror("Error while forking...");
    return -1;
  }

  // if fork() return a positive number, it means that the system call was successful
  else if(child_pid != 0) {
    return child_pid;
  }

  // if fork() return 0, it means that the system is in the child process
  else {
    if (execvp(program, arg_list) == -1) {
      perror("Error while executing...");
      return -1;
    }
  }
}

// function to create all the child processes checking if the system call was successful for the next processes
int create_log_files(){

  // create the log file
  int fd_cmd = open("./log/log_command.txt", O_CREAT | O_RDWR, 0666);
  int fd_motor_x = open("./log/log_motor_x.txt", O_CREAT | O_RDWR, 0666);
  int fd_motor_z = open("./log/log_motor_z.txt", O_CREAT | O_RDWR, 0666);
  int fd_world = open("./log/log_world.txt", O_CREAT | O_RDWR, 0666);
  int fd_insp = open("./log/log_inspection.txt", O_CREAT | O_RDWR, 0666);

  // if the system call fails, it returns -1
  if (fd_cmd == -1 || fd_motor_x == -1 || fd_motor_z == -1 || fd_world == -1 || fd_insp == -1) {
    return 1;
  } 

  // Close the file descriptors
  close(fd_cmd);
  close(fd_motor_x);
  close(fd_motor_z);
  close(fd_world);
  close(fd_insp);

  // if all the system calls are successful, the function returns 0
  return 0;
}

// function to kill all child processes
void kill_all(){

  // kill all child processes
  CONTROL(kill(pid_cmd, SIGKILL));
  CONTROL(kill(pid_motor_x, SIGKILL));
  CONTROL(kill(pid_motor_z, SIGKILL));
  CONTROL(kill(pid_world, SIGKILL));
  CONTROL(kill(pid_insp, SIGKILL));
}

time_t get_last_modified(char *file_name){

  // get the last modification time of the file
  struct stat attrib;
  
  if(stat(file_name, &attrib) == -1){
    perror("Error while getting the last modification time of the file");
    return -1;
  }
  return attrib.st_mtime;
}

// watchdog function
// it configure allarm signal with 60 seconds of timeout
// and if the child process is not alive, it kills all child processes
// it also check if the child processes are writing on the log files
// if they are not, it kills all child processes                        
int watchdog(){

  // set an array of the name of all log files
  char *log_array[5] = {"./log/log_command.txt", "./log/log_motor_x.txt", "./log/log_motor_z.txt", "./log/log_world.txt", "./log/log_inspection.txt"};

  // set an array of the PID of all child processes
  pid_t pid_array[5] = {pid_cmd, pid_motor_x, pid_motor_z, pid_world, pid_insp};

  // FLAG to check if the file is modified
  // if it's 1, it means that the file is not modified
  // if it's 0, it means that the file is modified in the last MAX_SLEEP seconds
  int flag_mod = 0;

  // variable to store the time of the last modification of the file
  time_t last_mod;

  // Variable to store the seconds since the last modification of the file
  int seconds_since_mod = 0;

  // infinite loop
  while(1){

    // get the current time
    time_t now = time(NULL);
    
    // for every log file
    for(int i = 0; i < 5; i++){

      // get the last modification time of the log file
      last_mod = get_last_modified(log_array[i]);

      // if the last_mod is -1, it means that the system call failed
      if(last_mod == -1){
        // kill all child processes
        kill_all();
        // Print error message
        perror("Error while getting the last modification time of the file...");
        return -1;
      }

      // check if the last modification was in the last 2 seconds
      if (now - seconds_since_mod > 2){
        // if the file was not modified
        flag_mod = 1;
      }
      else {
        // if the file was modified
        flag_mod = 0;
        seconds_since_mod = 0;
      }
    }

    // if the file was not modified, increment the seconds since the last modification
    if (flag_mod == 1){
      seconds_since_mod += 2;
    }

    // Check if child processes terminated unexpectedly
    for(int i = 0; i < 5; i++){
      // if the child process is not alive
      if(waitpid(pid_array[i], &status, WNOHANG) == pid_array[i]){
        // kill all child processes
        kill_all();
        // Print error message
        perror("Error while waiting for child processes...");
        return -1;
      }
    }

    // if the couter is greater than MAX_SLEEP seconds, kill all child processes
    if(seconds_since_mod > MAX_SLEEP){
      // kill all child processes
      kill_all();
      // Print error message
      perror("Error the time of the last modification is GREATER than MAX_SLEEP seconds ago...");
      return 1;
    }

    // sleep for 2 second
    sleep(2);
  }
}

int main() {

  // File descriptor for the log file
  int log_fd;

  // create the log file where write all happenings to the program
  log_fd = open("./log/log_master.txt", O_WRONLY | O_APPEND | O_CREAT, 0666); //NON TROVA LA DIRECTORY ERRORE QUI??????
  if (log_fd == -1) {
    perror("Error while creating the log file...");
    return -1;
  }

  // variable to write on the log file
  char buffer[100];

  // char variable in order to convert the PID of the processes to string
  char pid_master[10];
  char pid_motor_x_str[10];
  char pid_motor_z_str[10];

  // log that the MASTER process has started at the current time
  time_t now = time(NULL);
  struct tm t = *localtime(&now);
  sprintf(buffer, "Master process started at %d:%d:%d\n", t.tm_hour, t.tm_min, t.tm_sec);
  CONTROL(write(log_fd, buffer, strlen(buffer)));

  // chekc if there are errors while writing on the log file
  if (errno != 0) {
    perror("Error while writing on the log file...");
    close(log_fd);
    return -1;
  }

  // start the COMMAND CONSOLE process passing the PID of the MASTER process as argument
  char * arg_list_command[] = { "/usr/bin/konsole", "-e", "./bin/command_console", NULL };
  CONTROL(pid_cmd = spawn("/usr/bin/konsole", arg_list_command));

  if(pid_cmd == -1){
    // if the spawn() function fails, it returns -1
    goto spawn_error;
  }
  
  // start the MOTOR_X process
  char * arg_list_motor_x[] = {"./bin/motor_x", NULL };
  CONTROL(pid_motor_x = spawn("./bin/motor_x", arg_list_motor_x));

  if(pid_motor_x == -1){
    // if the spawn() function fails, it returns -1
    goto spawn_error;
  }

  // convert the PID of the MOTOR_X process to string
  sprintf(pid_motor_x_str, "%d", pid_motor_x);

  // start the MOTOR_Z process
  char * arg_list_motor_z[] = { "./bin/motor_z", NULL };
  CONTROL(pid_motor_z = spawn("./bin/motor_z", arg_list_motor_z));

  if(pid_motor_z == -1){
    // if the spawn() function fails, it returns -1
    goto spawn_error;
  }

  // convert the PID of the MOTOR_Z process to string
  sprintf(pid_motor_z_str, "%d", pid_motor_z);

  // start the WORLD process
  char * arg_list_world[] = {"./bin/world", NULL };
  CONTROL(pid_world = spawn("./bin/world", arg_list_world));

  if(pid_world == -1){
    // if the spawn() function fails, it returns -1
    goto spawn_error;
  }

  // start the INSPECTION process
  char * arg_list_insp[] = { "/usr/bin/konsole", "-e", "./bin/inspection_console", pid_motor_x_str, pid_motor_z_str, NULL};
  CONTROL(pid_insp = spawn("/usr/bin/konsole", arg_list_insp));

  if(pid_insp == -1){
    // if the spawn() function fails, it returns -1
    goto spawn_error;
  }

  // write on the log file the time when the child processes have been started
  now = time(NULL);
  t = *localtime(&now);
  sprintf(buffer, "All processes started at %d:%d:%d\n", t.tm_hour, t.tm_min, t.tm_sec);
  CONTROL(write(log_fd, buffer, strlen(buffer)));

  // chekc if there are errors while writing on the log file
  if (errno != 0) {
    perror("Error while writing on the log file...");
    close(log_fd);
    return -1;
  }
  
  // if there are NOT errors while starting the child processes, the MASTER process continues its execution
  goto no_spawn_error;

// if there are errors while starting the child processes, the MASTER process terminates
spawn_error:
  // kill all child processes
  perror("Error while starting the child processes...");
  kill_all();
  close(log_fd);
  return -1;

// if there are NOT errors while starting the child processes, the MASTER process continues its execution
no_spawn_error:
  // if the spawn() function is successful, the software is running normally
  // create all log files
  int spy = create_log_files();

  if(spy == -1){
    // if the create_log_files() function fails, it returns -1
    // Print error message
    perror("Error while creating log files...");
    // kill all child processes
    kill_all();
    close(log_fd);
    return -1;
  }

  // start the WATCHDOG process
  int watchdog_status = watchdog();

  // if the watchdog() function fails, it returns -1
  if(watchdog_status == -1){
    // kill all child processes
    kill_all();
    // Print error message
    perror("Error while running the watchdog...");
    
    now = time(NULL);
    t = *localtime(&now);
    sprintf(buffer, "Master process terminated at %d:%d:%d\n---> Error while running the watchdog...\n", t.tm_hour, t.tm_min, t.tm_sec);
    CONTROL(write(log_fd, buffer, strlen(buffer)));
    if(errno != 0){
      perror("Error while writing on the log file...");
      close(log_fd);
      return -1;
    }
    
    return -1;
  } 
  else if (watchdog_status == 1){ // if the watchdog() function is okay but the timeout is reached, it returns 1
    // kill all child processes
    kill_all();
    // Print error message
    perror("Watchdog timeout...");   
    
    now = time(NULL);
    t = *localtime(&now);
    sprintf(buffer, "Master process terminated at %d:%d:%d\n---> Watchdog timeout...\n", t.tm_hour, t.tm_min, t.tm_sec);
    CONTROL(write(log_fd, buffer, strlen(buffer)));
    if(errno != 0){
      perror("Error while writing on the log file...");
      close(log_fd);
      return 1;
    }

    return 1;
  } 
  else { // if the watchdog() function is successful, it returns 0 and the software terminates normally
    // print on the log file the time when the software terminates normally
    now = time(NULL);
    t = *localtime(&now);
    sprintf(buffer, "Master process terminated at %d:%d:%d\n---> Master process terminated normally...\n", t.tm_hour, t.tm_min, t.tm_sec);
    CONTROL(write(log_fd, buffer, strlen(buffer)));
    if (errno != 0) {
      perror("Error while writing on the log file...");
      close(log_fd);
      return -1;
    }
  }
  
  kill_all();
  // close the log file
  close(log_fd);

  return 0;
}