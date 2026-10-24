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
    Program *program;
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

%type <program> PROG
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

%locations

%%

PROG        : STMT_LIST                                             { prog = new Program($1); }
            ;

STMT_LIST   : STMT_LIST STMT                                        { $1->insert($2, @$); $$ = $1; }
            |                                                       { $$ = new StmtListNode(@$); }
            ;

STMT        : WHILE '(' BOOL ')' STMT                               { $$ = new WhileStmtNode($3, $5, @$, @1); }
            | IF '(' BOOL ')' STMT ELSE STMT                        { $$ = new IfStmtNode($3, $5, $7, @$, @1, @6); }
            | IF '(' BOOL ')' STMT %prec REDUCTION_PRECEDENCE       { $$ = new IfStmtNode($3, $5, nullptr, @$, @1, @1); }
            | FOR '(' OPT_ASGN ';' OPT_BOOL ';' OPT_ASGN ')' STMT   { $$ = new ForStmtNode($3, $5, $7, $9, @$, @1); }
            | BREAK ';'                                             { $$ = new BreakStmtNode(@$); }
            | CONTINUE ';'                                          { $$ = new ContinueStmtNode(@$); }
            | '{' STMT_LIST '}'                                     { $$ = $2; }
            | DECL ';'                                              { $$ = $1; }
            | ASGN ';'                                              { $$ = $1; }
            ;

ASGN        : ID '=' EXPR                                           { $$ = new AsgnStmtNode($1, $3, @$, @2); }
            | ARR_REF '=' EXPR                                      { $$ = new ArrAsgnStmtNode($1, $3, @$, @2); }
            ;

OPT_ASGN    : ASGN                                                  { $$ = $1; }
            |                                                       { $$ = nullptr; }
            ;

OPT_BOOL    : BOOL                                                  { $$ = $1; }
            |                                                       { $$ = nullptr; }
            ;

DECL        : TYPE VARLIST                                          { $$ = new DeclStmtNode($1, $2, @$); }
            ;

VARLIST     : VARLIST ',' ID                                        { $1->insert($3); $$ = $1; }
            | ID                                                    { $$ = new VarList($1); }
            ;

EXPR        : EXPR '+' EXPR                                         { $$ = new BinOpExprNode(PLUS, $1, $3, @$); }
            | EXPR '-' EXPR                                         { $$ = new BinOpExprNode(MINUS, $1, $3, @$); }
            | EXPR '*' EXPR                                         { $$ = new BinOpExprNode(MUL, $1, $3, @$); }
            | EXPR '/' EXPR                                         { $$ = new BinOpExprNode(DIV, $1, $3, @$); }
            | EXPR '%' EXPR                                         { $$ = new BinOpExprNode(MOD, $1, $3, @$); }
            | '(' EXPR ')'                                          { $$ = $2; }
            | '-' EXPR %prec NEG                                    { $$ = new UnaryOpExprNode($2, @$); }
            | ARR_REF                                               { $$ = new ArrayReferenceExprNode($1, @$); }
            | ID                                                    { $$ = new IdExprNode($1, @$); }
            | NUM                                                   { $$ = new ConstExprNode(INT_CONST, $1, @$); }
            | FLT                                                   { $$ = new ConstExprNode(FLOAT_CONST, $1, @$); }
            ;

ARR_REF     : ID '[' EXPR ']'                                       { $$ = new PartialArrReference($1); $$->insert($3); }
            | ARR_REF '[' EXPR ']'                                  { $1->insert($3); $$ = $1; }
            ;

BOOL        : BOOL OR BOOL                                          { $$ = new BinOpBoolNode($1, $3, OR_OP, @$); }
            | BOOL AND BOOL                                         { $$ = new BinOpBoolNode($1, $3, AND_OP, @$); }
            | '!' BOOL %prec NOT                                    { $$ = new BinOpBoolNode($2, nullptr, NOT_OP, @$); }
            | '(' BOOL ')'                                          { $$ = $2; }
            | EXPR RELOP EXPR                                       { $$ = new RelOpBoolNode($1, $3, $2, @$); }
            | TRUE                                                  { $$ = new ConstBoolNode(true, @$); }
            | FALSE                                                 { $$ = new ConstBoolNode(false, @$); }
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

static std::string operandJson(const Operand &o) {
    const char *kind = o.kind == OperandKind::Const ? "const"
                     : o.kind == OperandKind::Temp  ? "temp" : "var";
    return std::string("{\"k\":\"") + kind + "\",\"v\":\"" + jsonEscape(o.text) + "\"}";
}

static std::string locationJson(Location l) {
    std::string out;
    out += "{";
    out += "\"startLineNumber\":";
    out += std::to_string(l.first_line);
    out += ",\"startColumn\":";
    out += std::to_string(l.first_column);
    out += ",\"endLineNumber\":";
    out += std::to_string(l.last_line);
    out += ",\"endColumn\":";
    out += std::to_string(l.last_column);
    out += "}";
    return out;
}

//   copy  {dst,a}          bin  {dst,a,o,b}      neg  {dst,a}
//   load  {dst,a,b}        store {dst,a,b}
//   if    {a,o,b,target}   goto {target}
static std::string instrJson(const Instr &i) {
    std::string out = "{\"labels\":[";
    for (size_t k = 0; k < i.labels.size(); k++) {
        if (k) out += ",";
        out += std::to_string(i.labels[k]);
    }
    out += "],";
    out += "\"location\":";
    out += locationJson(i.range);
    out += ",";

    switch (i.op) {
        case Op::Copy:
            out += "\"op\":\"copy\",\"dst\":" + operandJson(i.dst) + ",\"a\":" + operandJson(i.a); break;
        case Op::Bin:
            out += "\"op\":\"bin\",\"dst\":" + operandJson(i.dst) + ",\"a\":" + operandJson(i.a)
                 + ",\"o\":\"" + jsonEscape(i.oper) + "\",\"b\":" + operandJson(i.b); break;
        case Op::Neg:
            out += "\"op\":\"neg\",\"dst\":" + operandJson(i.dst) + ",\"a\":" + operandJson(i.a); break;
        case Op::Load:
            out += "\"op\":\"load\",\"dst\":" + operandJson(i.dst) + ",\"a\":" + operandJson(i.a)
                 + ",\"b\":" + operandJson(i.b); break;
        case Op::Store:
            out += "\"op\":\"store\",\"dst\":" + operandJson(i.dst) + ",\"a\":" + operandJson(i.a)
                 + ",\"b\":" + operandJson(i.b); break;
        case Op::If:
            out += "\"op\":\"if\",\"a\":" + operandJson(i.a) + ",\"o\":\"" + jsonEscape(i.oper)
                 + "\",\"b\":" + operandJson(i.b) + ",\"target\":" + std::to_string(i.target); break;
        case Op::Goto:
            out += "\"op\":\"goto\",\"target\":" + std::to_string(i.target); break;
    }
    return out + "}";
}

static std::string listingJson(const Listing &p) {
    std::string out = "{\"instrs\":[";
    for (size_t k = 0; k < p.instrs.size(); k++) {
        if (k) out += ",";
        out += instrJson(p.instrs[k]);
    }
    out += "],\"endLabels\":[";
    for (size_t k = 0; k < p.endLabels.size(); k++) {
        if (k) out += ",";
        out += std::to_string(p.endLabels[k]);
    }
    return out + "]}";
}

// Compiles `source` and returns JSON:
//   {"ok":true,"output":"<text form>","program":{"instrs":[...],"endLabels":[...]}}
//   {"ok":false,"error":{"message":"...","line":N|null}}
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
        Listing listing = prog->getCode().flatten();
        result = "{\"ok\":true,\"output\":\"" + jsonEscape(toText(listing))
               + "\",\"program\":" + listingJson(listing) + "}";
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
        code = toText(prog->getCode().flatten());
    } catch (const std::exception &e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
    std::cout << code << std::endl;

    return 0;
}

#endif // ICG_LIB
