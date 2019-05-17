
/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 */

%code requires 
{
#include <sys/types.h>
#include <string>
#include <string.h>
#include <regex.h>
#include <dirent.h>
#include <algorithm>

#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
#define MAXFILENAME 1024
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN GREAT NEWLINE LESS PIPE AMPERSAND GREATGREAT GREATAND TWOGREAT GREATGREATAND

%{
//#define yylex yylex
#include <cstdio>
#include "shell.hh"
#include <string>

void yyerror(const char * s);
void expandWildcardsIfNecessary(std::string * arg);
void expandWildcard(char * prefix, char * suffix);
bool cmpfunction (char * i, char * j);

int yylex();
static std::vector<char *> _sortArgument = std::vector<char *>();
static bool wildCard;
%}

%%

goal:
  commands
  ;

commands:
  command
  | commands command
  ;

command: 
  pipe_list iomodifier_list background_opt NEWLINE {
    //printf("   Yacc: Execute command\n");
    Shell::_currentCommand.execute();
  }
  | NEWLINE {
    //printf("   Yacc: Execute command\n");
    Shell::_currentCommand.execute();
  }
  | error NEWLINE { yyerrok; }
  ;

simple_command:	
  command_and_args 
  ;

command_and_args:
  command_word argument_list {    
    Shell::_currentCommand.
    insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;

argument_list:
  argument_list argument
  | /* can be empty */
  ;

argument:
  WORD {
    //printf("   Yacc: insert argument \"%s\"\n", $1->c_str());
    //Command::_currentSimpleCommand->insertArgument( $1 );
    wildCard = false;
    char *p = (char *)"";
    expandWildcard(p, (char *)$1->c_str());
    std::sort(_sortArgument.begin(), _sortArgument.end(), cmpfunction);
    for (auto a: _sortArgument) {
      std::string * argToInsert = new std::string(a);
      Command::_currentSimpleCommand->insertArgument(argToInsert);
    }
    _sortArgument.clear();
    //if (!wildCard) Command::_currentSimpleCommand->insertArgument($1);
  }
  ;

command_word:
  WORD {
    //printf("   Yacc: insert command \"%s\"\n", $1->c_str());
    
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;

pipe_list:
  pipe_list PIPE simple_command 
  | simple_command 
  ;

iomodifier_opt:
  GREAT WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    Shell::_currentCommand.redirect(1,$2);
  }
  | GREATGREAT WORD {
    //printf("   Yacc: append output \"%s\"\n", $2->c_str());
    Shell::_currentCommand.redirect(1,$2);
    Shell::_currentCommand._append = true;
  }
  | GREATAND WORD {
    //printf("   Yacc: insert output and error \"%s\"\n", $2->c_str());
    Shell::_currentCommand.redirect(1,$2);
    Shell::_currentCommand.redirect(2, new std::string($2->c_str()));
    
  }
  | GREATGREATAND WORD {
    //printf("   Yacc: append output and error \"%s\"\n", $2->c_str());
    Shell::_currentCommand.redirect(1,$2);
    Shell::_currentCommand.redirect(2, new std::string($2->c_str()));
    Shell::_currentCommand._append = true;
  }
  | LESS WORD {
    //printf("   Yacc: get input \"%s\"\n", $2->c_str());
    Shell::_currentCommand.redirect(0, $2);
  }
  | TWOGREAT WORD {
    //printf("   Yacc: insert error \"%s\"\n", $2->c_str());
    Shell::_currentCommand.redirect(2, $2);
  }
  ;

iomodifier_list:
  iomodifier_list iomodifier_opt
  | /*empty*/
  ;

background_opt:
  AMPERSAND {
    //printf("   Yacc: run in background\n");
    Shell::_currentCommand._background = true;
  }
  | /*empty*/
  ;
%%

bool cmpfunction (char * i, char * j) { return strcmp(i,j)<0; }

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

void expandWildcardsIfNecessary(std::string * arg) {
  char * arg_c = (char *)arg->c_str();
  char * a;
  std::string path;
  if (strchr(arg_c,'?')==NULL & strchr(arg_c,'*')==NULL) {
    //printf("No '?' or '*' was found\n");
    Command::_currentSimpleCommand->insertArgument(arg);
    return;
  }
  DIR * dir;
  if (arg_c[0] == '/') {
    std::size_t found = arg->find('/');
    while (arg->find('/',found+1) != -1) 
      found = arg->find('/', found+1);
      
    path = arg->substr(0, found+1);
    a = (char *)arg->substr(found+1, -1).c_str();
    dir = opendir(path.c_str());
    //printf("%s\n", path.c_str());
  }
  else {
    dir = opendir(".");
    a = arg_c;
  }
  if (dir == NULL) {
    perror("opendir");
    return;
  }
  char * reg = (char*)malloc(2*strlen(arg_c)+10);
  char * r = reg;
  *r = '^'; r++;
  while (*a) {
    if (*a == '*') {*r='.'; r++; *r='*'; r++;}
    else if (*a == '?') {*r='.'; r++;}
    else if (*a == '.') {*r='\\'; r++; *r='.'; r++;}
    else {*r=*a; r++;}
    a++;
  }
  *r='$'; r++; *r=0;

  regex_t re;
  int expbuf = regcomp(&re, reg, REG_EXTENDED|REG_NOSUB);
  if (expbuf != 0) {
    perror("regcomp");
    return;
  }

  std::vector<char *> sortArgument = std::vector<char *>();
  struct dirent * ent;
  while ( (ent=readdir(dir)) != NULL) {
    if (regexec(&re, ent->d_name, 1, NULL, 0) == 0) {
      if (reg[1] == '.') {
        if (ent->d_name[0] != '.') {
          std::string name(ent->d_name);
          name = path + name;
          sortArgument.push_back(strdup((char *)name.c_str()));
        }
      } else {
        std::string name(ent->d_name);
        name = path + name;
        sortArgument.push_back(strdup((char *)name.c_str()));
      }
    }
  }

  closedir(dir);
  regfree(&re);

  std::sort(sortArgument.begin(), sortArgument.end(), cmpfunction);
  
  for (auto a: sortArgument) {
    std::string * argToInsert = new std::string(a);
    Command::_currentSimpleCommand->insertArgument(argToInsert);
  }

  sortArgument.clear();
}

void expandWildcard(char * prefix, char * suffix) {
  if (suffix[0] == 0) {
    _sortArgument.push_back(strdup(prefix));
    return;
  }
  char Prefix[MAXFILENAME];
  if (prefix[0] == 0) {
    if (suffix[0] == '/') {suffix += 1; sprintf(Prefix, "%s/", prefix);}
    else strcpy(Prefix, prefix);
  }
  else
    sprintf(Prefix, "%s/", prefix);

  char * s = strchr(suffix, '/');
  char component[MAXFILENAME];
  if (s != NULL) {
    strncpy(component, suffix, s-suffix);
    component[s-suffix] = 0;
    suffix = s + 1;
  }
  else {
    strcpy(component, suffix);
    suffix = suffix + strlen(suffix);
  }

  char newPrefix[MAXFILENAME];
  if (strchr(component,'?')==NULL & strchr(component,'*')==NULL) {
    if (Prefix[0] == 0) strcpy(newPrefix, component);
    else sprintf(newPrefix, "%s/%s", prefix, component);
    expandWildcard(newPrefix, suffix);
    return;
  }
  
  char * reg = (char*)malloc(2*strlen(component)+10);
  char * r = reg;
  *r = '^'; r++;
  int i = 0;
  while (component[i]) {
    if (component[i] == '*') {*r='.'; r++; *r='*'; r++;}
    else if (component[i] == '?') {*r='.'; r++;}
    else if (component[i] == '.') {*r='\\'; r++; *r='.'; r++;}
    else {*r=component[i]; r++;}
    i++;
  }
  *r='$'; r++; *r=0;

  regex_t re;
  int expbuf = regcomp(&re, reg, REG_EXTENDED|REG_NOSUB);
  
  char * dir;
  if (Prefix[0] == 0) dir = (char*)"."; else dir = Prefix;
  DIR * d = opendir(dir);
  if (d == NULL) {
    return;
  }
  struct dirent * ent;
  bool find = false;
  while ((ent = readdir(d)) != NULL) {
    if(regexec(&re, ent->d_name, 1, NULL, 0) == 0) {
      find = true;
      if (Prefix[0] == 0) strcpy(newPrefix, ent->d_name);
      else sprintf(newPrefix, "%s/%s", prefix, ent->d_name);

      if (reg[1] == '.') {
        if (ent->d_name[0] != '.') expandWildcard(newPrefix, suffix);
      } else 
        expandWildcard(newPrefix, suffix);
    }
  }
  if (!find) {
    if (Prefix[0] == 0) strcpy(newPrefix, component);
    else sprintf(newPrefix, "%s/%s", prefix, component);
    expandWildcard(newPrefix, suffix);
  }
  closedir(d);
  regfree(&re);
  free(reg);
}

#if 0
main()
{
  yyparse();
}
#endif
