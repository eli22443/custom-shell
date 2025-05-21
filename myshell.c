#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <linux/limits.h>
#include "LineParser.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <signal.h>
#include <fcntl.h>

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0
#define HISTLEN 20 // Define the maximum history length
#define FREE(X) \
  if (X)        \
  free((void *)X)

typedef struct process
{
  cmdLine *cmd;         /* the parsed command line*/
  pid_t pid;            /* the process id that is running the command*/
  int status;           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
  struct process *next; /* next process in chain */
} process;

typedef struct history_entry
{
  char *command; // The unparsed command line
  struct history_entry *next;
} history_entry;

history_entry *history_head = NULL;
history_entry *history_tail = NULL;
int history_count = 0; // Current number of entries in the history

int debug = 0;
process *process_list = NULL;
// Frees one line
void freeLine(cmdLine *pCmdLine)
{
  int i;
  if (!pCmdLine)
    return;

  FREE(pCmdLine->inputRedirect);
  FREE(pCmdLine->outputRedirect);
  for (i = 0; i < pCmdLine->argCount; ++i)
    FREE(pCmdLine->arguments[i]);

  FREE(pCmdLine);
}
/**Receive a process list (process_list), a command (cmd), and the process id (pid) of the process running the command.
 * Note that process_list is a pointer to a pointer so that we can insert at the beginning of the list if we wish. */
void addProcess(process **process_list, cmdLine *cmd, pid_t pid)
{
  process *new_process = (process *)malloc(sizeof(process));
  new_process->cmd = cmd;
  new_process->pid = pid;
  new_process->status = RUNNING;
  new_process->next = NULL;
  process *curr = *process_list;
  if (!curr)
  {
    *process_list = new_process;
    return;
  }
  while (curr->next)
  {
    curr = curr->next;
  }

  curr->next = new_process;
}

void updateProcessList(process **process_list)
{
  process *curr = *process_list;
  process *prev = NULL;

  while (curr)
  {
    int status;
    pid_t pid = curr->pid;
    int wait_result = waitpid(pid, &status, WNOHANG | WUNTRACED);

    if (wait_result == -1)
    {
      // Remove node from linked list
      if (!prev)
      {
        // First node
        *process_list = curr->next;
        freeLine(curr->cmd);
        free(curr);
        curr = *process_list;
      }
      else
      {
        prev->next = curr->next;
        freeLine(curr->cmd);
        free(curr);
        curr = prev->next;
      }
    }
    else if (wait_result == pid)
    {
      // Process changed state (prossess terminated)

      if (WIFEXITED(status))
      {
        // Process terminated normally
        curr->status = TERMINATED;
        fprintf(stderr, "Process %d terminated normally with exit status %d\n", pid, WEXITSTATUS(status));
      }
      else if (WIFSIGNALED(status))
      {
        // Process terminated due to a signal
        curr->status = TERMINATED;
        fprintf(stderr, "Process %d terminated by signal %d\n", pid, WTERMSIG(status));
      }
      else if (WIFSTOPPED(status))
      {
        // Process was stopped by a signal
        curr->status = SUSPENDED;
        fprintf(stderr, "Process %d stopped by signal %d\n", pid, WSTOPSIG(status));
      }
      else
      {
        // Process was resumed by a signal
        curr->status = RUNNING;
        fprintf(stderr, "Process %d continued\n", pid);
      }
      prev = curr;
      curr = curr->next;
    }
    else
    {
      // process is still running
      prev = curr;
      curr = curr->next;
    }
  }
}
/*print the processes*/
void printProcessList(process **process_list)
{
  updateProcessList(process_list);
  int pid;
  char command[256];
  char *status;
  process *curr = *process_list;
  process *prev = NULL;

  printf("PID          Command      STATUS\n");
  while (curr)
  {
    pid = curr->pid;
    strcpy(command, curr->cmd->arguments[0]);
    if (curr->status == 1)
      status = "Running";
    else if (curr->status == 0)
      status = "Suspended";
    else
    {
      status = "Terminated";
      if (!prev)
      {
        // Removing the head of the list
        *process_list = curr->next;
        freeLine(curr->cmd);
        free(curr);
        curr = *process_list;
        printf("%d      %s        %s\n", pid, command, status);
        continue;
      }
      else
      {
        // Removing from the middle or end of the list
        prev->next = curr->next;
        freeLine(curr->cmd);
        free(curr);
        curr = prev;
      }
    }
    printf("%d      %s        %s\n", pid, command, status);
    prev = curr;
    curr = curr->next;
  }
}
// free all memory allocated for the process list.
void freeProcessList(process *process_list)
{
  if (!process_list)
    return;

  freeLine(process_list->cmd);
  freeProcessList(process_list->next);
  free(process_list);
}

void updateProcessStatus(process *process_list, int pid, int status)
{
  process *curr = process_list;

  while (curr)
  {
    if (curr->pid == pid)
    {
      curr->status = status;
      return;
    }
    curr = curr->next;
  }
  fprintf(stderr, "Warning: Process with PID %d not found in process list.\n", pid);
}

void addHistory(const char *command_line)
{
  if (command_line == NULL)
    return;

  history_entry *new_entry = (history_entry *)malloc(sizeof(history_entry));
  if (new_entry == NULL)
  {
    perror("malloc (addHistory)");
    return; // Or exit
  }

  new_entry->command = strdup(command_line); // IMPORTANT: Copy the command
  if (new_entry->command == NULL)
  {
    perror("strdup (addHistory)");
    free(new_entry);
    return;
  }

  new_entry->next = NULL; // New entry is the last in the list

  if (history_head == NULL)
  {
    // First entry in the list
    history_head = new_entry;
    history_tail = new_entry;
  }
  else
  {
    // Add to the end of the list
    history_tail->next = new_entry;
    history_tail = new_entry;
  }

  history_count++;

  // Enforce HISTLEN limit (circular buffer behavior)
  if (history_count > HISTLEN)
  {
    // Remove the oldest entry (from the head)
    history_entry *temp = history_head;
    history_head = history_head->next;
    free(temp->command); // Free the command string
    free(temp);          // Free the history entry itself
    history_count--;
  }
}

void printHistory()
{
  history_entry *curr = history_head;
  int index = 1; // Start indexing at 1

  while (curr != NULL)
  {
    printf("%d %s\n", index, curr->command);
    curr = curr->next;
    index++;
  }
}

char *getHistoryCommand(int index)
{
  if (index < 1 || index > history_count)
  {
    fprintf(stderr, "Error: Invalid history index\n");
    return NULL;
  }

  history_entry *current = history_head;
  int i = 1;

  while (current != NULL && i < index)
  {
    current = current->next;
    i++;
  }

  if (current != NULL)
  {
    return current->command;
  }
  else
  {
    return NULL; // Should not happen, but good to be safe
  }
}

void freeHistory(history_entry *history)
{
  if (!history)
    return;

  free(history->command);
  freeHistory(history->next);
  free(history);
}

void execute(cmdLine *pCmdLine)
{
  if (strcmp(pCmdLine->arguments[0], "hist") == 0)
  {
    printHistory();
    freeLine(pCmdLine);
    return;
  }

  if (strcmp(pCmdLine->arguments[0], "cd") == 0)
  {
    if (pCmdLine->argCount < 2)
    {
      fprintf(stderr, "cd: missing argument\n");
    }
    else if (chdir(pCmdLine->arguments[1]) == -1)
    {
      perror("cd failed");
    }
    freeLine(pCmdLine);
    return;
  }

  if (strcmp(pCmdLine->arguments[0], "halt") == 0 ||
      strcmp(pCmdLine->arguments[0], "wakeup") == 0 ||
      strcmp(pCmdLine->arguments[0], "ice") == 0)
  {
    if (pCmdLine->argCount < 2)
    {
      fprintf(stderr, "%s: missing PID\n", pCmdLine->arguments[0]);
      return;
    }

    int pid = atoi(pCmdLine->arguments[1]);
    int status;
    int signal;

    if (strcmp(pCmdLine->arguments[0], "halt") == 0)
    {
      status = SUSPENDED;
      signal = SIGSTOP;
    }
    else if (strcmp(pCmdLine->arguments[0], "wakeup") == 0)
    {
      status = RUNNING;
      signal = SIGCONT;
    }
    else // ice
    {
      status = TERMINATED;
      signal = SIGINT;
    }

    if (kill(pid, signal) == -1)
    {
      perror("kill failed");
    }
    updateProcessStatus(process_list, pid, status); // Update status!
    freeLine(pCmdLine);
    return;
  }

  if (strcmp(pCmdLine->arguments[0], "procs") == 0)
  {
    printProcessList(&process_list);
    freeLine(pCmdLine);
    return;
  }

  int fd[2];
  if (pipe(fd) == -1)
  {
    perror("pipe failed");
    return;
  }

  pid_t pid = fork();

  if (pid == 0)
  {
    if (pCmdLine->inputRedirect != NULL)
    {
      int fdr = open(pCmdLine->inputRedirect, O_RDONLY);
      if (fdr == -1)
      {
        perror("failed to open input file");
        _exit(1);
      }
      dup2(fdr, 0);
      close(fdr);
    }

    if (pCmdLine->outputRedirect != NULL)
    {
      if (pCmdLine->next)
      {
        fprintf(stderr, "trying to redirect the output of the left--hand-side process");
        _exit(1);
      }

      int fdr = open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fdr == -1)
      {
        perror("failed to open output file");
        _exit(1);
      }
      dup2(fdr, 1);
      close(fdr);
    }

    if (pCmdLine->next)
    {
      close(1);
      dup2(fd[1], 1);
      close(fd[1]);
      close(fd[0]);
    }

    // fprintf(stderr, "(child1>going to execute)\n");
    execvp(pCmdLine->arguments[0], pCmdLine->arguments);
    perror("execvp failed");
    _exit(1);
  }
  else
  {
    if (debug == 1)
    {
      fprintf(stderr, "PID: %d\n", pid);
      fprintf(stderr, "Executing command: %s\n", pCmdLine->arguments[0]);
    }
    close(fd[1]);
    addProcess(&process_list, pCmdLine, pid); // Add the process
    if (pCmdLine->blocking)
    {
      if (waitpid(pid, NULL, 0) == 1)
      {
        return;
      }
    }
  }

  if (pCmdLine->next)
  {
    pid_t pid2 = fork();
    if (pid2 == -1)
    {
      perror("fork");
      exit(1);
    }
    if (pid2 == 0)
    {
      if (pCmdLine->next->inputRedirect != NULL)
      {
        perror("trying to redirect the input of the right--hand-side process");
        _exit(1);
      }

      if (pCmdLine->next->outputRedirect != NULL)
      {
        int fdr = open(pCmdLine->next->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fdr == -1)
        {
          perror("failed to open output file");
          _exit(1);
        }
        dup2(fdr, 1);
        close(fdr);
      }
      close(0);
      dup2(fd[0], 0);
      close(fd[0]);

      // fprintf(stderr, "(child2>going to execute)\n");
      execvp(pCmdLine->next->arguments[0], pCmdLine->next->arguments);
      perror("execvp failed!");
      exit(1);
    }
    else
    {
      close(fd[0]);
      addProcess(&process_list, pCmdLine->next, pid2); // Add the process
      if (pCmdLine->blocking)
      {
        if (waitpid(pid2, NULL, 0) == 1)
        {
          return;
        }
      }
    }
  }
}

int main(int argc, char **argv)
{
  cmdLine *line;
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-d") == 0)
      debug = 1;
  }
  while (1)
  {
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    printf("%s$ ", cwd);
    char readLine[2048];
    fgets(readLine, 2048, stdin);
    readLine[strcspn(readLine, "\n")] = 0;

    if (strcmp(readLine, "quit") == 0)
    {
      break;
    }
    else if (strcmp(readLine, "!!") == 0)
    {
      char *last_command = getHistoryCommand(history_count);
      line = parseCmdLines(last_command);
      addHistory(last_command);
      printf("%s\n", last_command);
    }
    else if (strncmp(readLine, "!", 1) == 0)
    {
      int index;
      sscanf(readLine + 1, "%d", &index);
      char *history_command = getHistoryCommand(index);
      line = parseCmdLines(history_command);
      addHistory(history_command);
      printf("%s\n", history_command);
    }
    else
    {
      // Normal command processing
      addHistory(readLine);
      line = parseCmdLines(readLine);
    }
    execute(line);
  }
  freeProcessList(process_list);
  freeHistory(history_head);
  return 0;
}