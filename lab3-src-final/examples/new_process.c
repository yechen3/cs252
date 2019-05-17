/**
 * new_process as a wrapper around fork() and execve() to run an executable binary
 **/

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        return 0;
    }

    const char* filename = argv[1];
    pid_t pid = fork();

    if (pid == -1) { // fork error
        return -1;
    }
    if (pid == 0) { // child process
        execle(filename, NULL, NULL); // execute the binary
        exit(1);
    }
    else { // parent process
        int stat_val;
        if(pid == (waitpid(pid, &stat_val, 0))) {
            if(WIFEXITED(stat_val)) {
                return WEXITSTATUS(stat_val); // return exit status of the executed binary
            }
        }
        else { // waitpid error
            return -1;
        }
    }
}
