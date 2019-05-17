
/*
 *
 * CS-252 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */
%x src

%{

#include <cstring>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "y.tab.hh"
#include "shell.hh"
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}

extern "C" char * read_line();

int mygetc(FILE * f) {
  static char *p;
  char ch;

  if (!isatty(0)) {
    return getc(f);
  }

  if (p==NULL || *p == 0) {
    char * s = read_line();
    p =s;
  }
  ch = *p;
  p++;

  return ch;
}

#undef getc
#define getc(f) mygetc(f)

%}

%option noyywrap

%%

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

">" {
  return GREAT;
}

"<" {
  return LESS;
}

"|" {
  return PIPE;
}

"&" {
  return AMPERSAND;
}

">>" {
  return GREATGREAT;
}

">&" {
  return GREATAND;
}

"2>" {
  return TWOGREAT;
}

">>&" {
  return GREATGREATAND;
}

source {
  BEGIN(src);
}

<src>[^ \t\n][^ \t\n]* {
  FILE * in = fopen(yytext, "r+");

  if (!in) {
    perror("fopen");
    BEGIN(INITIAL);
  } else {
    fputc('\n', in);
    yypush_buffer_state(yy_create_buffer(in, YY_BUF_SIZE));
    BEGIN(INITIAL);
    Shell::_srcCmd = true;
    yyparse();
    yypop_buffer_state();
    fclose(in);
    Shell::_srcCmd = false;
  }
}

~[^ \n\t]* {
  std::string str = std::string(yytext);
  std::string home = getenv("HOME");
  if (str.size() == 1) {
    yylval.cpp_string = new std::string(home);
  } 
  else {
    if (str[1] == '/') {
      str = home + str.substr(1, -1);
    } 
    else {
      str = std::string("/homes/") + str.substr(1,-1);
    }
    yylval.cpp_string = new std::string(str);
  }
  return WORD;
}

\"[^\n\"]*\" {
  std::string str = std::string(yytext);
  str = str.substr(1,str.size()-2);
  yylval.cpp_string = new std::string(str);
  return WORD;
}

[^ \\\t\n]*(\\[^nt])((\\[^nt])|[^ \\\t\n])* {
  std::string str = std::string(yytext);
  std::size_t found = str.find('\\');
  while(found!=-1) {
    str = str.substr(0,found) + str.substr(found+1,std::string::npos);
    found = str.find('\\', found+1);
  }
  yylval.cpp_string = new std::string(str);
  return WORD;
}

$\{[^ \}\n]*\}([^ \n]*|$\{[^ \}\n]*\})* {
  std::string str = std::string(yytext);
  std::string result = "";
  std::size_t found = str.find('$');
  while(found!=-1) {
    std::string component;
    result += str.substr(0, found);
    if (str[found+1] == '{') {
      std::size_t end = str.find('}');
      if (end != -1) {
        component = str.substr(found+2, end-found-2);
        if (getenv(component.c_str())) result += getenv(component.c_str());
        else result += component;      
        str = str.substr(end+1,-1);
      }
    }
    found = str.find('$');
  }
  result += str;
  /*result = result + str.substr(2,str.size()-3);
  if (getenv(str.c_str()))
    yylval.cpp_string = new std::string(getenv(str.c_str()));
  else
    yylval.cpp_string = new std::string(str);*/
  yylval.cpp_string = new std::string(result);
  return WORD;
}

\`[^\n\`]*\`|$\([^\n]*\) {
  std::string str = std::string(yytext);
  if (str.at(0) == '$') {
    str = str.substr(1,str.size()-1);
  }
  str = str.substr(1,str.size()-2);
  str += "\nexit\n";
  //printf("str : %s",str.c_str());

  int pin[2], pout[2], tmpin, tmpout;
  tmpin = dup(0); tmpout = dup(1);
  pipe(pin); pipe(pout);

  dup2(pin[0], 0);
  dup2(pout[1], 1);
  close(pin[0]);
  close(pout[1]);

  write(pin[1], str.c_str(), str.size());
  close(pin[1]);

  int ret = fork();
  if (ret==0) {
    char ** args = new char*[2];
    args[0] = (char*)"/proc/self/exe";
    args[1] = NULL;
    execvp(args[0], args);
    perror("execvp(subshell)");
    exit(1);
  } else if (ret < 0) {
    perror("fork");
    exit(1);
  } else {

    waitpid(ret, NULL, 0);

    dup2(tmpin, 0);
    dup2(tmpout, 1);
    close(tmpin);
    close(tmpout);
  
    int BUFSIZE = 1024;
    char* c = new char[BUFSIZE];
    int rd = read(pout[0], c, BUFSIZE);
    close(pout[0]);

    //printf("output is %d, %d, %s", BUFSIZE, rd, c);
    for (int i = rd - 12; i >= 0; --i ) {
      if (c[i] == '\n') {
        c[i] = '\t';
      }
      myunputc( c[i] );
    }
    delete c;
  }
}

[^ \|>\t\n][^ \|>\t\n]*  {
  /* Assume that file names have only alpha chars */
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}
