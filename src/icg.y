%{

#include <cstdio>
#include <cstdlib>
#include <vector>
#include "tree.hpp"
#include "type.hpp"
#include "lex.yy.c"

void yyerror(const char *s);

extern int yylex();
extern FILE *yyin;

Program *prog;


%}

%union {
    char *str;
    StmtNode *stmt;
    StmtListNode *stmtList;
    VarList *varList;
    ExprNode *expr;
    PartialArrReference *partialArr;
    BoolNode *boolExpr;
    TypeNode *typeNode;
    std::vector<int> *intList;
    int num;
}

%define parse.error verbose

%token <str> WHILE FOR IF ELSE SWITCH CASE RETURN BREAK CONTINUE INT FLOAT VOID ID NUM FLT RELOP AND OR TRUE FALSE

%type <stmtList> STMT_LIST
%type <stmt> STMT DECL ASGN OPT_ASGN
%type <varList> VARLIST
%type <expr> EXPR
%type <partialArr> ARR_REF
%type <boolExpr> BOOL OPT_BOOL
%type <typeNode> TYPE
%type <intList> LST
%type <num> BASE

%left '='

%left OR
%left AND

%left RELOP

%left '+' '-'
%left '*' '/' '%'

%nonassoc NOT
%nonassoc NEG

%nonassoc REDUCTION_PRECEDENCE
%nonassoc ELSE

%%

PROG        : STMT_LIST                                             { prog = new Program($1); }
            ;

STMT_LIST   : STMT_LIST STMT                                        { $1->insert($2); $$ = $1; }
            | STMT                                                  { $$ = new StmtListNode($1); }
            ;

STMT        : WHILE '(' BOOL ')' STMT                               { $$ = new WhileStmtNode($3, $5); }
            | IF '(' BOOL ')' STMT ELSE STMT                        { $$ = new IfStmtNode($3, $5, $7); }
            | IF '(' BOOL ')' STMT %prec REDUCTION_PRECEDENCE       { $$ = new IfStmtNode($3, $5, nullptr); }
            | FOR '(' OPT_ASGN ';' OPT_BOOL ';' OPT_ASGN ')' STMT   { $$ = new ForStmtNode($3, $5, $7, $9); }
            | BREAK ';'                                             { $$ = new BreakStmtNode(); }
            | CONTINUE ';'                                          { $$ = new ContinueStmtNode(); }
            | '{' STMT_LIST '}'                                     { $$ = $2; }
            | DECL ';'                                              { $$ = $1; }
            | ASGN ';'                                              { $$ = $1; }
            ;

ASGN        : ID '=' EXPR                                           { $$ = new AsgnStmtNode($1, $3); }
            | ARR_REF '=' EXPR                                      { $$ = new ArrAsgnStmtNode($1, $3); }
            ;

OPT_ASGN    : ASGN                                                  { $$ = $1; }
            |                                                       { $$ = nullptr; }
            ;

OPT_BOOL    : BOOL                                                  { $$ = $1; }
            |                                                       { $$ = nullptr; }
            ;

DECL        : TYPE VARLIST                                          { $$ = new DeclStmtNode($1, $2); }
            ;

VARLIST     : VARLIST ',' ID                                        { $1->insert($3); $$ = $1; }
            | ID                                                    { $$ = new VarList($1); }
            ;

EXPR        : EXPR '+' EXPR                                         { $$ = new BinOpExprNode(PLUS, $1, $3); }
            | EXPR '-' EXPR                                         { $$ = new BinOpExprNode(MINUS, $1, $3); }
            | EXPR '*' EXPR                                         { $$ = new BinOpExprNode(MUL, $1, $3); }
            | EXPR '/' EXPR                                         { $$ = new BinOpExprNode(DIV, $1, $3); }
            | EXPR '%' EXPR                                         { $$ = new BinOpExprNode(MOD, $1, $3); }
            | '(' EXPR ')'                                          { $$ = $2; }
            | '-' EXPR %prec NEG                                    { $$ = new UnaryOpExprNode($2); }
            | ARR_REF                                               { $$ = new ArrayReferenceExprNode($1); }
            | ID                                                    { $$ = new IdExprNode($1); }
            | NUM                                                   { $$ = new ConstExprNode(INT_CONST, $1); }
            | FLT                                                   { $$ = new ConstExprNode(FLOAT_CONST, $1); }
            ;

ARR_REF     : ID '[' EXPR ']'                                       { $$ = new PartialArrReference($1); $$->insert($3); }
            | ARR_REF '[' EXPR ']'                                  { $1->insert($3); $$ = $1; }
            ;

BOOL        : BOOL OR BOOL                                          { $$ = new BinOpBoolNode($1, $3, OR_OP); }
            | BOOL AND BOOL                                         { $$ = new BinOpBoolNode($1, $3, AND_OP); }
            | '!' BOOL %prec NOT                                    { $$ = new BinOpBoolNode($2, nullptr, NOT_OP); }
            | EXPR RELOP EXPR                                       { $$ = new RelOpBoolNode($1, $3, $2); }
            | TRUE                                                  { $$ = new ConstBoolNode(true); }
            | FALSE                                                 { $$ = new ConstBoolNode(false); }
            ;

TYPE        : BASE LST                                              { $$ = new TypeNode($1, $2); }
            ;

BASE        : INT                                                   { $$ = INT_TYPE; }
            | FLOAT                                                 { $$ = FLOAT_TYPE; }
            ;

LST         : LST '[' NUM ']'                                       { $1->push_back(atoi($3)); $$ = $1; }
            |                                                       { $$ = new std::vector<int>(); }
            ;

%%

#ifdef ICG_LIB

static std::string lastError;   // message set by yyerror()

void yyerror(const char *s) {
    lastError = s;
}

static std::string jsonEscape(const std::string &s) {
    std::string out;
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    return out;
}

static std::string errorJson(const std::string &message, int line) {
    std::string out = "{\"ok\":false,\"error\":{\"message\":\"" + jsonEscape(message) + "\",\"line\":";
    out += (line > 0) ? std::to_string(line) : std::string("null");
    out += "}}";
    return out;
}

// Compiles `source` and returns JSON:
//   {"ok":true,"output":"<three-address code>"}
//   {"ok":false,"error":{"message":"...","line":N|null}}
// The returned pointer stays valid until the next call.
extern "C" const char *icg_compile(const char *source) {
    static std::string result;

    resetState();
    prog = nullptr;
    lastError.clear();
    yylineno = 1;

    YY_BUFFER_STATE buffer = yy_scan_string(source);
    int status = yyparse();
    int errorLine = yylineno;
    yy_delete_buffer(buffer);

    if (status != 0 || prog == nullptr) {
        result = errorJson(lastError.empty() ? "syntax error" : lastError, errorLine);
        return result.c_str();
    }

    try {
        result = "{\"ok\":true,\"output\":\"" + jsonEscape(prog->getCode()) + "\"}";
    } catch (const std::exception &e) {
        result = errorJson(e.what(), 0);
    }
    return result.c_str();
}

#else

void yyerror(const char *s) {
    fprintf(stderr, "Error: %s\n", s);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        FILE *file = fopen(argv[1], "r");
        yyin = file;
    }
    
    yyparse();

    std::string code;
    try {
        code = prog->getCode();
    } catch (const std::exception &e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
    std::cout << code << std::endl;

    return 0;
}

#endif // ICG_LIB
