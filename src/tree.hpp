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

// Counters for generated labels (L1, L2, ...) and temporaries (t1, t2, ...).
inline int labelCount = 0;
inline int tempCount = 0;

inline int newlabel() {
    return ++labelCount;
}

inline std::string label(int l) {
    std::stringstream ss;
    ss << "L" << l << ": \t";
    return ss.str();
}

// Jump targets of the loops enclosing the statement being generated
// (innermost last). Used by `break` and `continue`.
struct LoopContext {
    int breakLabel;
    int continueLabel;
};

inline std::vector<LoopContext> loopStack;

// Restores all global state so a fresh program can be compiled from scratch
// (needed when compiling repeatedly in one process, e.g. in the browser).
inline void resetState() {
    labelCount = 0;
    tempCount = 0;
    loopStack.clear();
    ST = SymbolTable();
}

// Array indexing needs an array type; report why when it isn't one.
inline ArrayType *arrayTypeOrThrow(Type *type, const std::string &name) {
    if (type == nullptr)
        throw std::runtime_error("'" + name + "' is not declared");
    ArrayType *arrType = dynamic_cast<ArrayType *>(type);
    if (arrType == nullptr)
        throw std::runtime_error("'" + name + "' is not an array, or is indexed with too many dimensions");
    return arrType;
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

enum BoolOpType { AND_OP, OR_OP, NOT_OP };
enum BinOpType  { PLUS, MINUS, MUL, DIV, MOD };
enum ConstType  { INT_CONST, FLOAT_CONST };
enum BaseType   { INT_TYPE, FLOAT_TYPE };

class Address {
public:
    virtual ~Address() = default;
    virtual std::string toString() = 0;
};

class ConstAddress : public Address {
    std::string val;
public:
    explicit ConstAddress(std::string val) : val(std::move(val)) {}
    std::string toString() override { return val; }
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
};

class RefAddress : public Address {
    std::string name;
public:
    explicit RefAddress(std::string name) : name(std::move(name)) {}
    std::string toString() override { return name; }
};

class Node {
public:
    virtual ~Node() = default;
    virtual std::string getCode() { return "ROOTO"; }
};

class ExprNode : public Node {
public:
    Address *addr;
    ExprNode() : addr(nullptr) {}
    ~ExprNode() override = default;
    virtual std::string getCode() override = 0;
};

class BinOpExprNode : public ExprNode {
    BinOpType op;
    ExprNode *left;
    ExprNode *right;
public:
    BinOpExprNode(BinOpType op, ExprNode *left, ExprNode *right)
        : op(op), left(left), right(right) {}

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

    std::string getCode() override {
        std::stringstream ss;
        addr = new TempAddress();
        ss << left->getCode();
        ss << right->getCode();
        ss << addr->toString() << " = " << left->addr->toString() 
           << " " << getOp() << " " << right->addr->toString() << "\n";
        return ss.str();
    }
};

class UnaryOpExprNode : public ExprNode {
    ExprNode *expr;
public:
    explicit UnaryOpExprNode(ExprNode *expr) : expr(expr) {}

    std::string getCode() override {
        std::stringstream ss;
        addr = new TempAddress();
        ss << expr->getCode();
        ss << addr->toString() << " = minus " << expr->addr->toString() << "\n";
        return ss.str();
    }
};

class IdExprNode : public ExprNode {
    std::string varName;
public:
    explicit IdExprNode(const std::string &varName) : varName(varName) {
        addr = new RefAddress(varName);
    }
    std::string getCode() override { return ""; }
};

class ConstExprNode : public ExprNode {
    ConstType type;
    std::string value;
public:
    ConstExprNode(ConstType type, const std::string &value) 
        : type(type), value(value) {
        addr = new ConstAddress(value);
    }
    std::string getCode() override { return ""; }
};

class PartialArrReference : public Node {
public:
    std::string varName;
    std::vector<ExprNode *> indices;
    explicit PartialArrReference(std::string varName) : varName(std::move(varName)) {}
    void insert(ExprNode *index) { indices.push_back(index); }
};

class ArrayReferenceExprNode : public ExprNode {
    PartialArrReference *partialArrRef;
public:
    explicit ArrayReferenceExprNode(PartialArrReference *partialArrRef) 
        : partialArrRef(partialArrRef) {}

    std::string getCode() override {
        std::stringstream ss;
        Type* currType = ST.getType(partialArrRef->varName);
        TempAddress* offsetAddr = nullptr;

        for (size_t i = 0; i < partialArrRef->indices.size(); ++i) {
            ExprNode* indexExpr = partialArrRef->indices[i];
            ss << indexExpr->getCode();
            
            ArrayType* arrType = arrayTypeOrThrow(currType, partialArrRef->varName);
            
            currType = arrType->elem;
            
            // t = E.addr * L.type.width
            TempAddress* t_term = new TempAddress();
            ss << t_term->toString() << " = " << indexExpr->addr->toString() 
               << " * " << currType->width << "\n";
               
            if (i == 0) {
                offsetAddr = t_term;
            } else {
                // L.addr = L1.addr + t
                TempAddress* t_newOffset = new TempAddress();
                ss << t_newOffset->toString() << " = " << offsetAddr->toString() 
                   << " + " << t_term->toString() << "\n";
                offsetAddr = t_newOffset;
            }
        }
        
        addr = new TempAddress();
        if (offsetAddr) {
            ss << addr->toString() << " = " << partialArrRef->varName 
               << "[" << offsetAddr->toString() << "]\n";
        }
        
        return ss.str();
    }
};

class BoolNode : public Node {
public:
    int t = 0;
    int f = 0;
    ~BoolNode() override = default;
    virtual std::string getCode() override = 0;
};

class ConstBoolNode : public BoolNode {
    bool value;
public:
    explicit ConstBoolNode(bool value) : value(value) {}
    std::string getCode() override {
        std::stringstream ss;
        if (value) {
            ss << "goto L" << t << std::endl;
        } else {
            ss << "goto L" << f << std::endl;
        }
        return ss.str();
    }
};

class RelOpBoolNode : public BoolNode {
    ExprNode *left;
    ExprNode *right;
    std::string op;
public:
    RelOpBoolNode(ExprNode *left, ExprNode *right, std::string op)
        : left(left), right(right), op(std::move(op)) {}

    std::string getCode() override {
        std::stringstream ss;
        ss << left->getCode();
        ss << right->getCode();
        ss << "if " << left->addr->toString() << " " << op << " " 
           << right->addr->toString() << " goto L" << t << "\n";
        ss << "goto L" << f << "\n";
        return ss.str();
    }
};

class BinOpBoolNode : public BoolNode {
    BoolNode *left;
    BoolNode *right;
    BoolOpType op;
public:
    BinOpBoolNode(BoolNode *left, BoolNode *right, BoolOpType op)
        : left(left), right(right), op(op) {}

    std::string getCode() override {
        std::stringstream ss;
        if (op == AND_OP) {
            left->t = newlabel();
            left->f = f;
            right->t = t;
            right->f = f;
            ss << left->getCode();
            ss << label(left->t);
            ss << right->getCode();
        } else if (op == OR_OP) {
            left->t = t;
            left->f = newlabel();
            right->t = t;
            right->f = f;
            ss << left->getCode();
            ss << label(left->f);
            ss << right->getCode();
        } else if (op == NOT_OP) {
            left->t = f;
            left->f = t;
            ss << left->getCode();
        }
        return ss.str();
    }
};

class StmtNode : public Node {
public:
    int next = 0;
    ~StmtNode() override = default;
    virtual std::string getCode() override = 0;
};

class WhileStmtNode : public StmtNode {
    BoolNode *boolExpr;
    StmtNode *stmt;
public:
    WhileStmtNode(BoolNode *boolExpr, StmtNode *stmt) 
        : boolExpr(boolExpr), stmt(stmt) {}
    std::string getCode() override { 
        std::stringstream ss;
        int begin = newlabel();
        ss << label(begin);
        boolExpr->t = newlabel();
        boolExpr->f = next;
        ss << boolExpr->getCode();
        stmt->next = begin;
        loopStack.push_back({next, begin});
        ss << label(boolExpr->t) << stmt->getCode();
        loopStack.pop_back();
        ss << "goto L" << begin << std::endl;
        return ss.str();
    }
};

// for (init; cond; incr) body    -- init, cond and incr may each be null
class ForStmtNode : public StmtNode { 
    StmtNode *init;
    BoolNode *cond;
    StmtNode *incr;
    StmtNode *body;
public:
    ForStmtNode(StmtNode *init, BoolNode *cond, StmtNode *incr, StmtNode *body): init(init), cond(cond), incr(incr), body(body) {}
    std::string getCode() override {
        std::stringstream ss;
        int begin = newlabel();
        int incrLabel = newlabel(); // `continue` jumps here, so incr is not skipped
        if (init) ss << init->getCode();
        ss << label(begin);
        if (cond) {
            cond->t = newlabel();
            cond->f = next;
            ss << cond->getCode() << label(cond->t);
        }
        body->next = incrLabel;
        loopStack.push_back({next, incrLabel});
        ss << body->getCode();
        loopStack.pop_back();
        ss << label(incrLabel);
        if (incr) ss << incr->getCode();
        ss << "goto L" << begin << std::endl;
        return ss.str();
    }
};

class IfStmtNode : public StmtNode {
    BoolNode *boolExpr;
    StmtNode *thenStmt;
    StmtNode *elseStmt;
public:
    IfStmtNode(BoolNode *boolExpr, StmtNode *thenStmt, StmtNode *elseStmt = nullptr)
        : boolExpr(boolExpr), thenStmt(thenStmt), elseStmt(elseStmt) {}

    std::string getCode() override {
        std::stringstream ss;
        if (elseStmt == nullptr) {
            boolExpr->t = newlabel();
            boolExpr->f = next;
            ss << boolExpr->getCode() << label(boolExpr->t);
            thenStmt->next = next;
            ss << thenStmt->getCode();
        } else {
            boolExpr->t = newlabel();
            boolExpr->f = newlabel();
            ss << boolExpr->getCode() << label(boolExpr->t);
            thenStmt->next = next;
            ss << thenStmt->getCode();
            ss << "goto L" << next << "\n";
            ss << label(boolExpr->f);
            elseStmt->next = next;
            ss << elseStmt->getCode();
        }
        return ss.str();
    }
};

class BreakStmtNode : public StmtNode {
public:
    std::string getCode() override {
        if (loopStack.empty()) throw std::runtime_error("'break' outside of a loop");
        std::stringstream ss;
        ss << "goto L" << loopStack.back().breakLabel << "\n";
        return ss.str();
    }
};

class ContinueStmtNode : public StmtNode {
public:
    std::string getCode() override {
        if (loopStack.empty()) throw std::runtime_error("'continue' outside of a loop");
        std::stringstream ss;
        ss << "goto L" << loopStack.back().continueLabel << "\n";
        return ss.str();
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
    DeclStmtNode(TypeNode *type, VarList *varList) 
        : type(type), varList(varList) {}
    std::string getCode() override { 
        for (auto var : varList->varNames) {
            ST.insert(var, type->type);
        }
        return ""; 
    }
};

class AsgnStmtNode : public StmtNode {
    std::string varName;
    ExprNode *expr;
public:
    AsgnStmtNode(std::string varName, ExprNode *expr)
        : varName(std::move(varName)), expr(expr) {}

    std::string getCode() override {
        std::stringstream ss;
        ss << expr->getCode();
        ss << varName << " = " << expr->addr->toString() << "\n";
        return ss.str();
    }
};

class ArrAsgnStmtNode : public StmtNode {
    PartialArrReference *partialArrRef;
    ExprNode *expr;
public:
    ArrAsgnStmtNode(PartialArrReference *partialArrRef, ExprNode *expr)
        : partialArrRef(partialArrRef), expr(expr) {}

    std::string getCode() override {
        std::stringstream ss;
        
        ss << expr->getCode();
        
        Type* currType = ST.getType(partialArrRef->varName);
        TempAddress* offsetAddr = nullptr;

        for (size_t i = 0; i < partialArrRef->indices.size(); ++i) {
            ExprNode* indexExpr = partialArrRef->indices[i];
            ss << indexExpr->getCode();
            
            ArrayType* arrType = arrayTypeOrThrow(currType, partialArrRef->varName);
            
            currType = arrType->elem;
            
            // t = E.addr * L.type.width
            TempAddress* t_term = new TempAddress();
            ss << t_term->toString() << " = " << indexExpr->addr->toString() 
               << " * " << currType->width << "\n";
               
            if (i == 0) {
                offsetAddr = t_term;
            } else {
                // L.addr = L1.addr + t
                TempAddress* t_newOffset = new TempAddress();
                ss << t_newOffset->toString() << " = " << offsetAddr->toString() 
                   << " + " << t_term->toString() << "\n";
                offsetAddr = t_newOffset;
            }
        }
        
        if (offsetAddr) {
            ss << partialArrRef->varName << "[" << offsetAddr->toString() << "] = " 
               << expr->addr->toString() << "\n";
        }
        return ss.str();
    }
};

class StmtListNode : public StmtNode {
    std::vector<StmtNode *> statements;
public:
    explicit StmtListNode(StmtNode *node) {
        statements.push_back(node);
    }
    void insert(StmtNode *node) {
        statements.push_back(node);
    }

    std::string getCode() override {
        std::stringstream ss;
        for (size_t i = 0; i < statements.size(); ++i) {
            if (i < statements.size() - 1) {
                int l = newlabel();
                statements[i]->next = l;
                ss << statements[i]->getCode();
                ss << label(l);
            } else {
                statements[i]->next = this->next;
                ss << statements[i]->getCode();
            }
        }
        return ss.str();
    }
};

class Program {
    StmtListNode *stmtListNode;
public:
    explicit Program(StmtListNode *stmtListNode): stmtListNode(stmtListNode) {}
    std::string getCode() {
        std::stringstream ss;
        stmtListNode->next = newlabel();
        ss << stmtListNode->getCode();
        ss << label(stmtListNode->next);
        return ss.str();
    }
};

#endif // AST_H