%{
    /* definitions */
    #include <stdio.h>
    #include <stdlib.h>
    int char_count = 0;
    int line_count = 0;
    int positive = 0;
    int negative = 0;
    int positive_fraction = 0;
    int negative_fraction = 0;
%}
%%
        /* rules */
[a-zA-Z] { char_count++; }
[-][0-9]+ { negative++; }
[+]?[0-9]+ { positive++; }
[-][0-9]*\.[0-9]+ { negative_fraction++; }
[+]?[0-9]*.\.[0-9]+ { positive_fraction++; }
[\n] { line_count++; }
%%
/* code */
int main(int argc, char** argv) {
    if (argc != 2) {
        printf("usage: lex-tutorial <file>\n");
        exit(0);
    }
    yyin = fopen(argv[1], "r");

    yylex();

    printf("there were %d characters in %d lines\n",
            char_count, line_count);
    printf("there were %d positive and %d negative integers\n",
            positive, negative);
    printf("there were %d positive and %d negative fractions\n",
            positive_fraction, negative_fraction);
    
    return EXIT_SUCCESS;
}
