#include "msgs.h"
#define _POSIX_C_SOURCE 200809L
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAX_LEN 10

void print_cwd(bool internal);
char *get_input();
char **tokenize_input(char *, bool *);
void exec_cmds(char **, bool);
bool check_buildins(char **, char **, int *, int);
void exec_buildins(const int, char **);
void exec_special_buildins(char **);
char *change_dir(char *);
void print_help(char **);

void registerSignalHandler();
void handle_sigint(int signum);
void display_all_help();

void handle_usersig(int signum);
void registerUserSignal();

void remove_oldest_record();
void add_to_history(char *);
void print_history();

int history_count = 0;
int true_history_count = 0;
int true_history_arr[MAX_LEN];
char *input_history[MAX_LEN];
bool executed = false;

// Global variables
char *internal_cmds[] = {"exit", "pwd", "cd", "help", "history"};
char *prev_dir = NULL;

int main() {

  int len = sizeof(internal_cmds) / sizeof(internal_cmds[0]);

  char **cmds;
  char *input;

  registerSignalHandler();
  registerUserSignal();

  while (1) {

    bool background = false;
    executed = false;
    int cmd_num = 0;

    print_cwd(false);

    input = get_input();
    char *input_dup = strdup(input);
    input_dup[strlen(input_dup) - 1] = '\0';

    // tokenize string
    cmds = tokenize_input(input, &background);

    if (check_buildins(internal_cmds, cmds, &cmd_num, len)) {
      exec_buildins(cmd_num, cmds);
    } else {
      exec_cmds(cmds, background);
    }

    if (executed) {
      if (strcmp(input_dup, "history") != 0) {
        add_to_history(input_dup);
      }
    }
    // Cleanup
    waitpid(-1, NULL, WNOHANG);
    free(input_dup);
    free(input);
    free(cmds);
  }
  return 0;
}
// Input histroy
//-------------------------------------------------------------------------------------
void remove_oldest_record() {
  if (history_count > 0) {
    free(input_history[0]);
    for (int i = 1; i < history_count; i++) {
      input_history[i - 1] = input_history[i];
      true_history_arr[i - 1] = true_history_arr[i];
    }
    history_count--;
  }
}

void add_to_history(char *input) {
  if (history_count >= MAX_LEN) {
    remove_oldest_record();
  }
  input_history[history_count] = strdup(input);
  // Handle memory allocation
  if (input_history[history_count] == NULL) {
    const char *err_msg = "Memory Allocation Failed";
    write(STDERR_FILENO, err_msg, strlen(err_msg));
    exit(EXIT_FAILURE);
  }
  true_history_arr[history_count] = true_history_count;
  history_count++;
  true_history_count++;
}

void print_history() {
  add_to_history("history");
  for (int i = history_count - 1; i >= 0; i--) {
    char history[1024];
    snprintf(history, sizeof(history), "%d\t%s\n", true_history_arr[i],
             input_history[i]);
    write(STDIN_FILENO, history, strlen(history));
  }
}

// Signal handlers
//---------------------------------------------------------------------------------------
void display_all_help() {
  const char *help_msg = FORMAT_MSG("help", HELP_HELP_MSG);
  const char *exit_msg = FORMAT_MSG("exit", EXIT_HELP_MSG);
  const char *pwd_msg = FORMAT_MSG("pwd", PWD_HELP_MSG);
  const char *cd_msg = FORMAT_MSG("cd", CD_HELP_MSG);
  const char *history_msg = FORMAT_MSG("history", HISTORY_HELP_MSG);

  write(STDOUT_FILENO, help_msg, strlen(help_msg));
  write(STDOUT_FILENO, exit_msg, strlen(exit_msg));
  write(STDOUT_FILENO, pwd_msg, strlen(pwd_msg));
  write(STDOUT_FILENO, cd_msg, strlen(cd_msg));
  write(STDOUT_FILENO, history_msg, strlen(history_msg));
}

void handle_sigint(int signum) {
  write(STDOUT_FILENO, "\n", 1);

  display_all_help();
  print_cwd(false);
}

void registerSignalHandler() {
  struct sigaction handler;
  handler.sa_handler = handle_sigint;
  handler.sa_flags = SA_RESTART;
  sigemptyset(&handler.sa_mask);

  if (sigaction(SIGINT, &handler, NULL) == -1) {
    const char *err_msg =
        FORMAT_MSG("shell", "failed to register signal handler\n");
    write(STDERR_FILENO, err_msg, strlen(err_msg));
    exit(EXIT_FAILURE);
  }
}

void handle_usersig(int signum) {

  printf("IM HERE\n");
  executed = true;
}

void registerUserSignal() {
  struct sigaction handler;
  handler.sa_handler = handle_usersig;
  handler.sa_flags = 0;
  sigemptyset(&handler.sa_mask);

  if (sigaction(SIGUSR1, &handler, NULL) == -1) {
    const char *err_msg =
        FORMAT_MSG("shell", "failed to register signal handler\n");
    write(STDERR_FILENO, err_msg, strlen(err_msg));
    exit(EXIT_FAILURE);
  }
}

// BUILDINS
// -------------------------------------------------------------------------------------------
// print help msgs
void print_help(char **cmds) {

  // help msgs
  const char *help_msg = FORMAT_MSG("help", HELP_HELP_MSG);
  const char *exit_msg = FORMAT_MSG("exit", EXIT_HELP_MSG);
  const char *pwd_msg = FORMAT_MSG("pwd", PWD_HELP_MSG);
  const char *cd_msg = FORMAT_MSG("cd", CD_HELP_MSG);
  const char *history_msg = FORMAT_MSG("history", HISTORY_HELP_MSG);

  char general_msg[100];

  // Print help for all commands
  if (cmds[1] == NULL) {
    write(STDOUT_FILENO, help_msg, strlen(help_msg));
    write(STDOUT_FILENO, exit_msg, strlen(exit_msg));
    write(STDOUT_FILENO, pwd_msg, strlen(pwd_msg));
    write(STDOUT_FILENO, cd_msg, strlen(cd_msg));
    write(STDOUT_FILENO, history_msg, strlen(history_msg));

    // Too many arguments
  } else if (cmds[1] != NULL && cmds[2] != NULL) {
    const char *err_msg = FORMAT_MSG("help", TMA_MSG);
    write(STDERR_FILENO, err_msg, strlen(err_msg));

    // Help for specific command
  } else if (cmds[1] != NULL) {

    if (strcmp(cmds[1], internal_cmds[0]) == 0) {
      write(STDOUT_FILENO, exit_msg, strlen(exit_msg));
    } else if (strcmp(cmds[1], internal_cmds[1]) == 0) {
      write(STDOUT_FILENO, pwd_msg, strlen(pwd_msg));
    } else if (strcmp(cmds[1], internal_cmds[2]) == 0) {
      write(STDOUT_FILENO, cd_msg, strlen(cd_msg));
    } else if (strcmp(cmds[1], internal_cmds[3]) == 0) {
      write(STDOUT_FILENO, help_msg, strlen(help_msg));
    } else if (strcmp(cmds[1], internal_cmds[4]) == 0) {
      write(STDOUT_FILENO, history_msg, strlen(history_msg));
    } else {
      snprintf(general_msg, sizeof(general_msg), "%s: %s\n", cmds[1],
               EXTERN_HELP_MSG);
      write(STDOUT_FILENO, general_msg, strlen(general_msg));
    }
  }

  executed = true;
}

// Execute buildins
void exec_buildins(const int cmd_num, char **cmds) {
  switch (cmd_num) {
  // Exit
  case 0:
    if (cmds[1] != NULL) {
      const char *err_msg = FORMAT_MSG("exit", TMA_MSG);
      write(STDERR_FILENO, err_msg, strlen(err_msg));
    } else {
      exit(EXIT_SUCCESS);
    }
    break;

  // PWD
  case 1:
    if (cmds[1] != NULL) {
      const char *err_msg = FORMAT_MSG("pwd", TMA_MSG);
      write(STDERR_FILENO, err_msg, strlen(err_msg));
    } else {
      print_cwd(true);
    }
    break;

  // Change Directory - cd
  case 2:
    exec_special_buildins(cmds);
    break;

  // Help me
  case 3:
    print_help(cmds);
    break;

  case 4:
    print_history();
    break;
  }
}

// Find is a cmd is a build in
bool check_buildins(char **internal_cmds, char **input, int *cmd_num, int len) {
  bool valid_cmd = false;

  for (int i = 0; i < len; i++) {
    if (input[0] != NULL && (strcmp(input[0], internal_cmds[i]) == 0)) {
      valid_cmd = true;
      *cmd_num = i;
    }
  }

  return valid_cmd;
}

// Execute custom cmds
void exec_special_buildins(char **cmds) {
  if (cmds[1] == NULL) {
    // Go to home dir
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    char *old_dir = change_dir(pw->pw_dir);
    if (old_dir != NULL) {
      prev_dir = old_dir;
    }
    return;

  } else if (cmds[2] != NULL) {
    // too many args
    const char *err_msg = FORMAT_MSG("cd", TMA_MSG);
    write(STDERR_FILENO, err_msg, strlen(err_msg));
    return;
  }

  // check for special cmd -
  if (strcmp(cmds[1], "-") == 0) {
    char *old_dir = change_dir(prev_dir);
    if (old_dir != NULL) {
      prev_dir = old_dir;
    }
    return;
  }

  // String manipulations
  char spc_char[2] = {'\0'};
  char new_dir[1024] = {'\0'};

  int len = strlen(cmds[1]);
  int spc_len = 0;
  char *home_dir;

  spc_char[0] = cmds[1][0];

  // check for special cmd ~
  if (strcmp(spc_char, "~") == 0) {
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    home_dir = pw->pw_dir;
    spc_len = strlen(home_dir);

    // Create new path
    for (int i = 0; i < spc_len; i++) {
      new_dir[i] = home_dir[i];
    }

    int j = 1;
    for (int i = spc_len; i < (spc_len + len); i++) {
      new_dir[i] = cmds[1][j];
      j++;
    }

    char *old_dir = change_dir(new_dir);
    if (old_dir != NULL) {
      prev_dir = old_dir;
    }
    return;
  }

  // Change directory for normal path
  char *old_dir = change_dir(cmds[1]);
  if (old_dir != NULL) {
    prev_dir = old_dir;
  }
}

// Change directory
char *change_dir(char *dir) {
  // Get current dir
  char *current = getcwd(NULL, 0);
  if (current == NULL) {
    const char *err_msg = FORMAT_MSG("shell", GETCWD_ERROR_MSG);
    write(STDERR_FILENO, err_msg, strlen(err_msg));
    return NULL;
  }

  if (chdir(dir) == -1) {
    const char *err_msg = FORMAT_MSG("cd", CHDIR_ERROR_MSG);
    write(STDERR_FILENO, err_msg, strlen(err_msg));
    free(current);
    return NULL;
  }

  if (prev_dir != NULL) {
    free(prev_dir);
  }

  executed = true;

  return current;
}

void print_cwd(bool internal) {
  char *cwd = getcwd(NULL, 0);

  if (cwd == NULL) {
    const char *err_msg = FORMAT_MSG("shell", GETCWD_ERROR_MSG);
    write(STDERR_FILENO, err_msg, strlen(err_msg));
    return;
  }

  write(STDOUT_FILENO, cwd, strlen(cwd));
  if (internal) {
    write(STDOUT_FILENO, "\n ", 1);
    executed = true;
  } else {
    write(STDOUT_FILENO, "$ ", 2);
  }

  free(cwd);
}

// Get INPUT and execute it
// --------------------------------------------------------------------------------------------
char *get_input() {
  int MAX_INPUT = 100;
  char *buff = (char *)malloc(MAX_INPUT * sizeof(char));
  if (buff == NULL) {
    const char *err_msg = "Memory Allocation Failed";
    write(STDERR_FILENO, err_msg, strlen(err_msg));
    exit(EXIT_FAILURE);
  }
  ssize_t bytes_read;

  bytes_read = read(STDIN_FILENO, buff, MAX_INPUT);

  if (bytes_read == -1) {
    const char *err_msg = FORMAT_MSG("shell", READ_ERROR_MSG);
    write(STDERR_FILENO, err_msg, strlen(err_msg));
  }

  // remove newline
  buff[bytes_read] = '\0';
  return buff;
}

char **tokenize_input(char *input, bool *background) {
  int MAX_TOKENS = 100;
  char **tokens = (char **)malloc(MAX_TOKENS * sizeof(char *));
  if (tokens == NULL) {
    const char *err_msg = "Memory Allocation Failed";
    write(STDERR_FILENO, err_msg, strlen(err_msg));
    exit(EXIT_FAILURE);
  }

  char *string = input;
  char *delim = " \n\t\r";
  char *saveptr = NULL;
  char *token = NULL;
  int i = 0;

  while ((token = strtok_r(string, delim, &saveptr))) {
    tokens[i] = token;
    string = NULL;
    i++;
  }

  // Look for &
  if (i > 1 && (strcmp(tokens[i - 1], "&") == 0)) {
    *background = true;
    tokens[i - 1] = NULL;
  }
  tokens[i] = NULL;

  return tokens;
}

void exec_cmds(char **cmds, bool background) {
  pid_t pid = fork();
  pid_t wpid = 0;
  int wstatus;
  if (pid == -1) {
    const char *err_msg = FORMAT_MSG("shell", FORK_ERROR_MSG);
    write(STDERR_FILENO, err_msg, strlen(err_msg));
    exit(EXIT_FAILURE);
  }

  if (pid) {
    if (!background) {
      wpid = waitpid(pid, &wstatus, 0);
    }
    // wait for child
    if (wpid == -1) {
      const char *err_msg = FORMAT_MSG("shell", WAIT_ERROR_MSG);
      write(STDERR_FILENO, err_msg, strlen(err_msg));
      exit(EXIT_FAILURE);
    }

    if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) {
      executed = true;
    }
  } else {
    // execute child
    if (execvp(cmds[0], cmds) == -1) {
      const char *err_msg = FORMAT_MSG("shell", EXEC_ERROR_MSG);
      write(STDERR_FILENO, err_msg, strlen(err_msg));
      exit(EXIT_FAILURE);
    }
    kill(getppid(), SIGUSR1);
  }
}
