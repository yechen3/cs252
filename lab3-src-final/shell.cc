#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include "shell.hh"
#include "y.tab.hh"

#ifndef YY_BUF_SIZE
#ifdef __ia64__
/* On IA-64, the buffer size is 16k, not 8k.
 * Moreover, YY_BUF_SIZE is 2*YY_READ_BUF_SIZE in the general case.
 * Ditto for the __ia64__ case accordingly.
 */
#define YY_BUF_SIZE 32768
#else
#define YY_BUF_SIZE 16384
#endif /* __ia64__ */
#endif

#ifndef YY_TYPEDEF_YY_BUFFER_STATE
#define YY_TYPEDEF_YY_BUFFER_STATE
typedef struct yy_buffer_state *YY_BUFFER_STATE;
#endif

void yyrestart(FILE * input_file );
int yyparse(void);
void yypush_buffer_state(YY_BUFFER_STATE buffer);
void yypop_buffer_state();
YY_BUFFER_STATE yy_create_buffer(FILE * file, int size);


void Shell::prompt() {
  if ( isatty(0) & !_srcCmd ){
    char * pPrompt = getenv("PROMPT");
    if (pPrompt != NULL) printf("%s ", pPrompt);
  	else printf("myshell>");
    fflush(stdout);
  }
}

extern "C" void sigIntHandler(int sig)
{
  if (sig == SIGINT) {
  	Shell::_currentCommand.clear();
  	printf("\n");
  	Shell::prompt();
  }

  if (sig == SIGCHLD) {
  	pid_t pid = waitpid(-1, NULL, WNOHANG);
  	for (unsigned i=0; i<Shell::_bgPIDs.size(); i++) {
  		if (pid == Shell::_bgPIDs[i]) {
  			printf("[%d] exited\n", pid);
  			Shell::_bgPIDs.erase(Shell::_bgPIDs.begin()+i);
  			break;
  		}
  	}
  }
}

void source(void) {
  std::string s = ".shellrc";
  FILE * in = fopen(s.c_str(), "r");

  if (!in) {
    return;
  }

  yypush_buffer_state(yy_create_buffer(in, YY_BUF_SIZE));
  Shell::_srcCmd = true;
  yyparse();
  yypop_buffer_state();
  fclose(in);
  Shell::_srcCmd = false;
}

int main(int argc, char **argv) {
  struct sigaction sa;
  sa.sa_handler = sigIntHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  
  source();
  
  std::string s = std::to_string(getpid());
  setenv("$", s.c_str(), 1);
  
  char abs_path[256];
  realpath(argv[0], abs_path);
  setenv("SHELL", abs_path, 1);

  Shell::_srcCmd = false;
  Shell::prompt();
  if (sigaction(SIGINT, &sa, NULL)) {
    perror("sigaction");
    exit(2);
  }
  if (sigaction(SIGCHLD, &sa, NULL)) {
    perror("sigaction");
    exit(2);
  }
  yyrestart(stdin);
  yyparse();
}

Command Shell::_currentCommand;
std::vector<int> Shell::_bgPIDs;
bool Shell::_srcCmd;
