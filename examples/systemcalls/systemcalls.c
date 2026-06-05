#include "systemcalls.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 * successfully using the system() call, false otherwise.
*/
bool do_system(const char *cmd)
{
    int status = system(cmd);

    if (status == -1) {
        return false;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return true;
    }

    return false;
}

/**
* @param count - The number of variables passed to the function.
* @param ... - Command and arguments to execute using execv()
*/
bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);

    char *command[count + 1];

    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }

    command[count] = NULL;
    va_end(args);

    fflush(stdout);

    pid_t pid = fork();

    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        execv(command[0], command);
        _exit(EXIT_FAILURE);
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return false;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return true;
    }

    return false;
}

/**
* @param outputfile - The full path to the file to write command output.
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);

    char *command[count + 1];

    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }

    command[count] = NULL;
    va_end(args);

    fflush(stdout);

    pid_t pid = fork();

    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (fd == -1) {
            _exit(EXIT_FAILURE);
        }

        if (dup2(fd, STDOUT_FILENO) == -1) {
            close(fd);
            _exit(EXIT_FAILURE);
        }

        close(fd);

        execv(command[0], command);
        _exit(EXIT_FAILURE);
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return false;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return true;
    }

    return false;
}
