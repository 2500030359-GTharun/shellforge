#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "builtin.h"

#define MAX_INPUT 1024
#define MAX_ARGS 64

int main(void)
{
    char input[MAX_INPUT];

    while (1)
    {
        printf("shellforge$ ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("\n");
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0)
            continue;

        char *argv[MAX_ARGS];
        int argc = 0;

        char *token = strtok(input, " ");

        while (token != NULL && argc < MAX_ARGS - 1)
        {
            argv[argc] = token;
            argc++;

            token = strtok(NULL, " ");
        }

        argv[argc] = NULL;

        /* Check for built-in command */
        if (is_builtin(argv[0]))
        {
            int result = execute_builtin(argv);

            if (result == -1)
                break;

            continue;
        }

        /* Execute external command */
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            continue;
        }

        if (pid == 0)
        {
            execvp(argv[0], argv);

            perror("exec");
            exit(EXIT_FAILURE);
        }

        waitpid(pid, NULL, 0);
    }

    return 0;
}
