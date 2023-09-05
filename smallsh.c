#define _POSIX_C_SOURCE 200809L


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>


#define MIN_WORDS 512
#define STDIN_FD 0
#define STDOUT_FD 1


/*-----String find and replace function definition-----*/
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub);


/*-----SIGINT signal handler function defintion (does nothing)-----*/
void handle_SIGINT(int signo) {};


/*-----Convert int to char function definition-----*/
char *int_to_char(int int_value);


int main ()
{
  // Signal handling
  struct sigaction SIGINT_action = {0}, ignore_action = {0};
  ignore_action.sa_handler = SIG_IGN;
  sigaction(SIGTSTP, &ignore_action, NULL);

  
  /*-----Variable definitions-----*/
  char *IFS;
  
  char *line = NULL;
  size_t n = 0;

  char **words = malloc(sizeof(char*) * MIN_WORDS);
  int word_counter = 0;

  pid_t smallsh_pid;
  char *smallsh_pid_char;

  pid_t childPid;
  int childStatus;

  char *fg_proc_ext_stat_char = "0";
  int fg_proc_ext_stat_int = 0;

  char *bg_proc_pid_char = "";
  pid_t bg_proc_pid_int;
  pid_t bg_wait_pid;

  char **args;
  int args_count;

  int bg_proc = -1;
  int infile = -1;
  int outfile = -1;
  int arg_start = 0;
  int arg_end = 0;
  int command_file_flag = 0;

  
  // Infinite loop
  for (;;) {
    /*-----Managing background processes-----*/
    while ((bg_wait_pid = waitpid(0, &childStatus, WNOHANG | WUNTRACED)) > 0) {
      if (WIFEXITED(childStatus)) {
        fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) bg_wait_pid, childStatus);
      }
      else if (WIFSIGNALED(childStatus)) {
        fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) bg_wait_pid, childStatus); 
      }
      else if (WIFSTOPPED(childStatus)) {
        kill(bg_wait_pid, SIGCONT);
        fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) bg_wait_pid);
      }
    }
 
  
    /*-----Printing the prompt and reading line of input-----*/
    if (getenv("PS1") == NULL) fprintf(stderr, "");
    else fprintf(stderr, "%s ", getenv("PS1"));
    

    /*-----Reading a line of input-----*/
    // Register SIGINT handling
    SIGINT_action.sa_handler = handle_SIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);
    
    getline(&line, &n, stdin);
    
    // SIGINT signal on getline return EINTR
    if (errno == EINTR) {
      putchar('\n');
      clearerr(stdin);
      errno = 0;
      continue;
    }

    // Set to ignore SIGINT signal;
    sigaction(SIGINT, &ignore_action, &SIGINT_action);

    // EOF on stdin
    if (feof(stdin)) goto exit_command;


    /*-----Word splitting-----*/
    if (getenv("IFS") == NULL) IFS = " \t\n";
    else IFS = getenv("IFS");
  
    char *token = strtok(line, IFS);
  
    while (token != NULL) {
      words[word_counter] = strdup(token);
      word_counter++;
      token = strtok(NULL, IFS);
    }

    // This is just a test for checking word splitting
    //for (int i = 0; i < word_counter; i++) {
      //printf("%s\n", words[i]);
    //}
    //printf("%d\n", word_counter);
 

    /*-----Parameter expansion-----*/
    for (int i = 0; i < word_counter; i++) {
    
      // Check for leading "~/"
      if (strncmp(words[i], "~/", 2) == 0) {
        str_gsub(&words[i], "~", getenv("HOME"));
      }
    
      // Expansion of "$$"
      smallsh_pid = getpid();
      smallsh_pid_char = int_to_char((intmax_t) smallsh_pid);
      str_gsub(&words[i], "$$", smallsh_pid_char);
    
      // Expansion of "$?"
      fg_proc_ext_stat_char = int_to_char(fg_proc_ext_stat_int);
      str_gsub(&words[i], "$?", fg_proc_ext_stat_char);

      // Expansion of "$!"
      str_gsub(&words[i], "$!", bg_proc_pid_char);
    }
    // Test expansion
    //for (int i = 0; i < word_counter; i++) {
      //printf("%s\n", words[i]);
    //}
  

    /*-----Parsing words-----*/
    // Move forwards through wordslist until end or "#" is reached
    for (int i = 0; i < word_counter; i++) {
      if (strcmp(words[i], "#") == 0) {
        for (int j = i; j < word_counter; j++) {
          free(words[j]);
        }
        word_counter = i;
        break;
      }
    }

    // Look backwards for tokens and arguments
    if (word_counter != 0) {
      if (word_counter - 1 > 0) {
        if (strcmp(words[word_counter-1], "&") == 0) {
          bg_proc = 1;
          if (word_counter - 3 > 0) {
            if ((strcmp(words[word_counter-3], ">") == 0) || (strcmp(words[word_counter-3], "<") == 0)) {
              if (strcmp(words[word_counter-3], ">") == 0) {
                outfile = word_counter - 2;
                arg_end = outfile - 2;
              }
              else {
                infile = word_counter - 2;
                arg_end = infile - 2;
              }
              if (word_counter - 5 > 0) {
                if ((strcmp(words[word_counter-5], ">") == 0) || (strcmp(words[word_counter-5], "<") == 0)) {
                  if ((strcmp(words[word_counter-5], ">") == 0) && (outfile == -1)) {
                    outfile = word_counter - 4;
                    arg_end = outfile - 2;
                  }
                  else if (infile == -1) {
                    infile = word_counter - 4;
                    arg_end = infile - 2;
                  }
                }
              }
            }
          }
        }
        else if (word_counter - 2 > 0) {
          if ((strcmp(words[word_counter-2], ">") == 0) || (strcmp(words[word_counter-2], "<") == 0)) {
            if (strcmp(words[word_counter-2], ">") == 0) {
              outfile = word_counter - 1;
              arg_end = outfile - 2;
            }
            else {
              infile = word_counter - 1;
              arg_end = infile - 2;
            }
            if (word_counter - 4 > 0) {
              if ((strcmp(words[word_counter-4], ">") == 0) || (strcmp(words[word_counter-4], "<") == 0)) {
                if ((strcmp(words[word_counter-4], ">") == 0) && (outfile == -1)) {
                  outfile = word_counter - 3;
                  arg_end = outfile - 2;
                }
                else if (infile == -1) {
                  infile = word_counter - 3;
                  arg_end = infile - 2;
                }
              }
            }
          }
        }
      }
    }

    if (arg_end == 0 && infile == -1 && outfile == -1 && bg_proc == -1 && word_counter > 1) {
      arg_end = word_counter - 1;
    }
    else if (arg_end == 0 && infile == -1 && outfile == -1 && bg_proc == 1 && word_counter > 1) {
      arg_end = word_counter - 2;
    }

    // Test parsing
    // printf("bg_proc: %d\n", bg_proc);
    // printf("infile: %d\n", infile);
    // printf("outfile: %d\n", outfile);
    // printf("arg_start: %d\n", arg_start);
    // printf("arg_end: %d\n", arg_end);
    // printf("command: %d\n", command);
    
    
    /*-----Execution-----*/
    // Silently return if no command word present
    if (word_counter == 0) {
      continue;
    }
  
    // Built-in command "exit"
  exit_command:
    // stdin end of file implied as exit $?
    if (feof(stdin)) {
      kill(0, SIGINT);
      fprintf(stderr, "\nexit\n");
      exit(fg_proc_ext_stat_int);
    }

    if (strcmp(words[0], "exit") == 0) {
      // Too many arguments
      if (arg_end - arg_start > 1) {
        // set $? to error
        fg_proc_ext_stat_int = 1;
        fg_proc_ext_stat_char = int_to_char(fg_proc_ext_stat_int);
        fprintf(stderr, "\nexit\n");
        fprintf(stderr, "smallsh: exit: too many arguments\n");
        goto free_mem;
      }
      else if (arg_end - arg_start == 1) {
        //Argument is non-numeric
        for (size_t i = 0; i < strlen(words[1]); i++) {
          if (words[1][i] < '0' || words[1][i] > '9') {
            fg_proc_ext_stat_int = 1;
            fg_proc_ext_stat_char = int_to_char(fg_proc_ext_stat_int);
            fprintf(stderr, "\nexit\n");
            fprintf(stderr, "smallsh: exit: %s: numeric argument required\n", words[1]);
            goto free_mem;
          }
        }
        // Exit with argument exit status
        fg_proc_ext_stat_char = words[1];
        fg_proc_ext_stat_int = atoi(words[1]);
        kill(0, SIGINT);
        fprintf(stderr, "\nexit\n");
        exit(fg_proc_ext_stat_int);
      }
      else {
        kill(0, SIGINT);
        fprintf(stderr, "\nexit\n");
        exit(fg_proc_ext_stat_int);
      }
    }
    
    // Built-in command "cd"
    if (strcmp(words[0], "cd") == 0) {
      if (arg_end - arg_start > 1) {
        fg_proc_ext_stat_int = 1;
        fg_proc_ext_stat_char = int_to_char(fg_proc_ext_stat_int);
        fprintf(stderr, "smallsh: cd: too many arguments\n");
        goto free_mem;
      }
      else if (arg_end - arg_start == 1) {
        if (chdir(words[1]) == -1) {
          fg_proc_ext_stat_int = 1;
          fg_proc_ext_stat_char = int_to_char(fg_proc_ext_stat_int);
          fprintf(stderr, "smallsh: cd: %s: No such file or directory\n", words[1]);
          goto free_mem;
        }
        else {
          goto free_mem;
        }
      }
      else {
        if (chdir(getenv("HOME")) == -1) {
          fg_proc_ext_stat_int = 1;
          fg_proc_ext_stat_char = int_to_char(fg_proc_ext_stat_int);
          fprintf(stderr, "Error changing working directory to HOME\n");
          goto free_mem;
        }
        else {
          goto free_mem;
        }
      }
    }

    // Non built-in commands
    // Fork a child process
    pid_t spawnpid = fork();

    // Error check fork()
    if (spawnpid == -1) {
      fg_proc_ext_stat_int = 1;
      fg_proc_ext_stat_char = int_to_char(fg_proc_ext_stat_int);
      fprintf(stderr, "Error fork() failed\n");
      goto free_mem;
    }
    
    // Code for child process
    else if (spawnpid == 0) {
      // Reset SIGINT handling to old action
      sigaction(SIGINT, &SIGINT_action, NULL);
      // Check for "/" in command
      for (int i = 0; i < strlen(words[0]); i++) {
        if (words[0][i] == '/') {
          command_file_flag = 1;
          break;
        }
      }
      // Check for input file "<"
      if (infile != -1) {
        // close stdin
        if (close(STDIN_FD) == -1) {
          fprintf(stderr, "Error closing stdin\n");
          exit(1);
        }
        // open infile
        if (open(words[infile], O_RDONLY) == -1) {
          err(errno, "%s", words[infile]);
          exit(1);
        }
      }
      // Check for output file ">"
      if (outfile != -1) {
        // close stdout
        if (close(STDOUT_FD) == -1) {
          fprintf(stderr, "Error closing stdout\n");
          exit(1);
        }
        // open outfile if file doesn't exist create with permission 0777
        int outfile_fd = open(words[outfile], O_WRONLY);
        if (outfile_fd == -1 && errno == ENOENT) {
          outfile_fd = open(words[outfile], O_CREAT, 0777);
          if (outfile_fd == -1) {
            fprintf(stderr, "Error creating outfile\n");
            exit(1);
          }
          close(outfile_fd);
          outfile_fd = open(words[outfile], O_WRONLY);
          if (outfile == -1) {
            fprintf(stderr, "Error opening specified outfile\n");
            exit(1);
          }
        }
      }
      // Execution of command w/ filepath
      if (command_file_flag == 1) {
        args_count = arg_end - arg_start;
        args = malloc(args_count + 2);
        if (args_count == 0) {
          args[0] = words[0];
          args[1] = NULL;
        }
        else {
          for (int i = 0; i < args_count + 1; i++) {
            args[i] = words[i];
          }
          args[args_count+1] = NULL;
        }
        if (execv(args[0], args) == -1) {
          fprintf(stderr, "Error executing command\n");
          exit(1);
        }
      }
      else {
        // search PATH
        args_count = arg_end - arg_start;
        args = malloc(args_count + 2);
        if (args_count == 0) {
          args[0] = words[0];
          args[1] = NULL;
        }
        else {
          for (int i = 0; i < args_count + 1; i++) {
            args[i] = words[i];
          }
          args[args_count+1] = NULL;
        }
        if (execvp(args[0], args) == -1) {
          fprintf(stderr, "Error executing command\n");
          exit(1);
        }
      }
      exit(0);
    }
    
    /*-----Waiting-----*/
    else {
      if (bg_proc == -1) {
        childPid = waitpid(spawnpid, &childStatus, WUNTRACED);
        //printf("childPid: %jd\n", (intmax_t) childPid);
        if (WIFEXITED(childStatus)) {
          fg_proc_ext_stat_int = WEXITSTATUS(childStatus);
          fg_proc_ext_stat_char = int_to_char(fg_proc_ext_stat_int);
        }
        else if (WIFSIGNALED(childStatus)) {
          fg_proc_ext_stat_int = 128 + WTERMSIG(childStatus);
          fg_proc_ext_stat_char = int_to_char(fg_proc_ext_stat_int);
        }
        else if (WIFSTOPPED(childStatus)) {
          bg_proc_pid_int = childPid;
          bg_proc_pid_char = int_to_char((intmax_t) childPid);
          kill(childPid, SIGCONT);
          fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) childPid);
          break;
        }
      }
      else {
        bg_proc_pid_int = spawnpid;
        bg_proc_pid_char = int_to_char((intmax_t) bg_proc_pid_int);
      }
    }

  free_mem:
    // Free any allocated memory
    for (int i = 0; i < word_counter; i++) {
      free(words[i]);
    }
   
    // Reset variables
    word_counter = 0;
    bg_proc = -1;
    infile = -1;
    outfile = -1;
    arg_start = 0;
    arg_end = 0;
    command_file_flag = 0;
  }
}


/*-----String find and replace code (Courtesy Instructor Gambord via https://www.youtube.com/watch?v=-3ty5W_6-IQ)-----*/
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub)
{
  char *str = *haystack;
  size_t haystack_len = strlen(str);
  size_t const needle_len = strlen(needle); 
  size_t const sub_len = strlen(sub);

  for (; (str = strstr(str, needle));) {
    ptrdiff_t off = str - *haystack;
    if (sub_len > needle_len) {
      str = realloc(*haystack, sizeof **haystack * (haystack_len + sub_len - needle_len + 1));
      if (!str) goto exit;
      *haystack = str;
      str = *haystack + off;
    }
    memmove(str + sub_len, str + needle_len, haystack_len + 1 - off - needle_len);
    memcpy(str, sub, sub_len);
    haystack_len = haystack_len + sub_len - needle_len;
    str += sub_len;
  }
  str = *haystack;
  if (sub_len < needle_len) {
    str = realloc(*haystack, sizeof **haystack * (haystack_len + 1));
    if (!str) goto exit;
    *haystack = str;
  }
exit:
  return str;
}


/*-----Code for pid int to pid char function*/
char *int_to_char(int int_value) {
  size_t digit_count = 0;
  int tmp_int = int_value;
  do {
    tmp_int = tmp_int / 10;
    digit_count++;
  } while (tmp_int != 0);
  char *tmp_char = malloc(digit_count+1);
  sprintf(tmp_char, "%d", int_value);
  return tmp_char;
}
