/*
 * Rub3cK0r3 - my interactive terminal
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 */

#include <bits/posix2_lim.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "xshll.h"

/*
 * A shell reads user input, interprets the commands, and
 * executes them by forking child processes.
 * The standard C library provides functions like fork(), exec(), and wait()
 * that help us achieve this.
 *
 * How does this work??
 * The program sets up a loop to keep the shell running until the user types
 * "exit." It prompts the user for input and reads the command from the standard
 * input using fgets(). The input is then tokenized into arguments using
 * strtok() and stored in the args array. The shell checks if the command is
 * "exit"; if so, it exits the loop. If not, it forks a child process using
 * fork(). In the child process, the command is executed using execvp(), which
 * searches for the command in the system's PATH and runs it. The parent process
 * waits for the child to complete using wait().
 */

/**
 * Function used to initialize our shell. We used the approach explained in
 * http://www.gnu.org/software/libc/manual/html_node/Initializing-the-Shell.html
 */

void welcomeScreen() {
  // FUNNY WELCOME SCREEN OF XSHLL
  printf("                .__     .__   .__   \n");
  printf("___  ___  ______|  |__  |  |  |  |  \n");
  printf("\\  \\/  / /  ___/|  |  \\ |  |  |  |  \n");
  printf(" >    <  \\___ \\ |   Y  \\|  |__|  |__\n");
  printf("/__/\\_ \\/____  >|___|  /|____/|____/\n");
  printf("      \\/     \\/      \\/             \n");
}

void showHelp() {
  // MINIMAL HELP SCREEN OF XSHLL
  printf("XSHLL - A minimal custom shell\n");
  printf("Built-in Commands:\n");
  printf("  xhelp             Display this help message\n");
  printf("  exit              Exit the shell\n");
  printf("  cd [DIR]          Change directory to DIR\n");
  printf("  pwd               Print current working directory\n");
  printf("  echo [TEXT]       Display TEXT\n");
}

void shellPrompt() {
  /*
   * The  gethostname() function shall return the standard host name for the
   * current machine.
   *
   * The namelen argument shall specify the size of the array pointed to by  the
   * name  argument.  The returned name shall be null-terminated, except that
   * if namelen is an insufficient length to hold the host name, then the
   * returned name shall be truncated and it  is unspecified whether the
   * returned name is null-terminated.
   *
   * Host names are limited to {HOST_NAME_MAX} bytes.
   * getlogin()  returns  a  pointer to a string containing the name of the user
   * logged in on the controlling terminal of the process, or a null pointer if
   * this information cannot be determined.

   * The  getcwd()  function  copies an absolute pathname of the current working
   * directory to the array pointed to by buf, which is of length size.
   * If the length of the absolute pathname of the current working directory,
   * including the  terminating  null  byte, exceeds  size  bytes,  NULL is
   * returned, and errno is set to ERANGE; an application should check for this
   * error, and allocate a larger buffer if necessary.
   */
  char hostname[HOST_NAME_MAX + 1] = "unknown";
  if (gethostname(hostname, sizeof(hostname)) != 0) {
    perror("Error: gethostname");
  }

  char *username = getlogin();

  char *cwd = getcwd(NULL, 0); // asignación dinámica segura
  if (!cwd) {
    perror("Error: getcwd");
    cwd = strdup("unknown");
  }

  // Mostrar ~ si estamos en el HOME
  const char *home = getenv("HOME");
  if (home && strstr(cwd, home) == cwd) {
    printf("%s@%s:~%s$ ", username, hostname, cwd + strlen(home));
  } else {
    printf("%s@%s:%s$ ", username, hostname, cwd);
  }
  free(cwd);
}

int changeDirectory(char *args[]) {
  /*
   * The  chdir()  function shall cause the directory
   * named by the pathname pointed to by the path
   * argument to become the current working  directory;
   * that  is,  the  starting point for path searches
   * for pathnames not beginning with '/'.
   *
   * Upon successful completion, 0 shall be returned.
   * Otherwise, -1 shall  be  returned,  the  current
   * working  directory  shall  remain unchanged, and
   * errno shall be set to indicate the error.
   */

  // For now we are not going to dive inte the path
  // because we havent done the parser yet
  int ret = chdir(args[1]);
  return ret;
}
// Function to handle (parsing) pipes but after this we still got a lot of work
// to do
char **handlePipes(char *args[256]) {
  // Dynamic memory allocated memory scpace, once everything done, free.
  char **commands = malloc(sizeof(char *) * MAX_ARGS);
  // If the allocation wasnt completed
  if (!commands) {
    perror("malloc");
    exit(1);
  }

  char buffer[MAX_LINE]; // I create a buffer
  int i = 0;             // Index in args
  int j = 0;             // Index in commands
  buffer[0] = '\0';      // I terminate the buffer

  // Loop for asigning the whole command to args to be parsed after..
  while (args[i] != NULL) {
    // Accumulate command tokens until "|"
    while (args[i] != NULL && strcmp(args[i], "|") != 0) {
      strcat(buffer, args[i]);
      strcat(buffer, " ");
      i++;
    }

    // Trim trailing space of the expression that we get after the loop..
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == ' ') {
      buffer[len - 1] = '\0';
    }

    commands[j++] = strdup(buffer); // Copy the command string
    buffer[0] = '\0';               // Clear buffer for next command

    if (args[i] != NULL && strcmp(args[i], "|") == 0) {
      i++; // Skip the "|"
    }
  }

  commands[j] = NULL; // Null-terminate the array
  return commands;
}

// Function to lauch any program with execvp, basically finally working with the
// pipeline once we got the command table
void executePipeline(char **commands) {
  int numCommands = 0;

  // Loop for getting the number of commands in the pipeline
  while (commands[numCommands] != NULL)
    numCommands++;

  int pipefd[2]; // fd = filedecryptor
  int in_fd = 0; // start reading from stdin
  pid_t pid;     // pid of the proccess

  // for each command in the pipeline we have to make some implementations
  for (int i = 0; i < numCommands; i++) {
    char *cmd = strdup(commands[i]); // Don't modify original, using strdup
    char *args[64];
    int j = 0;

    // Tokenize command string into args[]
    args[j] = strtok(cmd, " ");
    while (args[j] != NULL && j < 63) {
      args[++j] = strtok(NULL, " ");
    }
    args[j] = NULL;

    if (i < numCommands - 1) {
      if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
      }
    }

    /* proccess management */
    pid = fork();
    if (pid == 0) {
      if (in_fd != 0) {
        dup2(in_fd, 0); // stdin
        close(in_fd);
      }

      if (i < numCommands - 1) {
        close(pipefd[0]);   // close read end
        dup2(pipefd[1], 1); // stdout
        close(pipefd[1]);
      }

      execvp(args[0], args);
      perror("execvp");
      exit(1);
    } else {
      wait(NULL); // Wait for child (could be optimized later)

      free(cmd);
      if (in_fd != 0)
        close(in_fd);
      if (i < numCommands - 1) {
        close(pipefd[1]);  // close write end
        in_fd = pipefd[0]; // next command reads from here
      }
    }
  }
}

int handleBuiltIn(char **args) {
  // Implementation for the main built-in commands of this shell
  // printf("args[0] -> %s\n", args[0]);
  if (strcmp(args[0], "exit") == 0)
    exit(EXIT_SUCCESS);
  else if (strcmp(args[0], "cd") == 0) {
    if (args[1] == NULL) {
      // TODO:Doesnt exist the argument for cd BUT we could implement that just
      // cd goes to the personal folder
      fprintf(stderr, "shell: se esperaba un argumento para \"cd\"\n");
    } else {
      // I call my own function to change directory
      // printf("Si segv es aqui el fallo es el change directory");
      changeDirectory(args);
    }
    return 0;
  } else if (strcmp(args[0], "echo") == 0) {
    // TODO: No viene implementada la redireccion
    // la redireccion implicaría manipulacion de ficheros y parsear input
    // completo para ver en cuando tenemos que sustituir el contenido (>) o
    // añadir el contenido (>>)
    for (int i = 1; args[i] != NULL; i++) {
      printf("%s", args[i]);
      if (args[i + 1] != NULL)
        printf(" ");
    }
    printf("\n");
    // OUR OWN BUILT-IN FOR SHOWING HELP ->> xhelp command
  } else if (strcmp(args[0], "xhelp") == 0) {
    showHelp();
  } else {
    return 1;
  }
}

int main() {
  // We print the ASCII art banner of XSHLL
  welcomeScreen();
  char input[MAX_LINE]; // basically, user input
  char *args[LIMIT]; // arguments, delimited with Pipes and so on and so forth..

  for (;;) {
    // We show the prompt of the user
    shellPrompt();
    // restrictions for the user to put the input
    // input has to be lower than MAX_LINE bytes can occupy
    if (!fgets(input, MAX_LINE, stdin))
      break;
    // we end the user's input to be able to manipulate it
    input[strcspn(input, "\n")] = '\0';

    // Loop for tokenize the input to be able to parse it:
    // Create Command table
    int i = 0;
    args[i] = strtok(input, " ");
    while (args[i] != NULL && i < MAX_ARGS - 1) {
      args[++i] = strtok(NULL, " ");
    }
    args[i] = NULL;

    // printf("\nDebugging.. : %s\n", *args);

    // if the input is NULL from the beginning he just hit enter
    // printf("args[0] es null? -> ARGS[0] = %s\n", args[0]);
    if (args[0] == NULL)
      continue;

    char **handled;
    // I use the handle pipes function
    handled = handlePipes(args);

    if (handled[1] != NULL) {
      // There are no pipes, so we execute the whole command without split
      executePipeline(handled);
    } else {
      // Just a comand, it can be built-in Tokenize if built-in
      char *singleCmd = strdup(handled[0]);

      // argv ->> tokegnized command (singleCmd)
      char *argv[64];
      int i = 0;
      argv[i] = strtok(singleCmd, " ");
      while (argv[i] != NULL && i < 63) {
        argv[++i] = strtok(NULL, " ");
      }
      argv[i] = NULL; // Obviously sequence terminated with null

      // If the command is not built in we execute
      if (handleBuiltIn(argv) == 1) {
        executePipeline(handled);
      }
      free(singleCmd);
      singleCmd = NULL;
    }

    // Final loop for freeing each command in handled after using it for all the
    // functionality of before
    for (int i = 0; handled[i] != NULL; i++) {
      free(handled[i]); // Free each command
    }
    // we also free the now empty char[] of pointers that now dont exist "handled" 
    free(handled);
    handled = NULL; // optional but optimal
    
  }
  return 0;
}

/*
set expandtab
set tabstop=4
set shiftwidth=4
*/
