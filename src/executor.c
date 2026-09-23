#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include "executor.h"
#include "builtin.h"


/*
 * SIGCHLD handler
 *
 * Reap finished background child processes
 * so that zombie processes are not created.
 */
static void sigchld_handler(int sig)
{
    int saved_errno = errno;

    (void)sig;

    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
        /* Reap completed child */
    }

    errno = saved_errno;
}


/*
 * Install SIGCHLD handler
 */
void setup_background_handler(void)
{
    if (signal(SIGCHLD, sigchld_handler) == SIG_ERR)
    {
        perror("signal");
    }
}


/*
 * Execute an external command
 */
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

    /*
     * Child
     */
    if (pid == 0)
    {
        execvp(argv[0], argv);

        perror("execvp");
        _exit(127);
    }

    /*
     * Parent
     */
    if (waitpid(pid, &status, 0) < 0)
    {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return 1;
}


/*
 * Execute one command
 */
int execute_command(command_t *command)
{
    pid_t pid;
    int status;

    if (command == NULL || command->argc == 0)
        return 1;

    /*
     * Built-in command
     */
    if (is_builtin(command->argv[0]))
        return execute_builtin(command->argv);

    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return 1;
    }

    /*
     * Child process
     */
    if (pid == 0)
    {
        /*
         * Background command:
         * stdin comes from /dev/null.
         */
        if (command->background)
        {
            int null_fd = open("/dev/null", O_RDONLY);

            if (null_fd < 0)
            {
                perror("open /dev/null");
                _exit(1);
            }

            if (dup2(null_fd, STDIN_FILENO) < 0)
            {
                perror("dup2 /dev/null");
                close(null_fd);
                _exit(1);
            }

            close(null_fd);
        }

        /*
         * Input redirection
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
         * Output redirection
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

        /*
         * Execute command
         */
        execvp(command->argv[0], command->argv);

        perror("execvp");
        _exit(127);
    }

    /*
     * Parent process
     */

    /*
     * Background command:
     * print PID and do not wait.
     */
    if (command->background)
    {
        printf("[Background PID: %d]\n", pid);
        fflush(stdout);

        return 0;
    }

    /*
     * Foreground command:
     * wait for child.
     */
    if (waitpid(pid, &status, 0) < 0)
    {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return 1;
}


/*
 * Execute a pipeline
 */
int execute_pipeline(pipeline_t *pipeline)
{
    if (pipeline == NULL || pipeline->command_count == 0)
        return 1;

    /*
     * If there is only one command,
     * execute it normally.
     */
    if (pipeline->command_count == 1)
        return execute_command(&pipeline->commands[0]);

    /*
     * Background flag is stored on the last command.
     */
    int background =
        pipeline->commands[pipeline->command_count - 1].background;

    int previous_read = -1;

    pid_t pids[MAX_COMMANDS];

    int last_status = 0;

    /*
     * Create each process in the pipeline.
     */
    for (int i = 0;
         i < pipeline->command_count;
         i++)
    {
        int pipefd[2] = {-1, -1};

        /*
         * Create pipe unless this is the last command.
         */
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

        /*
         * Child process
         */
        if (pid == 0)
        {
            command_t *command =
                &pipeline->commands[i];

            /*
             * For a background pipeline,
             * first command gets stdin from /dev/null.
             *
             * Explicit < redirection below can override it.
             */
            if (background &&
                i == 0 &&
                command->input == NULL)
            {
                int null_fd = open("/dev/null", O_RDONLY);

                if (null_fd < 0)
                {
                    perror("open /dev/null");
                    _exit(1);
                }

                if (dup2(null_fd, STDIN_FILENO) < 0)
                {
                    perror("dup2 /dev/null");
                    close(null_fd);
                    _exit(1);
                }

                close(null_fd);
            }

            /*
             * Read from previous pipe.
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
             * Write to current pipe.
             */
            if (i < pipeline->command_count - 1)
            {
                if (dup2(pipefd[1], STDOUT_FILENO) < 0)
                {
                    perror("dup2 stdout");
                    _exit(1);
                }
            }

            /*
             * Close unused descriptors.
             */
            if (previous_read != -1)
                close(previous_read);

            if (pipefd[0] != -1)
                close(pipefd[0]);

            if (pipefd[1] != -1)
                close(pipefd[1]);

            /*
             * Explicit input redirection
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
             * Explicit output redirection
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

            /*
             * Execute pipeline command
             */
            execvp(command->argv[0], command->argv);

            perror("execvp");
            _exit(127);
        }

        /*
         * Parent process
         */
        pids[i] = pid;

        /*
         * Close previous pipe read end.
         */
        if (previous_read != -1)
            close(previous_read);

        /*
         * Keep current pipe read end
         * for the next command.
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

    /*
     * Background pipeline:
     * print PID and return immediately.
     */
    if (background)
    {
        printf("[Background Pipeline PID: %d]\n", pids[0]);
        fflush(stdout);

        return 0;
    }

    /*
     * Foreground pipeline:
     * wait for all commands.
     */
    for (int i = 0;
         i < pipeline->command_count;
         i++)
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
