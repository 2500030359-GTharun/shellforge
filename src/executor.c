#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "executor.h"

int execute_external(char **argv)
{
    pid_t pid;
    int status;

    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return 1;
    }

    if (pid == 0)
    {
        execvp(argv[0], argv);

        perror("execvp");
        exit(1);
    }

    if (pid > 0)
    {
        waitpid(pid, &status, 0);
    }

    return 0;
}
