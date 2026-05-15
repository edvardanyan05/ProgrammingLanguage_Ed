#pragma once
#include <iostream>
#include <memory>
#include <map>
#include <string>
#include <cstdint>
#include <vector>
#include "binding_table.hpp"

enum class TapeOp : uint8_t {
    // RV32I arithmetic and logic
    ADD, MOV, SUB, AND, OR, XOR, NOT, // bitwise not
    SLL, SRL, SRA,
    SLT, SLTU,
    ADDI, ANDI, ORI, XORI,
    SLLI, SRLI, SRAI,
    LUI, AUIPC,

    // RV32I control flow
    JAL, JALR,
    BEQ, BNE, BLT, BGE, BLTU, BGEU,

    // RV32I memory
    LW, SW,

    // Existing VM extensions
    MUL, DIV, MODULO, POW, FLOOR_DIV, FRAC_DIV,
    UNARY, LOAD_CONST, LOAD_VAR, LOAD_STR, LOAD_NONE, 
    UNDEFINED,
    CMP_GT, CMP_LT, CMP_GET, CMP_LET, CMP_EQ, CMP_NEQ,
    JMP, JZ, JNZ,
    STORE_VAR,
    PRINT, PRINT_STR,
    LOGICAL_AND, LOGICAL_OR, LOGICAL_NOT,
    CALL, RETURN, PUSH_ARG, LOAD_PARAM,
    LOAD, STORE,
    INPUT, // user-input
    LENGTH, // string length
    // math functions
    SIN, COS, TAN,
    ASIN, ACOS, ATAN, ATAN2,
    SQRT, EXP, LOG, LOG10,
    CEIL, FLOOR, ABS, ROUND,
    FMOD, CBRT, MATH_POW, LOG2, LOG_AB, // log(b)/log(a)
    // math constants
    CONST_PI, CONST_E, // pi, e
    // Conversions
    ORD, // char -> int
    CHR, // int -> char
    BIN, // int -> binary string
    OCT, // int -> octal string
    DEC, // int -> decimal string
    HEX, // int -> hexadecimal string
    TYPE, // type(argument) -> "string" / "number" / "none"
};

class SynNode {
public:
    virtual ~SynNode() = default;
    virtual void print(std::string prefix = "", bool isLast = true) const = 0;
    virtual std::vector<std::shared_ptr<SynNode>> getChildren() const = 0;
};

class LitNumber : public SynNode {
    double value;
public:
    LitNumber(double val) : value(val) {}
    double getValue() const { return value; }
    void print(std::string prefix, bool isLast) const override;
    std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};

class LitMathConst : public SynNode {
    private:
        TapeOp constant;
    public:
        LitMathConst(TapeOp c) : constant(c) {}
        TapeOp getConstant() const { return constant; }
        void print(std::string prefix, bool isLast) const override {
        std::cout << prefix << (isLast ? "└── " : "├── ")
                  << (constant == TapeOp::CONST_PI ? "Constant: PI" : "Constant: E") << std::endl;
        }
        std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};

class RefSlot : public SynNode {
    bool isLocal;
    union {
        size_t globalAddr;
        int32_t localOffset;
    };
public:
    RefSlot(size_t addr) : isLocal(false), globalAddr(addr) {}
    RefSlot(int32_t off)  : isLocal(true), localOffset(off) {}

    bool getIsLocal() const { return isLocal; }
    size_t getGlobalAddr() const { return globalAddr; }
    int32_t getLocalOffset() const { return localOffset; }

    void print(std::string prefix, bool isLast) const override {
        std::cout << prefix << (isLast ? "└── " : "├── ")
                  << "Var (" << (isLocal ? "local off=" : "global addr=")
                  << (isLocal ? localOffset : (int)globalAddr) << ")" << std::endl;
    }
    std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};

class OpBinary : public SynNode {
    std::string op;
    std::shared_ptr<SynNode> left, right;
public:
    OpBinary(const std::string& o, std::shared_ptr<SynNode> l, std::shared_ptr<SynNode> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
    void print(std::string prefix, bool isLast) const override;
    TapeOp getOpCode() const;
    std::shared_ptr<SynNode> getLeft() const { return left; }
    std::shared_ptr<SynNode> getRight() const { return right; }
    std::string getOp() const { return op; }
    std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {left, right}; }
};

class OpUnary : public SynNode {
    std::string op;
    std::shared_ptr<SynNode> child;
public:
    OpUnary(const std::string& o, std::shared_ptr<SynNode> c) : op(o), child(std::move(c)) {}
    void print(std::string prefix, bool isLast) const override;
    std::string getOp() const { return op; }
    std::shared_ptr<SynNode> getChild() const { return child; }
    std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {child}; }
};

class OpPick : public SynNode {
    private:
        std::shared_ptr<SynNode> condition;
        std::shared_ptr<SynNode> trueExpr;
        std::shared_ptr<SynNode> falseExpr;
    public:
        OpPick(std::shared_ptr<SynNode> cond, std::shared_ptr<SynNode> trueExp, std::shared_ptr<SynNode> falseExp)
            : condition(std::move(cond)), trueExpr(std::move(trueExp)), falseExpr(std::move(falseExp)) {}

        std::shared_ptr<SynNode> getCondition() const { return condition; }
        std::shared_ptr<SynNode> getTrueExpr() const { return trueExpr; }
        std::shared_ptr<SynNode> getFalseExpr() const { return falseExpr; }
        
        void print(std::string prefix, bool isLast) const override {
            std::cout << prefix << (isLast ? "└── " : "├── ") << "Ternary: ? :" << std::endl;
            std::string newPrefix = prefix + (isLast ? "    " : "│   ");
            condition->print(newPrefix, false);
            trueExpr->print(newPrefix, false);
            falseExpr->print(newPrefix, true);
        }
        
        std::vector<std::shared_ptr<SynNode>> getChildren() const override {
            return {condition, trueExpr, falseExpr};
        }
};

class LitEmpty : public SynNode {
    public:
    void print(std::string prefix, bool isLast) const override {
        std::cout << prefix << (isLast ? "└── " : "├── ") << "None" << std::endl;
    }
    std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};

class Stmt : public SynNode {
    public:
        int lineNumber = 0;
        virtual ~Stmt() = default;
        virtual void print(std::string prefix, bool isLast) const = 0;
        std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};

class BuiltinLen : public SynNode {
    private:
        std::shared_ptr<SynNode> arg;
    public:
        BuiltinLen(std::shared_ptr<SynNode> a) : arg(std::move(a)) {}
        std::shared_ptr<SynNode> getArg() const { return arg; }
        void print(std::string prefix, bool isLast) const override {
            std::cout << prefix << (isLast ? "└── " : "├── ") << "Length" << std::endl;
            arg->print(prefix + (isLast ? "    " : "│   "), true);
        }
    std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {arg}; }
};

class StmtBlock : public Stmt {
    std::vector<std::shared_ptr<Stmt>> statements;
public:
    void addStatement(std::shared_ptr<Stmt> stmt) { statements.push_back(stmt); }
    void print(std::string prefix, bool isLast) const override;
    std::vector<std::shared_ptr<Stmt>> getStatements() const { return statements; }
};

class StmtAssign : public Stmt {
private:
    bool isLocalFlag;
    int32_t localOffset;
    size_t globalAddr;
    std::shared_ptr<SynNode> value;

public:
    StmtAssign(int32_t offset, std::shared_ptr<SynNode> val)
        : isLocalFlag(true), localOffset(offset), globalAddr(0), value(val) {}

    StmtAssign(size_t addr, std::shared_ptr<SynNode> val)
        : isLocalFlag(false), localOffset(0), globalAddr(addr), value(val) {}

    bool isLocal() const { return isLocalFlag; }
    int32_t getOffset() const { return localOffset; }
    size_t getAddress() const { return globalAddr; }
    std::shared_ptr<SynNode> getValue() const { return value; }

    void print(std::string prefix = "", bool isLast = true) const override {
        std::cout << prefix << (isLast ? "└── " : "├── ") << "Assignment (=)" << std::endl;
        if (isLocalFlag) {
            std::cout << prefix << (isLast ? "    " : "│   ") << "Local offset: " << localOffset << std::endl;
        } else {
            std::cout << prefix << (isLast ? "    " : "│   ") << "Global addr: " << globalAddr << std::endl;
        }
        value->print(prefix + (isLast ? "    " : "│   "), true);
    }
};

class IfStmt : public Stmt {
    std::shared_ptr<SynNode> condition;
    std::shared_ptr<Stmt> thenBranch, elseBranch;
public:
    IfStmt(std::shared_ptr<SynNode> cond,
                    std::shared_ptr<Stmt> thenBr,
                    std::shared_ptr<Stmt> elseBr = nullptr)
        : condition(cond), thenBranch(thenBr), elseBranch(elseBr) {}
    void print(std::string prefix, bool isLast) const override;
    std::shared_ptr<SynNode> getCondition() const { return condition; }
    std::shared_ptr<Stmt> getThenBr() const { return thenBranch; }
    std::shared_ptr<Stmt> getElseBr() const { return elseBranch; }
};

class WhileStmt : public Stmt {
    std::shared_ptr<SynNode> condition;
    std::shared_ptr<Stmt> body;
public:
    WhileStmt(std::shared_ptr<SynNode> cond, std::shared_ptr<Stmt> b)
        : condition(std::move(cond)), body(std::move(b)) {}
    void print(std::string prefix, bool isLast) const override;
    std::shared_ptr<SynNode> getCondition() const { return condition; }
    std::shared_ptr<Stmt> getBody() const { return body; }
};

class StmtEmit : public Stmt {
    std::vector<std::shared_ptr<SynNode>> expressions;
public:
    StmtEmit(std::vector<std::shared_ptr<SynNode>> exprs) : expressions(std::move(exprs)) {}
    const std::vector<std::shared_ptr<SynNode>>& getExpressions() const { return expressions; }
    void print(std::string prefix, bool isLast) const override;
};

class ForStmt : public Stmt {
    private:
        std::shared_ptr<Stmt> init; // i = start
        std::shared_ptr<SynNode> condition; // i < 10
        std::shared_ptr<Stmt> update; // i = i+1
        std::shared_ptr<Stmt> body; // i = i+1
    public:
        ForStmt(std::shared_ptr<Stmt> in, 
                         std::shared_ptr<SynNode> cond, 
                         std::shared_ptr<Stmt> updt,
                         std::shared_ptr<Stmt> bdy)
        : init(std::move(in)), condition(std::move(cond)), update(std::move(updt)), body(std::move(bdy)) {}
        void print(std::string prefix, bool isLast) const override;
        std::shared_ptr<Stmt> getInit()      const { return init; }
    std::shared_ptr<SynNode>       getCondition() const { return condition; }
    std::shared_ptr<Stmt> getUpdate()    const { return update; }
    std::shared_ptr<Stmt> getBody()      const { return body; }
};

class LitText : public SynNode {
    private:
        std::string value;
    public:
        LitText(const std::string& val = "") : value(val) {}
        const std::string getValue() const {
            return value;
        }
        void print(std::string prefix, bool isLast) const override {
            std::cout << prefix << (isLast ? "└── " : "├── ") << "String: \"" << value << "\"" << std::endl;
        }
        std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};

// function definition
class StmtRoutine : public Stmt {
    private:
        std::string name;
        std::vector<std::string> params;
        std::shared_ptr<Stmt> body;
        int localSlotCount;
        bool isVoid;
    public:
        StmtRoutine(const std::string& n, std::vector<std::string> p,
                        std::shared_ptr<Stmt> b, int slots, bool v = false)
        : name(n), params(std::move(p)), body(std::move(b)), localSlotCount(slots), isVoid(v) {}
        const std::string& getName() const { return name; }
        const std::vector<std::string>& getParams() const { return params; }
        std::shared_ptr<Stmt> getBody() const { return body; }
        int getLocalSlotCount() const { return localSlotCount; }
        bool getIsVoid() const { return isVoid; }
        void print(std::string prefix, bool isLast) const override;
        std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};

// function call
class CallRoutine : public SynNode {
    private:
        std::string name;
        std::vector<std::shared_ptr<SynNode>> args;
    public:
        CallRoutine(const std::string& n, std::vector<std::shared_ptr<SynNode>> a)
        : name(n), args(std::move(a)) {}
        const std::string& getName() const { return name; }
        const std::vector<std::shared_ptr<SynNode>>& getArgs() const { return args; }
        void print(std::string prefix, bool isLast) const override;
        std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};

// return statement
class StmtGive : public Stmt {
    private:
        std::shared_ptr<SynNode> expression;
    public:
        StmtGive(std::shared_ptr<SynNode> expr) : expression(std::move(expr)) {}
        std::shared_ptr<SynNode> getExpression() const { return expression; }
        void print(std::string prefix, bool isLast) const override;
        std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};

class StmtCallExpr : public Stmt {
    std::shared_ptr<CallRoutine> call;
public:
    StmtCallExpr(std::shared_ptr<SynNode> c)
        : call(std::dynamic_pointer_cast<CallRoutine>(c)) {}
    std::shared_ptr<CallRoutine> getCall() const { return call; }
    void print(std::string prefix, bool isLast) const override {
        call->print(prefix, isLast);
    }
    std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};

// break Statement
class StmtHalt : public Stmt {
    public:
        void print(std::string prefix, bool isLast) const override {
            std::cout << prefix << (isLast ? "└── " : "├── ") << "Break" << std::endl;
        }
        std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};

// continue Statement
class StmtSkip : public Stmt {
public:
    void print(std::string prefix, bool isLast) const override {
        std::cout << prefix << (isLast ? "└── " : "├── ") << "Continue" << std::endl;
    }
    std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};

struct PickArm {
    std::vector<std::shared_ptr<SynNode>> values; // case 1,...
    std::shared_ptr<Stmt> body;
};

class StmtPick : public Stmt {
private:
    std::shared_ptr<SynNode> expression;
    std::vector<PickArm> cases;
    std::shared_ptr<Stmt> defaultBody;   // nullptr if not
public:
    StmtPick(std::shared_ptr<SynNode> expr,
               std::vector<PickArm> cs,
               std::shared_ptr<Stmt> def)
        : expression(std::move(expr)), cases(std::move(cs)), defaultBody(std::move(def)) {}

    std::shared_ptr<SynNode> getExpression() const { return expression; }
    const std::vector<PickArm>& getCases() const { return cases; }
    std::shared_ptr<Stmt> getDefaultBody() const { return defaultBody; }

    void print(std::string prefix, bool isLast) const override {
        std::cout << prefix << (isLast ? "└── " : "├── ") << "Switch" << std::endl;
        std::string p = prefix + (isLast ? "    " : "│   ");
        expression->print(p, false);
        for(size_t i = 0; i < cases.size(); ++i) {
            std::cout << p << "├── Case: ";
            for(size_t j = 0; j < cases[i].values.size(); ++j) {
                if(j) std::cout << ", ";
                cases[i].values[j]->print("", false);
            }
            std::cout << std::endl;
            cases[i].body->print(p + "│   ", (i == cases.size()-1 && !defaultBody));
        }
        if(defaultBody) {
            std::cout << p << "└── Default" << std::endl;
            defaultBody->print(p + "    ", true);
        }
    }

    std::vector<std::shared_ptr<SynNode>> getChildren() const override { return {}; }
};