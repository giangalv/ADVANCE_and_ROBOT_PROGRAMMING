// Group: CDGG
// Authors:
// Claudio Demaria S5433737
// Gianluca Galvagni S5521188

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>

// PIDs of the child processes
pid_t pid_procA;
pid_t pid_procB;

// Buffer for the log file
char log_buffer[100];

// File descriptor for the log file
int log_fd;

// Function to spawn a child process
int spawn(const char * program, char * arg_list[]) {
  pid_t child_pid = fork();
  if(child_pid < 0) {
    perror("Error while forking...");
    return 1;
  }
  else if(child_pid != 0) {
    return child_pid;
  }
  else {
    if(execvp (program, arg_list) == 0);
    perror("Exec failed");
    return 1;
  }
}

// Function to get the last modified time of a file
time_t get_last_modified(char *filename)
{
  struct stat attr;
  stat(filename, &attr);
  return attr.st_mtime;
}

int watchdog()
{
  // Array of the log files
  char *log_files[2] = {"log/processA.log", "log/processB.log"};

  // Array of the PIDs
  pid_t pids[2] = {pid_procA, pid_procB};

  // Variable to check if the log files were modified
  int modified;

  // Counter to check if the processes are alive
  int counter = 0;

  while (1)
  {
    // Get the current time
    time_t current_time = time(NULL);

    // Check if the log files were modified
    for (int i = 0; i < 2; i++)
    {
      // Get the last modified time of the file
      time_t last_modified = get_last_modified(log_files[i]);

      // Check if the file was modified in the last 3 seconds
      if (current_time - last_modified > 3)
      {
        modified = 0;
      }
      else
      {
        modified = 1;
        counter = 0;
      }
    }

    // If the log files were not modified, increment the counter
    if (modified==0)
    {
      counter += 3;
    }

    // If the counter is greater than 300 seconds, kill the processes
    if (counter > 300)
    {
      // Kill all the processes
      kill(pid_procA, SIGKILL);
      kill(pid_procB, SIGKILL);
      printf("Processes killed by the watchdog\n");
      return 0;
    }

    // Sleep for 2 seconds
    sleep(2);
  }
}

int main() {

  // Open the log file
  if ((log_fd = open("log/master.log",O_WRONLY|O_APPEND|O_CREAT, 0666)) == -1)
  {
    perror("Error opening log file");
    return 1;
  }
        
  // Get the time when the Master starts its execution
  time_t rawtime;
  struct tm *info;
  time( &rawtime );
  info = localtime( &rawtime );

  //struct sockaddr_in serv_addr, cli_addr;
  int mode;
  do {
    printf("\n\n Here you can choose to run the program in 3 different modalities:\n\n 1) Normal modality;\n 2) Server modality;\n 3) Client modality. \n \n\n Enter 1, 2 or 3:  ");
    scanf("%d", &mode);
  } while (mode < 1 || mode > 3);

  // Write into the log file
  sprintf(log_buffer, "<master_process> Master process started: %s\n", asctime(info));
  if (write(log_fd, log_buffer, strlen(log_buffer)) == -1)
  {
    close(log_fd);
    perror("Error in writing function");
    return 1;
  }

  // Process A
  char mode_str[10];
  if (mode == 2){
    sprintf(mode_str, "%d", mode);
    char * arg_list_A[] = { "/usr/bin/konsole", "-e", "./bin/processA", mode_str,NULL };
    pid_procA = spawn("/usr/bin/konsole", arg_list_A);
    if (pid_procA == -1)
    {
      close(log_fd);
      perror("Error in spawning process A");
      return 1;
    }
  }
  else if (mode == 3){
    sprintf(mode_str, "%d", mode);
    char * arg_list_A[] = { "/usr/bin/konsole", "-e", "./bin/processA", mode_str, NULL };
    pid_procA = spawn("/usr/bin/konsole", arg_list_A);
    if (pid_procA == -1)
    {
      close(log_fd);
      perror("Error in spawning process A");
      return 1;
    }
  }
  else {
    char * arg_list_A[] = { "/usr/bin/konsole", "-e", "./bin/processA", mode_str, NULL };
    pid_procA = spawn("/usr/bin/konsole", arg_list_A);
    if (pid_procA == -1)
    {
      close(log_fd);
      perror("Error in spawning process A");
      return 1;
    }
  }

  // Process B
  char * arg_list_B[] = { "/usr/bin/konsole", "-e", "./bin/processB", NULL };
  pid_procB = spawn("/usr/bin/konsole", arg_list_B);
  if (pid_procB == -1)
  {
    close(log_fd);
    perror("Error in spawning process B");
    return 1;
  }

  // Open the log file for process A
  int fd_pa = open("log/processA.log", O_CREAT | O_RDWR, 0666);
  if (fd_pa == -1)
  {
    close(log_fd);
    perror("Error opening log file");
    return 1;
  }

  // Open the log file for process B
  int fd_pb = open("log/processB.log", O_CREAT | O_RDWR, 0666);
  if (fd_pb == -1)
  {
    close(log_fd);
    close(fd_pa);
    perror("Error opening log file");
    return 1;
  }
 
  // Close the log files
  close(fd_pa);
  close(fd_pb);

  // Start the watchdog
  watchdog();

  // Variable to store the status of the child processes
  int status;

  // Wait for the child processes to terminate
  waitpid(pid_procA, &status, 0);
  waitpid(pid_procB, &status, 0);

  // Get the time when the Master terminates its execution
  time( &rawtime );
  info = localtime(&rawtime);

  // Write into the log file
  sprintf(log_buffer, "<master_process> Master process terminated: %s\n", asctime(info));
  if (write(log_fd, log_buffer, strlen(log_buffer)) == -1)
  {
    close(log_fd);
    perror("Error in writing function");
    return 1;
  }
  
  // Close the log file
  close(log_fd);
  
  printf ("Main program exiting with status %d\n", status);
  return 0;
}

