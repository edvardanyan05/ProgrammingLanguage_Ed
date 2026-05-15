#include "syntax.hpp"

void LitNumber::print(std::string prefix, bool isLast) const {
    std::cout << prefix << (isLast ? "└── " : "├── ") << "Number: " << value << std::endl;
}

TapeOp OpBinary::getOpCode() const {
    if(op == "+")  return TapeOp::ADD;
    if(op == "-")  return TapeOp::SUB;
    if(op == "/")  return TapeOp::DIV;
    if(op == "//") return TapeOp::FLOOR_DIV;
    if(op == "%/") return TapeOp::FRAC_DIV;
    if(op == "*")  return TapeOp::MUL;
    if(op == "**") return TapeOp::POW;
    if(op == "&")  return TapeOp::AND;
    if(op == "|")  return TapeOp::OR;
    if(op == "^")  return TapeOp::XOR;
    if(op == "~")  return TapeOp::NOT;
    if(op == "%")  return TapeOp::MODULO;
    if(op == "<<") return TapeOp::SLL;
    if(op == ">>") return TapeOp::SRL;
    if(op == ">")  return TapeOp::CMP_GT;
    if(op == "<")  return TapeOp::CMP_LT;
    if(op == ">=") return TapeOp::CMP_GET;
    if(op == "<=") return TapeOp::CMP_LET;
    if(op == "==") return TapeOp::CMP_EQ;
    if(op == "!=") return TapeOp::CMP_NEQ;
    if(op == "and") return TapeOp::LOGICAL_AND;
    if(op == "or") return TapeOp::LOGICAL_OR;
    if(op == "not") return TapeOp::LOGICAL_NOT;
    return TapeOp::UNDEFINED;
}

void OpBinary::print(std::string prefix, bool isLast) const {
    std::cout << prefix << (isLast ? "└── " : "├── ") << "BinaryOp: " << op << std::endl;
    std::string newPrefix = prefix + (isLast ? "    " : "│   ");
    left->print(newPrefix, false);
    right->print(newPrefix, true);
}

void OpUnary::print(std::string prefix, bool isLast) const {
    std::string sign = (op == "-" || op == "_") ? "-" : "+";
    std::cout << prefix << (isLast ? "└── " : "├── ") << "UnaryOp: " << sign << std::endl;
    std::string newPrefix = prefix + (isLast ? "    " : "│   ");
    child->print(newPrefix, true);
}

void IfStmt::print(std::string indent, bool isLast) const {
    std::cout << indent << (isLast ? "└── " : "├── ") << "If" << std::endl;
    condition->print(indent + (isLast ? "    " : "│   "), false);
    thenBranch->print(indent + (isLast ? "    " : "│   "), elseBranch == nullptr);
    if(elseBranch) elseBranch->print(indent + (isLast ? "    " : "│   "), true);
}

void WhileStmt::print(std::string indent, bool isLast) const {
    std::cout << indent << (isLast ? "└── " : "├── ") << "While" << std::endl;
    condition->print(indent + (isLast ? "    " : "│   "), false);
    body->print(indent + (isLast ? "    " : "│   "), true);
}

void StmtBlock::print(std::string indent, bool isLast) const {
    std::cout << indent << (isLast ? "└── " : "├── ") << "Block" << std::endl;
    for(size_t i = 0; i < statements.size(); ++i)
        statements[i]->print(indent + (isLast ? "    " : "│   "), i == statements.size()-1);
}

void StmtEmit::print(std::string prefix, bool isLast) const {
    std::cout << prefix << (isLast ? "└── " : "├── ") << "Print" << std::endl;
    
    for(size_t i = 0; i < expressions.size(); ++i) {
        bool last = (i == expressions.size() - 1);
        expressions[i]->print(prefix + (isLast ? "    " : "│   "), last);
        if (!last) {
            std::cout << " ";
        }
    }
}

void ForStmt::print(std::string prefix, bool isLast) const {
    std::cout << prefix << (isLast ? "└── " : "├── ") << "For" << std::endl;
    std::string p = prefix + (isLast ? "    " : "│   ");
    init->print(p, false);
    condition->print(p, false);
    update->print(p, false);
    body->print(p, true);
}

void StmtRoutine::print(std::string prefix, bool isLast) const {
    std::cout << prefix << (isLast ? "└── " : "├── ")
              << "Function: " << name << "(";
    for(size_t i = 0; i < params.size(); ++i) {
        std::cout << params[i];
        if(i < params.size()-1) std::cout << ", ";
    }
    std::cout << ")" << std::endl;
    body->print(prefix + (isLast ? "    " : "│   "), true);
}

void CallRoutine::print(std::string prefix, bool isLast) const {
    std::cout << prefix << (isLast ? "└── " : "├── ")
              << "Call: " << name << std::endl;
    for(size_t i = 0; i < args.size(); ++i)
        args[i]->print(prefix + (isLast ? "    " : "│   "), i == args.size()-1);
}

void StmtGive::print(std::string prefix, bool isLast) const {
    std::cout << prefix << (isLast ? "└── " : "├── ") << "Return" << std::endl;
    if(expression)
        expression->print(prefix + (isLast ? "    " : "│   "), true);
}