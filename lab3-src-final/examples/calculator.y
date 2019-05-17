%{
    #include<ctype.h>
    #include<stdio.h>
    #include<stdlib.h>
    #define YYSTYPE double
    int yylex();
    int yyerror(char*);    
%}
%token NUM

%left '+' '-'
%left '*' '/'
%right UMINUS

%%

S : S E '\n' { printf("answer: %g \nenter another expression: ", $2); }
| S '\n'
|
| error '\n' { yyerror("parse error\n"); yyerrok; }
;
E : E '+' E { $$=$1+$3; }
| E '-' E { $$=$1-$3; }
| E '*' E { $$=$1*$3; }
| E '/' E { $$=$1/$3; }
| '('E')' { $$=$2; }
| '-'E %prec UMINUS { $$=-$2; }
| NUM
;

%%

#include "lex.yy.c"

int  main() {
    printf("enter expression: ");
    yyparse();
    return EXIT_SUCCESS;
}

int yyerror (char * s) {
    printf ("%s \n", s);
    exit (1);
}
