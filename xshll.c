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
  printf("XSHLL - A minimal custom shell\n");
  printf("Built-in Commands:\n");
  printf("  help              Display this help message\n");
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
   * if namelen is an insuffi‐ cient length to hold the host name, then the
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
    if (!username) {
        struct passwd *pw = getpwuid(getuid());
        username = pw ? pw->pw_name : "unknown";
    }

    char *cwd = getcwd(NULL, 0);  // asignación dinámica segura
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
  int ret = chdir(*args);
  return ret;
}
int handleBuiltIn(char * args[256]){
    if (strcmp(args[0], "exit") == 0)
      exit(EXIT_SUCCESS);
    else if (strcmp(args[0], "cd") == 0) {
      if (args[1] == NULL) {
        fprintf(stderr, "shell: se esperaba un argumento para \"cd\"\n");
      } else {
        if (chdir(args[1]) != 0) {
          perror("shell");
        }
      }
      return 0;
    } else if (strcmp(args[0], "echo") == 0) {
      //No viene implementada la redireccion
      for (int i = 1; args[i] != NULL; i++) {
        printf("%s", args[i]);
        if (args[i + 1] != NULL)
          printf(" ");
      }
      printf("\n");
    } else {
      return 1;
    }
}

int main() {
  // We print the ASCII art banner
  welcomeScreen();
  char input[MAX_LINE];
  char *args[LIMIT];

  for (;;) {
    // We show the prompt to the user
    shellPrompt();
    if (!fgets(input, MAX_LINE, stdin))
      break;
    input[strcspn(input, "\n")] = '\0';

    int i = 0;
    args[i] = strtok(input, " ");
    while (args[i] != NULL && i < MAX_ARGS - 1) {
      args[++i] = strtok(NULL, " ");
    }
    args[i] = NULL;

    if (args[0] == NULL)
      continue;

    if (handleBuiltIn(args) == 1){
      pid_t pid = fork();
      if (pid == 0) {
        execvp(args[0], args);
        perror("exec");
        exit(1);
      } else {
        wait(NULL);
      }
    }
  }
  return 0;
}

/*
set expandtab
set tabstop=4
set shiftwidth=4
*/
