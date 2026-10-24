#ifndef AST_H
#define AST_H

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "type.hpp"
#include "symtab.hpp"

#ifndef YYLTYPE
#define YYLTYPE_IS_DECLARED 1
typedef struct YYLTYPE {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
} YYLTYPE;
#endif

extern YYLTYPE yylloc;
typedef YYLTYPE Location;

inline int labelCount = 0;
inline int tempCount = 0;

inline int newlabel() {
    return ++labelCount;
}

struct LoopContext {
    int breakLabel;
    int continueLabel;
};

inline std::vector<LoopContext> loopStack;

inline void resetState() {
    labelCount = 0;
    tempCount = 0;
    loopStack.clear();
    ST = SymbolTable();
}

inline ArrayType *arrayTypeOrThrow(Type *type, STEntry *entry) {
    if (type == nullptr)
        throw std::runtime_error("'" + entry->name + "' is not declared");
    ArrayType *arrType = dynamic_cast<ArrayType *>(type);
    if (arrType == nullptr)
        throw std::runtime_error("'" + entry->name + "' is not an array, or is indexed with too many dimensions");
    return arrType;
}

inline STEntry *requireDeclared(const std::string &name) {
    STEntry *entry = ST.lookup(name);
    if (entry == nullptr) {
        throw std::runtime_error("'" + name + "' is not declared");
    }
    return entry;
}

class Address;
class ConstAddress;
class TempAddress;
class RefAddress;

class Node;
class ExprNode;
class BoolNode;
class StmtNode;
class TypeNode;
class VarList;
class PartialArrReference;

bool needsNext(const StmtNode *stmt);

enum BoolOpType { AND_OP, OR_OP, NOT_OP };
enum BinOpType  { PLUS, MINUS, MUL, DIV, MOD };
enum ConstType  { INT_CONST, FLOAT_CONST };
enum BaseType   { INT_TYPE, FLOAT_TYPE };

enum class OperandKind { Const, Var, Temp };

class Address {
public:
    virtual ~Address() = default;
    virtual std::string toString() = 0;
    virtual OperandKind kind() const = 0;
};

class ConstAddress : public Address {
    std::string val;
public:
    explicit ConstAddress(std::string val) : val(std::move(val)) {}
    std::string toString() override { return val; }
    OperandKind kind() const override { return OperandKind::Const; }
};

class TempAddress : public Address {
    int tmpno;
    static int getNextId() {
        return ++tempCount;
    }
public:
    TempAddress() : tmpno(getNextId()) {}
    std::string toString() override {
        std::stringstream ss;
        ss << "t" << tmpno;
        return ss.str();
    }
    OperandKind kind() const override { return OperandKind::Temp; }
};

class RefAddress : public Address {
public:
    std::string name;
    STEntry *entry = nullptr;

    explicit RefAddress(std::string name) : name(std::move(name)) {}

    STEntry *resolve() {
        if (entry != nullptr) return entry;
        entry = ST.lookup(name);
        if (entry == nullptr) {
            throw std::runtime_error("'" + name + "' is not declared");
        }
        return entry;
    }
    std::string toString() override { return name; }
    OperandKind kind() const override { return OperandKind::Var; }
};

struct Operand {
    OperandKind kind = OperandKind::Var;
    std::string text;
};

inline Operand operandOf(Address *a) {
    return { a->kind(), a->toString() };
}

inline Operand constOperand(int value) {
    return { OperandKind::Const, std::to_string(value) };
}

inline Operand varOperand(const std::string &name) {
    return { OperandKind::Var, name };
}

enum class Op {
    Copy,   // dst = a
    Bin,    // dst = a op b
    Neg,    // dst = minus a
    Load,   // dst = a[b]
    Store,  // dst[a] = b
    If,     // if a rel b goto target
    Goto    // goto target
};

struct Instr {
    Op op = Op::Goto;
    Operand dst, a, b;
    std::string oper;
    int target = 0;
    std::vector<int> labels;
    Location range;

    // dst = a
    static Instr copy(Operand dst, Operand a) {
        Instr i; i.op = Op::Copy; i.dst = dst; i.a = a; return i;
    }
    // dst = a op b
    static Instr bin(Operand dst, Operand a, std::string oper, Operand b) {
        Instr i; i.op = Op::Bin; i.dst = dst; i.a = a; i.oper = std::move(oper); i.b = b; return i;
    }
    // dst = minus a
    static Instr neg(Operand dst, Operand a) {
        Instr i; i.op = Op::Neg; i.dst = dst; i.a = a; return i;
    }
    // dst = a[b]
    static Instr load(Operand dst, Operand arr, Operand index) {
        Instr i; i.op = Op::Load; i.dst = dst; i.a = arr; i.b = index; return i;
    }
    // dst[a] = b
    static Instr store(Operand arr, Operand index, Operand value) {
        Instr i; i.op = Op::Store; i.dst = arr; i.a = index; i.b = value; return i;
    }
    // if a rel b goto target
    static Instr ifGoto(Operand a, std::string rel, Operand b, int target) {
        Instr i; i.op = Op::If; i.a = a; i.oper = std::move(rel); i.b = b; i.target = target; return i;
    }
    // goto target
    static Instr jump(int target) {
        Instr i; i.op = Op::Goto; i.target = target; return i;
    }
};

struct Listing {
    std::vector<Instr> instrs;
    std::vector<int> endLabels;
};

class Code {
    struct Item {
        bool isLabel;
        int label;
        Instr instr;
    };
    std::vector<Item> items;
public:
    void emit(Location range, Instr instr) { instr.range = range; items.push_back({false, 0, instr}); }
    void placeLabel(int l)        { items.push_back({true, l, Instr()}); }
    void append(const Code &other) {
        items.insert(items.end(), other.items.begin(), other.items.end());
    }

    Listing flatten() const {
        Listing p;
        std::vector<int> pending;
        for (const Item &item : items) {
            if (item.isLabel) {
                pending.push_back(item.label);
            } else {
                Instr i = item.instr;
                i.labels = pending;
                pending.clear();
                p.instrs.push_back(i);
            }
        }
        p.endLabels = pending;
        return p;
    }
};


inline std::string instrText(const Instr &i) {
    std::stringstream ss;
    switch (i.op) {
        case Op::Copy:  ss << i.dst.text << " = " << i.a.text; break;
        case Op::Bin:   ss << i.dst.text << " = " << i.a.text << " " << i.oper << " " << i.b.text; break;
        case Op::Neg:   ss << i.dst.text << " = minus " << i.a.text; break;
        case Op::Load:  ss << i.dst.text << " = " << i.a.text << "[" << i.b.text << "]"; break;
        case Op::Store: ss << i.dst.text << "[" << i.a.text << "] = " << i.b.text; break;
        case Op::If:    ss << "if " << i.a.text << " " << i.oper << " " << i.b.text << " goto L" << i.target; break;
        case Op::Goto:  ss << "goto L" << i.target; break;
    }
    return ss.str();
}

inline std::string toText(const Listing &p) {
    std::stringstream ss;
    for (const Instr &i : p.instrs) {
        for (int l : i.labels) ss << "L" << l << ": \t";
        ss << instrText(i) << "\n";
    }
    for (int l : p.endLabels) ss << "L" << l << ": \t";
    return ss.str();
}

class Node {
public:
    virtual ~Node() = default;
    virtual Code getCode() { return Code(); }
};

class ExprNode : public Node {
protected:
    Location range;
public:
    Address *addr;
    ExprNode(Location range) : addr(nullptr), range(range) {}
    ~ExprNode() override = default;
    virtual Code getCode() override = 0;
};

class BinOpExprNode : public ExprNode {
    BinOpType op;
    ExprNode *left;
    ExprNode *right;
public:
    BinOpExprNode(BinOpType op, ExprNode *left, ExprNode *right, Location range)
        : ExprNode(range), op(op), left(left), right(right) {}

    std::string getOp() const {
        switch (op) {
            case PLUS:  return "+";
            case MINUS: return "-";
            case MUL:   return "*";
            case DIV:   return "/";
            case MOD:   return "%";
        }
        return "";
    }

    Code getCode() override {
        Code code;
        code.append(left->getCode());
        code.append(right->getCode());
        addr = new TempAddress();
        code.emit(range, Instr::bin(operandOf(addr), operandOf(left->addr), getOp(), operandOf(right->addr)));
        return code;
    }
};

class UnaryOpExprNode : public ExprNode {
    ExprNode *expr;
public:
    explicit UnaryOpExprNode(ExprNode *expr, Location range)
        : ExprNode(range), expr(expr) {}

    Code getCode() override {
        Code code;
        code.append(expr->getCode());
        addr = new TempAddress();
        code.emit(range, Instr::neg(operandOf(addr), operandOf(expr->addr)));
        return code;
    }
};

class IdExprNode : public ExprNode {
    std::string varName;
public:
    explicit IdExprNode(const std::string &varName, Location range)
        : ExprNode(range), varName(varName) {
        addr = new RefAddress(varName);
    }
    Code getCode() override {
        dynamic_cast<RefAddress *>(addr)->resolve();
        return Code(); 
    }
};

class ConstExprNode : public ExprNode {
    ConstType type;
    std::string value;
public:
    ConstExprNode(ConstType type, const std::string &value, Location range)
        : ExprNode(range), type(type), value(value) {
        addr = new ConstAddress(value);
    }
    Code getCode() override { return Code(); }
};

class PartialArrReference : public Node {
    public:
    RefAddress *base;
    std::vector<ExprNode *> indices;
    explicit PartialArrReference(std::string varName) { base = new RefAddress(varName); }
    void insert(ExprNode *index) { indices.push_back(index); }
};

class ArrayReferenceExprNode : public ExprNode {
    PartialArrReference *partialArrRef;
public:
    explicit ArrayReferenceExprNode(PartialArrReference *partialArrRef, Location range)
        : ExprNode(range), partialArrRef(partialArrRef) {}

    Code getCode() override {
        Code code;
        STEntry *entry = partialArrRef->base->resolve();
        Type *currType = entry->type;
        TempAddress* offsetAddr = nullptr;

        for (size_t i = 0; i < partialArrRef->indices.size(); ++i) {
            ExprNode* indexExpr = partialArrRef->indices[i];
            code.append(indexExpr->getCode());
            
            ArrayType* arrType = arrayTypeOrThrow(currType, entry);
            
            currType = arrType->elem;
            
            // t = E.addr * L.type.width
            TempAddress* t_term = new TempAddress();
            code.emit(range, Instr::bin(operandOf(t_term), operandOf(indexExpr->addr), "*", constOperand(currType->width)));
               
            if (i == 0) {
                offsetAddr = t_term;
            } else {
                // L.addr = L1.addr + t
                TempAddress* t_newOffset = new TempAddress();
                code.emit(range, Instr::bin(operandOf(t_newOffset), operandOf(offsetAddr), "+", operandOf(t_term)));
                offsetAddr = t_newOffset;
            }
        }
        
        addr = new TempAddress();
        if (offsetAddr) {
            code.emit(range, Instr::load(operandOf(addr), operandOf(partialArrRef->base), operandOf(offsetAddr)));
        }
        
        return code;
    }
};

class BoolNode : public Node {
protected:
    Location range;
public:
    int t = 0;
    int f = 0;
    BoolNode(Location range): range(range) {}
    ~BoolNode() override = default;
    virtual Code getCode() override = 0;
};

class ConstBoolNode : public BoolNode {
    bool value;
public:
    explicit ConstBoolNode(bool value, Location range)
        : BoolNode(range), value(value) {}
    Code getCode() override {
        Code code;
        code.emit(range, Instr::jump(value ? t : f));
        return code;
    }
};

class RelOpBoolNode : public BoolNode {
    ExprNode *left;
    ExprNode *right;
    std::string op;
public:
    RelOpBoolNode(ExprNode *left, ExprNode *right, std::string op, Location range)
        : BoolNode(range), left(left), right(right), op(std::move(op)) {}

    Code getCode() override {
        Code code;
        code.append(left->getCode());
        code.append(right->getCode());
        code.emit(range, Instr::ifGoto(operandOf(left->addr), op, operandOf(right->addr), t));
        code.emit(range, Instr::jump(f));
        return code;
    }
};

class BinOpBoolNode : public BoolNode {
    BoolNode *left;
    BoolNode *right;
    BoolOpType op;
public:
    BinOpBoolNode(BoolNode *left, BoolNode *right, BoolOpType op, Location range)
        : BoolNode(range), left(left), right(right), op(op) {}

    Code getCode() override {
        Code code;
        if (op == AND_OP) {
            left->t = newlabel();
            left->f = f;
            right->t = t;
            right->f = f;
            code.append(left->getCode());
            code.placeLabel(left->t);
            code.append(right->getCode());
        } else if (op == OR_OP) {
            left->t = t;
            left->f = newlabel();
            right->t = t;
            right->f = f;
            code.append(left->getCode());
            code.placeLabel(left->f);
            code.append(right->getCode());
        } else if (op == NOT_OP) {
            left->t = f;
            left->f = t;
            code.append(left->getCode());
        }
        return code;
    }
};

class StmtNode : public Node {
protected:
    Location range;
public:
    int next = 0;
    StmtNode(Location range): range(range) {}
    ~StmtNode() override = default;
    virtual Code getCode() override = 0;
};

class WhileStmtNode : public StmtNode {
    BoolNode *boolExpr;
    StmtNode *stmt;
    Location whileToken;
public:
    WhileStmtNode(BoolNode *boolExpr, StmtNode *stmt, Location range, Location whileToken) 
        : boolExpr(boolExpr), stmt(stmt), StmtNode(range), whileToken(whileToken) {}
    Code getCode() override { 
        Code code;
        int begin = newlabel();
        code.placeLabel(begin);
        boolExpr->t = newlabel();
        boolExpr->f = next;
        code.append(boolExpr->getCode());
        stmt->next = begin;
        loopStack.push_back({next, begin});
        code.placeLabel(boolExpr->t);
        code.append(stmt->getCode());
        loopStack.pop_back();
        code.emit(range, Instr::jump(begin));
        return code;
    }
};

class ForStmtNode : public StmtNode { 
    StmtNode *init;
    BoolNode *cond;
    StmtNode *incr;
    StmtNode *body;
    Location forToken;
    
public:
    ForStmtNode(StmtNode *init, BoolNode *cond, StmtNode *incr, StmtNode *body, Location range, Location forToken): 
        init(init), cond(cond), incr(incr), body(body),
        StmtNode(range), forToken(forToken) {}
    Code getCode() override {
        Code code;
        int begin = newlabel();
        int incrLabel = newlabel(); // `continue` jumps here, so incr is not skipped
        if (init) code.append(init->getCode());
        code.placeLabel(begin);
        if (cond) {
            cond->t = newlabel();
            cond->f = next;
            code.append(cond->getCode());
            code.placeLabel(cond->t);
        }
        body->next = incrLabel;
        loopStack.push_back({next, incrLabel});
        code.append(body->getCode());
        loopStack.pop_back();
        code.placeLabel(incrLabel);
        if (incr) code.append(incr->getCode());
        code.emit(range, Instr::jump(begin));
        return code;
    }
};

class IfStmtNode : public StmtNode {
    BoolNode *boolExpr;
    StmtNode *thenStmt;
    StmtNode *elseStmt;
    Location ifToken, elseToken;
public:
    IfStmtNode(BoolNode *boolExpr, StmtNode *thenStmt, StmtNode *elseStmt, Location range, Location ifToken, Location elseToken)
        : boolExpr(boolExpr), thenStmt(thenStmt), elseStmt(elseStmt),
         StmtNode(range), ifToken(ifToken), elseToken(elseToken) {}
    Code getCode() override {
        Code code;
        if (elseStmt == nullptr) {
            boolExpr->t = newlabel();
            boolExpr->f = next;
            code.append(boolExpr->getCode());
            code.placeLabel(boolExpr->t);
            thenStmt->next = next;
            code.append(thenStmt->getCode());
        } else {
            boolExpr->t = newlabel();
            boolExpr->f = newlabel();
            code.append(boolExpr->getCode());
            code.placeLabel(boolExpr->t);
            thenStmt->next = next;
            code.append(thenStmt->getCode());
            code.emit(range, Instr::jump(next));
            code.placeLabel(boolExpr->f);
            elseStmt->next = next;
            code.append(elseStmt->getCode());
        }
        return code;
    }
};

class BreakStmtNode : public StmtNode {
public:
    explicit BreakStmtNode(Location range) : StmtNode(range) {}
    Code getCode() override {
        if (loopStack.empty()) throw std::runtime_error("'break' outside of a loop");
        Code code;
        code.emit(range, Instr::jump(loopStack.back().breakLabel));
        return code;
    }
};

class ContinueStmtNode : public StmtNode {
public:
    explicit ContinueStmtNode(Location range) : StmtNode(range) {}
    Code getCode() override {
        if (loopStack.empty()) throw std::runtime_error("'continue' outside of a loop");
        Code code;
        code.emit(range, Instr::jump(loopStack.back().continueLabel));
        return code;
    }
};

class TypeNode : public Node {
    BaseType baseType;
    std::vector<int> dimensions;
public:
    Type *type;
    TypeNode(int baseType, const std::vector<int> *dims = nullptr)
        : baseType(static_cast<BaseType>(baseType)) {
        if (dims) dimensions = *dims;
        Type *ptr = (baseType == INT_TYPE) ? (Type *)new IntType : (Type *)new FloatType;
        for (int i = (int)dimensions.size() - 1; i >= 0; i--) {
            ptr = new ArrayType(dimensions[i], ptr);
        }
        type = ptr;
    }
};

class VarList : public Node {
public:
    std::vector<std::string> varNames;
    explicit VarList(const std::string &varName) {
        varNames.push_back(varName);
    }
    void insert(const std::string &varName) {
        varNames.push_back(varName);
    }
};

class DeclStmtNode : public StmtNode {
    TypeNode *type;
    VarList *varList;
public:
    DeclStmtNode(TypeNode *type, VarList *varList, Location range) 
        : type(type), varList(varList), StmtNode(range) {}
    Code getCode() override { 
        for (auto var : varList->varNames) {
            ST.insert(var, type->type);
        }
        return Code();
    }
};

class AsgnStmtNode : public StmtNode {
    std::string varName;
    ExprNode *expr;
    Location eqToken;
public:
    AsgnStmtNode(std::string varName, ExprNode *expr, Location range, Location eq)
        : varName(std::move(varName)), expr(expr), StmtNode(range), eqToken(eq) {}

    Code getCode() override {
        Code code;
        requireDeclared(varName);
        code.append(expr->getCode());
        code.emit(range, Instr::copy(varOperand(varName), operandOf(expr->addr)));
        return code;
    }
};

class ArrAsgnStmtNode : public StmtNode {
    PartialArrReference *partialArrRef;
    ExprNode *expr;
    Location eqToken;
public:
    ArrAsgnStmtNode(PartialArrReference *partialArrRef, ExprNode *expr, Location range, Location eq)
        : partialArrRef(partialArrRef), expr(expr), StmtNode(range), eqToken(eq) {}

    Code getCode() override {
        Code code;
        
        code.append(expr->getCode());
        
        STEntry *entry = partialArrRef->base->resolve();
        Type *currType = entry->type;
        TempAddress* offsetAddr = nullptr;

        for (size_t i = 0; i < partialArrRef->indices.size(); ++i) {
            ExprNode* indexExpr = partialArrRef->indices[i];
            code.append(indexExpr->getCode());
            
            ArrayType* arrType = arrayTypeOrThrow(currType, entry);
            
            currType = arrType->elem;
            
            // t = E.addr * L.type.width
            TempAddress* t_term = new TempAddress();
            code.emit(range, Instr::bin(operandOf(t_term), operandOf(indexExpr->addr), "*", constOperand(currType->width)));
               
            if (i == 0) {
                offsetAddr = t_term;
            } else {
                // L.addr = L1.addr + t
                TempAddress* t_newOffset = new TempAddress();
                code.emit(range, Instr::bin(operandOf(t_newOffset), operandOf(offsetAddr), "+", operandOf(t_term)));
                offsetAddr = t_newOffset;
            }
        }
        
        if (offsetAddr) {
            code.emit(range, Instr::store(operandOf(partialArrRef->base), operandOf(offsetAddr), operandOf(expr->addr)));
        }        
        return code;
    }
};

class StmtListNode : public StmtNode {
    std::vector<StmtNode *> statements;
public:
    explicit StmtListNode(Location range) : StmtNode(range) {}
    void insert(StmtNode *node, Location x) {
        statements.push_back(node);
        range = x;
    }

    Code getCode() override {
        Code code;
        for (size_t i = 0; i < statements.size(); i++) {
            StmtNode *stmt = statements[i];
            bool last = (i + 1 == statements.size());
            if (last || needsNext(stmt)) {
                stmt->next = last ? next : newlabel();
            } else {
                stmt->next = 0;
            }
            code.append(stmt->getCode());
            if (!last && needsNext(stmt)) code.placeLabel(stmt->next);
        }
        return code;
    }
};

inline bool needsNext(const StmtNode *stmt) {
    if (dynamic_cast<const IfStmtNode *>(stmt) != nullptr) return true;
    if (dynamic_cast<const WhileStmtNode *>(stmt) != nullptr) return true;
    if (dynamic_cast<const ForStmtNode *>(stmt) != nullptr) return true;
    if (dynamic_cast<const StmtListNode *>(stmt) != nullptr) return true;
    return false;
}

class Program {
    StmtListNode *stmtListNode;
public:
    explicit Program(StmtListNode *stmtListNode): stmtListNode(stmtListNode) {}
    Code getCode() {
        Code code;
        stmtListNode->next = newlabel();
        code.append(stmtListNode->getCode());
        code.placeLabel(stmtListNode->next);
        return code;
    }
};

#endif // AST_H