#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "executor.h"
#include "builtin.h"

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
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0)
    {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return 1;
}


/* Execute one command */
int execute_command(command_t *command)
{
    pid_t pid;
    int status;

    if (command == NULL || command->argc == 0)
        return 1;

    /* Built-in command */
    if (is_builtin(command->argv[0]))
        return execute_builtin(command->argv);

    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return 1;
    }

    if (pid == 0)
    {
        /* Input redirection */
        if (command->input != NULL)
        {
            int fd = open(command->input, O_RDONLY);

            if (fd < 0)
            {
                perror("open input");
                _exit(1);
            }

            if (dup2(fd, STDIN_FILENO) < 0)
            {
                perror("dup2 input");
                close(fd);
                _exit(1);
            }

            close(fd);
        }

        /* Output redirection */
        if (command->output != NULL)
        {
            int flags = O_WRONLY | O_CREAT;

            if (command->append)
                flags |= O_APPEND;
            else
                flags |= O_TRUNC;

            int fd = open(command->output, flags, 0644);

            if (fd < 0)
            {
                perror("open output");
                _exit(1);
            }

            if (dup2(fd, STDOUT_FILENO) < 0)
            {
                perror("dup2 output");
                close(fd);
                _exit(1);
            }

            close(fd);
        }

        execvp(command->argv[0], command->argv);

        perror("execvp");
        _exit(127);
    }

    if (command->background)
    {
        printf("[background pid %d]\n", pid);
        return 0;
    }

    if (waitpid(pid, &status, 0) < 0)
    {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return 1;
}


/* Execute a pipeline */
int execute_pipeline(pipeline_t *pipeline)
{
    if (pipeline == NULL || pipeline->command_count == 0)
        return 1;

    /* Single command */
    if (pipeline->command_count == 1)
        return execute_command(&pipeline->commands[0]);

    int previous_read = -1;

    pid_t pids[MAX_COMMANDS];

    int last_status = 0;

    for (int i = 0; i < pipeline->command_count; i++)
    {
        int pipefd[2] = {-1, -1};

        /* Create pipe for every command except the last */
        if (i < pipeline->command_count - 1)
        {
            if (pipe(pipefd) < 0)
            {
                perror("pipe");
                return 1;
            }
        }

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");

            if (pipefd[0] != -1)
                close(pipefd[0]);

            if (pipefd[1] != -1)
                close(pipefd[1]);

            return 1;
        }

        if (pid == 0)
        {
            command_t *command = &pipeline->commands[i];

            /*
             * If this is not the first command,
             * read from the previous pipe.
             */
            if (previous_read != -1)
            {
                if (dup2(previous_read, STDIN_FILENO) < 0)
                {
                    perror("dup2 stdin");
                    _exit(1);
                }
            }

            /*
             * If this is not the last command,
             * write into the current pipe.
             */
            if (i < pipeline->command_count - 1)
            {
                if (dup2(pipefd[1], STDOUT_FILENO) < 0)
                {
                    perror("dup2 stdout");
                    _exit(1);
                }
            }

            /* Close unused descriptors */
            if (previous_read != -1)
                close(previous_read);

            if (pipefd[0] != -1)
                close(pipefd[0]);

            if (pipefd[1] != -1)
                close(pipefd[1]);

            /*
             * Explicit input redirection.
             * For the first command this replaces stdin.
             */
            if (command->input != NULL)
            {
                int fd = open(command->input, O_RDONLY);

                if (fd < 0)
                {
                    perror("open input");
                    _exit(1);
                }

                if (dup2(fd, STDIN_FILENO) < 0)
                {
                    perror("dup2 input");
                    close(fd);
                    _exit(1);
                }

                close(fd);
            }

            /*
             * Explicit output redirection.
             * For the last command this replaces stdout.
             */
            if (command->output != NULL)
            {
                int flags = O_WRONLY | O_CREAT;

                if (command->append)
                    flags |= O_APPEND;
                else
                    flags |= O_TRUNC;

                int fd = open(command->output, flags, 0644);

                if (fd < 0)
                {
                    perror("open output");
                    _exit(1);
                }

                if (dup2(fd, STDOUT_FILENO) < 0)
                {
                    perror("dup2 output");
                    close(fd);
                    _exit(1);
                }

                close(fd);
            }

            execvp(command->argv[0], command->argv);

            perror("execvp");
            _exit(127);
        }

        /* Parent */
        pids[i] = pid;

        /*
         * Parent no longer needs the previous
         * pipe read end.
         */
        if (previous_read != -1)
            close(previous_read);

        /*
         * Parent closes the write end.
         * It keeps the read end for the next command.
         */
        if (i < pipeline->command_count - 1)
        {
            close(pipefd[1]);
            previous_read = pipefd[0];
        }
        else
        {
            previous_read = -1;
        }
    }

    /* Wait for all children */
    for (int i = 0; i < pipeline->command_count; i++)
    {
        int status;

        if (waitpid(pids[i], &status, 0) < 0)
        {
            perror("waitpid");
            last_status = 1;
        }
        else if (i == pipeline->command_count - 1)
        {
            if (WIFEXITED(status))
                last_status = WEXITSTATUS(status);
            else
                last_status = 1;
        }
    }

    return last_status;
}
