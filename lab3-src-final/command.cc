/*
 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 * DO NOT PUT THIS PROJECT IN A PUBLIC REPOSITORY LIKE GIT. IF YOU WANT 
 * TO MAKE IT PUBLICALLY AVAILABLE YOU NEED TO REMOVE ANY SKELETON CODE 
 * AND REWRITE YOUR PROJECT SO IT IMPLEMENTS FUNCTIONALITY DIFFERENT THAN
 * WHAT IS SPECIFIED IN THE HANDOUT. WE OFTEN REUSE PART OF THE PROJECTS FROM  
 * SEMESTER TO SEMESTER AND PUTTING YOUR CODE IN A PUBLIC REPOSITORY
 * MAY FACILITATE ACADEMIC DISHONESTY.
 */

#include <cstdio>
#include <cstdlib>

#include <iostream>

#include "command.hh"
#include "shell.hh"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h> 
#include <errno.h>
#include <string.h>
#include <string>
#include <stdlib.h>


Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _append = false;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    if ( _outFile ) {
        delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile ) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

void Command::redirect(int i, std::string * curr) {
    if (i==0) {
        if ( _inFile ) {
            printf("Ambiguous input redirect.\n");
            exit(1);
        } else {
            _inFile = curr;
        }
    }
    if (i==2) {
        if ( _errFile ) {
            printf("Ambiguous error redirect.\n");
            exit(1);
        } else {
            _errFile = curr;
        }
    }
    if (i==1) {
        if ( _outFile ) {
            printf("Ambiguous output redirect.\n");
            exit(1);
        } else {
            _outFile = curr;
        }
    }
}

void Command::execute() {
    // Don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
        Shell::prompt();
        return;
    }

    // Print contents of Command data structure
    //print();

    // exit myshell
    std::string* cmd = _simpleCommands[0]->_arguments[0];
    if ( !strcmp(cmd->c_str(),"exit") ) {
            printf( "Good bye!!\n");
        exit(1);
    }

    // set the environment variable
    if ( !strcmp(cmd->c_str(),"setenv") ) {
        if ( _simpleCommands[0]->_arguments.size() != 3 ) {
            fprintf(stderr, "setenv should take two arguments\n");
            return;
        }
        const char * A = _simpleCommands[0]->_arguments[1]->c_str();
        const char * B = _simpleCommands[0]->_arguments[2]->c_str();
        setenv(A, B, 1);
        clear();
        Shell::prompt();
        return;
    }

    // unset the environment varibale
    if ( !strcmp(cmd->c_str(),"unsetenv") ) {
        const char * A = _simpleCommands[0]->_arguments[1]->c_str();
        unsetenv(A);
        clear();
        Shell::prompt();
        return;
    }

    // change directory
    if ( !strcmp(cmd->c_str(),"cd") ) {
        if (_simpleCommands[0]->_arguments.size()==1) {
            chdir(getenv("HOME"));
        } else {
            const char * path = _simpleCommands[0]->_arguments[1]->c_str();
            chdir(path);
        }
        clear();
        Shell::prompt();
        return;
    }

    //save in/out
    int tmpin = dup(0);
    int tmpout = dup(1);
    int tmperr = dup(2);
    int numsimplecommands = _simpleCommands.size();
    
    // set the initial input
    int fdin;
    if ( _inFile ) {
        const char* infile = _inFile->c_str();
        fdin = open(infile, O_RDONLY);
        if (fdin < 0) {
            fprintf(stderr, "/bin/sh: 1: cannot open %s: No such file\n", infile);
            Shell::prompt();
            clear();
            return;
        }
    } 
    else {
        //Use default input
        fdin = dup(tmpin);
    }
    // Add execution here
    int ret;
    int fdout;
    int fderr;
    for (int i=0; i<numsimplecommands; i++) {
        //redirect input
        // Setup i/o redirection
        dup2(fdin, 0);
        close(fdin);
        //setup output
        if (i == numsimplecommands-1) {
            //Last simple command
            if ( _outFile ) {
                const char* outfile = _outFile->c_str();
                if ( _append ){
                    fdout = open(outfile, O_CREAT|O_WRONLY|O_APPEND, 0664);
                }
                else {
                    fdout = open(outfile, O_CREAT|O_WRONLY|O_TRUNC, 0664);
                }
            } 
            else {
                //Use default output
                fdout = dup(tmpout);
            }

            if ( _errFile ) {
                const char* errfile = _errFile->c_str();
                if ( _append ){
                    fderr = open(errfile, O_CREAT|O_WRONLY|O_APPEND, 0664);
                }
                else {
                    fderr = open(errfile, O_CREAT|O_WRONLY|O_TRUNC, 0664);
                }
            }
            else {
                fderr = dup(tmperr);
            }
            dup2(fderr, 2);
            close(fderr);
            int n = _simpleCommands[i]->_arguments.size();
            char * c = strdup(_simpleCommands[i]->_arguments[n-1]->c_str());
            setenv("_", c, 1);
        }
        else {
            //Not last simple command
            //create pipe
            int fdpipe[2];
            pipe(fdpipe);
            fdout = fdpipe[1];
            fdin = fdpipe[0];
        }
        //Redirect output
        dup2(fdout, 1);
        close(fdout);

        //Create child process
        // For every simple command fork a new process
        ret = fork();
        if (ret==0) {
            // printenv function call
            if (!strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "printenv")) {
                char **p = environ;
                while (*p!=NULL) {
                    printf("%s\n", *p);
                    p++;
                }
                exit(0);
            }
            // and call exec
            const char * command = _simpleCommands[i]->_arguments[0]->c_str();
            int argnum = _simpleCommands[i]->_arguments.size();
            char ** args = new char*[argnum+1];
            for (int j=0; j<argnum; j++) {
                args[j] = (char *)_simpleCommands[i]->_arguments[j]->c_str();
            }
            args[argnum] = NULL;
            execvp(command, args);
            perror("execvp");
            exit(1);
        } 
        else if (ret < 0) {
            perror("fork");
            return;
        }   
    }
    
    //restore in/out defaults
    dup2(tmpin,0);
    dup2(tmpout,1);
    dup2(tmperr,2);
    close(tmpin);
    close(tmpout);
    close(tmperr);

    if (!_background) {
        // Wait for last command
        int status;
        waitpid(ret, &status, 0);
        std::string s = std::to_string(WEXITSTATUS(status));
        setenv("?", s.c_str(), 1);
        char * pError = getenv("ON_ERROR");
        if (pError != NULL && WEXITSTATUS(status) != 0) printf("%s\n", pError);
    } else {
        std::string s = std::to_string(ret);
        setenv("!", s.c_str(), 1);
        Shell::_bgPIDs.push_back(ret);
    }

    // Clear to prepare for next command
    clear();

    // Print new prompt
    Shell::prompt();
}

SimpleCommand * Command::_currentSimpleCommand;
