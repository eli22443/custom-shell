#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{
    int fd[2];
    if (pipe(fd) == -1)
    {
        perror("pipe failed");
        return 1;
    }

    fprintf(stderr, "(parent_process>forking...)\n");
    pid_t pid1 = fork();
    if (pid1 == -1)
    {
        perror("fork");
        exit(1);
    }
    if (pid1 == 0)
    {
        close(1);
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe...)\n");
        dup2(fd[1], 1);
        close(fd[1]);
        close(fd[0]);

        fprintf(stderr, "(child1>going to execute cmd: ls -ls)\n");
        char *const ls[] = {"ls", "-ls", (char *)NULL};
        execvp("ls", ls);
        perror("execvp ls failed!");
        exit(1);
    }
    else
    {
        fprintf(stderr, "(parent_process>created process with id: %d)\n", pid1);

        fprintf(stderr, "(parent_process>closing the write end of the pipe...)\n");
        close(fd[1]);

        fprintf(stderr, "(parent_process>waiting for child processes to terminate...)\n");
        waitpid(pid1, NULL, 0);
    }

    fprintf(stderr, "(parent_process>forking...)\n");
    pid_t pid2 = fork();
    if (pid2 == -1)
    {
        perror("fork");
        exit(1);
    }
    if (pid2 == 0)
    {
        close(0);
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe...)\n");
        dup2(fd[0], 0);
        close(fd[0]);

        fprintf(stderr, "(child2>going to execute cmd: wc)\n");
        char *const wc[] = {"wc", (char *)NULL};
        execvp("wc", wc);
        perror("execvp wc failed!");
        exit(1);
    }
    else
    {
        fprintf(stderr, "(parent_process>created process with id: %d)\n", pid2);

        fprintf(stderr, "(parent_process>closing the read end of the pipe...)\n");
        close(fd[0]);

        fprintf(stderr, "(parent_process>waiting for child processes to terminate...)\n");
        waitpid(pid2, NULL, 0);
    }
    fprintf(stderr, "(parent_process>exiting...)\n");

    return 0;
}