#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"

int is_builtin(char *cmd)
{
    if (cmd == NULL)
        return 0;

    if (strcmp(cmd, "cd") == 0)
        return 1;

    if (strcmp(cmd, "pwd") == 0)
        return 1;

    if (strcmp(cmd, "echo") == 0)
        return 1;

    if (strcmp(cmd, "exit") == 0)
        return 1;

    return 0;
}

int execute_builtin(char **argv)
{
    /* cd */
    if (strcmp(argv[0], "cd") == 0)
    {
        char *dir;

        if (argv[1] == NULL)
        {
            dir = getenv("HOME");

            if (dir == NULL)
            {
                printf("cd: HOME not set\n");
                return 1;
            }
        }
        else
        {
            if (argv[2] != NULL)
            {
                printf("cd: too many arguments\n");
                return 1;
            }

            dir = argv[1];
        }

        if (chdir(dir) != 0)
        {
            perror("cd");
            return 1;
        }

        return 0;
    }

    /* pwd */
    if (strcmp(argv[0], "pwd") == 0)
    {
        char buffer[4096];

        if (argv[1] != NULL)
        {
            printf("pwd: too many arguments\n");
            return 1;
        }

        if (getcwd(buffer, sizeof(buffer)) == NULL)
        {
            perror("pwd");
            return 1;
        }

        printf("2500030359: %s\n", buffer);

        return 0;
    }

    /* echo */
    if (strcmp(argv[0], "echo") == 0)
    {
        int i = 1;

        while (argv[i] != NULL)
        {
            printf("%s", argv[i]);

            if (argv[i + 1] != NULL)
                printf(" ");

            i++;
        }

        printf("\n");

        return 0;
    }

    /* exit */
    if (strcmp(argv[0], "exit") == 0)
    {
        if (argv[1] != NULL)
	        {
            printf("exit: too many arguments\n");
            return 1;
        }

        return -1;
    }

    return 0;
}
