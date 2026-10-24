%option yylineno

%{
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "y.tab.h"

int yycolumn = 1;

#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = yycolumn; \
    yylloc.last_column = yycolumn + yyleng - 1; \
    yycolumn += yyleng;
%}

ws      [ \t]+
ident   [a-zA-Z_][a-zA-Z0-9_]*
number  [0-9]+
float   [0-9]+\.[0-9]+
punc    [\(\)\{\}\[\];,]
symbol  [\+\-\*\/=!%]
relop   (==|!=|<=|>=|<|>)

%%

\n       { yycolumn = 1; }
{ws}     { }

{number} { yylval.str = strdup(yytext); return NUM; }
{float}  { yylval.str = strdup(yytext); return FLT; }
{punc}   { return yytext[0]; }
{symbol} { return yytext[0]; }
"while"  { return WHILE; }
"for"    { return FOR; }
"if"     { return IF; }
"else"   { return ELSE; }
"return" { return RETURN; }
"break"  { return BREAK; }
"switch" { return SWITCH; }
"case"   { return CASE; }
"continue" { return CONTINUE; }
"int"    { return INT; }
"float"  { return FLOAT; }
"true"   { return TRUE; }
"false"  { return FALSE; }
{relop}  { yylval.str = strdup(yytext); return RELOP; }
"&&"     { return AND; }
"||"     { return OR; }
{ident}  { yylval.str = strdup(yytext); return ID; }
.        { printf("Unknown character: %s\n", yytext); }

%%

int yywrap() { return 1; }