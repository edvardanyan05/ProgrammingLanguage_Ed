#include "stack_emitter.hpp"
#include <cmath>
#include <fstream>
#include <cstring>

const int SP = 2;
const int FP = 8;

static void rebaseJumpTargets(std::vector<TapeInstr>& instructions, uint16_t baseOffset) {
    for(auto& inst : instructions) {
        if(inst.op == (uint32_t)TapeOp::JZ || inst.op == (uint32_t)TapeOp::JMP) {
            setAddress(inst, static_cast<uint16_t>(getAddress(inst) + baseOffset));
        }
    }
}

void Emitter::emitMainPrologue(std::vector<TapeInstr>& code) {
    int slots = symTable.getProgramFrameSlotCount();
    if (slots < 1) slots = 1;
    int frameSize = (slots + 4) * 4;
    code.push_back({(uint32_t)TapeOp::ADDI, SP, SP, (uint32_t)(int32_t)(-frameSize)});
    lineNumbers.push_back(0);
    code.push_back({(uint32_t)TapeOp::ADDI, FP, SP, (uint32_t)frameSize});
    lineNumbers.push_back(0);
}

int Emitter::allocateTempRegister() {
    if(!freeRegisters.empty()) {
        int reg = freeRegisters.top();
        freeRegisters.pop();
        return reg;
    }
    while(nextTempIndex == SP || nextTempIndex == FP) {
        ++nextTempIndex;
    }
    return nextTempIndex++;
}

void Emitter::freeTempRegister(int reg) {
    if(reg != SP && reg != FP) {
        freeRegisters.push(reg);
    }
}

bool Emitter::tryEmitMathBuiltinCall(
    const std::string& name,
    const std::vector<std::shared_ptr<SynNode>>& args,
    std::vector<TapeInstr>& code,
    int& resultReg
) {
    auto emitArg = [&](const std::shared_ptr<SynNode>& arg) -> int {
        globalCtx.consts.clear();
        globalCtx.vars.clear();
        auto argCode = generateTapeBundle(postOrderTraverse(arg));
        rebaseJumpTargets(argCode, static_cast<uint16_t>(code.size()));
        code.insert(code.end(), argCode.begin(), argCode.end());
        return argCode.empty() ? 0 : static_cast<int>(argCode.back().dst);
    };

    auto emitUnary = [&](TapeOp op) -> bool {
        if(args.size() != 1) return false;
        int argReg = emitArg(args[0]);
        resultReg = allocateTempRegister();
        code.push_back({(uint32_t)op, (uint32_t)resultReg, (uint32_t)argReg, 0});
        return true;
    };

    auto emitBinary = [&](TapeOp op) -> bool {
        if(args.size() != 2) return false;
        int leftReg = emitArg(args[0]);
        int rightReg = emitArg(args[1]);
        resultReg = allocateTempRegister();
        code.push_back({(uint32_t)op, (uint32_t)resultReg, (uint32_t)leftReg, (uint32_t)rightReg});
        return true;
    };

    if(name == "sin") return emitUnary(TapeOp::SIN);
    if(name == "cos") return emitUnary(TapeOp::COS);
    if(name == "tan") return emitUnary(TapeOp::TAN);
    if(name == "asin") return emitUnary(TapeOp::ASIN);
    if(name == "acos") return emitUnary(TapeOp::ACOS);
    if(name == "atan") return emitUnary(TapeOp::ATAN);
    if(name == "atan2") return emitBinary(TapeOp::ATAN2);
    if(name == "sqrt") return emitUnary(TapeOp::SQRT);
    if(name == "cbrt") return emitUnary(TapeOp::CBRT);
    if(name == "pow") return emitBinary(TapeOp::MATH_POW);
    if(name == "exp") return emitUnary(TapeOp::EXP);
    if(name == "log") return emitUnary(TapeOp::LOG);
    if(name == "ln") return emitUnary(TapeOp::LOG);
    if(name == "log10") return emitUnary(TapeOp::LOG10);
    if(name == "log2") return emitUnary(TapeOp::LOG2);
    if(name == "ceil") return emitUnary(TapeOp::CEIL);
    if(name == "floor") return emitUnary(TapeOp::FLOOR);
    if(name == "abs") return emitUnary(TapeOp::ABS);
    if(name == "round") return emitUnary(TapeOp::ROUND);
    if(name == "fmod") return emitBinary(TapeOp::FMOD);
    if(name == "log_ab") return emitBinary(TapeOp::LOG_AB);

    if(name == "input") {
        if(args.size() > 1) return false; // input can have 0 or 1 argument
        resultReg = allocateTempRegister();

        // for prompt
        if(args.size() == 1) {
            int promptReg = emitArg(args[0]);
            code.push_back({(uint32_t)TapeOp::PRINT, (uint32_t)promptReg, 0, 0});
        }
        code.push_back({(uint32_t)TapeOp::INPUT, (uint32_t)resultReg, 0, 0});
        return true;
    }

    if(name == "type") {
        if(args.size() != 1) return false;

        int argReg = emitArg(args[0]);
        resultReg = allocateTempRegister();
        code.push_back({(uint32_t)TapeOp::TYPE, (uint32_t)resultReg, (uint32_t)argReg, 0});
        return true;
    }

    if(name == "ord") {
        if(args.size() != 1) return false;
        int argReg = emitArg(args[0]);
        resultReg = allocateTempRegister();
        code.push_back({(uint32_t)TapeOp::ORD, (uint32_t)resultReg, (uint32_t)argReg, 0});
        return true;
    }

    if(name == "chr") {
        if(args.size() != 1) return false;
        int argReg = emitArg(args[0]);
        resultReg = allocateTempRegister();
        code.push_back({(uint32_t)TapeOp::CHR, (uint32_t)resultReg, (uint32_t)argReg, 0});
        return true;
    }

    if(name == "bin") {
        if(args.size() != 1) return false;
        int argReg = emitArg(args[0]);
        resultReg = allocateTempRegister();
        code.push_back({(uint32_t)TapeOp::BIN, (uint32_t)resultReg, (uint32_t)argReg, 0});
        return true;
    }

    if(name == "hex") {
        if(args.size() != 1) return false;
        int argReg = emitArg(args[0]);
        resultReg = allocateTempRegister();
        code.push_back({(uint32_t)TapeOp::HEX, (uint32_t)resultReg, (uint32_t)argReg, 0});
        return true;
    }
    if(name == "oct") {
        if(args.size() != 1) return false;
        int argReg = emitArg(args[0]);
        resultReg = allocateTempRegister();
        code.push_back({(uint32_t)TapeOp::OCT, (uint32_t)resultReg, (uint32_t)argReg, 0});
        return true;
    }
    if(name == "dec") {
        if(args.size() != 1) return false;
        int argReg = emitArg(args[0]);
        resultReg = allocateTempRegister();
        code.push_back({(uint32_t)TapeOp::DEC, (uint32_t)resultReg, (uint32_t)argReg, 0});
        return true;
    }

    return false;
}

std::vector<std::shared_ptr<SynNode>> Emitter::postOrderTraverse(std::shared_ptr<SynNode> root) {
    if(!root) return {};
    std::vector<std::shared_ptr<SynNode>> postOrder;
    std::stack<std::shared_ptr<SynNode>> s1, s2;
    s1.push(root);
    while(!s1.empty()) {
        auto node = s1.top(); s1.pop(); s2.push(node);
        if(std::dynamic_pointer_cast<OpPick>(node)) {
            continue;
        }
        for(auto& child : node->getChildren()) s1.push(child);
    }
    while(!s2.empty()) { postOrder.push_back(s2.top()); s2.pop(); }
    return postOrder;
}

std::shared_ptr<SynNode> Emitter::optimize(std::shared_ptr<SynNode> node) {
    if(!node) return nullptr;
    if(auto bin = std::dynamic_pointer_cast<OpBinary>(node)) {
        auto left  = optimize(bin->getLeft());
        auto right = optimize(bin->getRight());
        auto lNum  = std::dynamic_pointer_cast<LitNumber>(left);
        auto rNum  = std::dynamic_pointer_cast<LitNumber>(right);
        if(lNum && rNum) {
            double v1 = lNum->getValue(), v2 = rNum->getValue(), result = 0;
            switch(bin->getOpCode()) {
                case TapeOp::ADD: result = v1+v2; break;
                case TapeOp::SUB: result = v1-v2; break;
                case TapeOp::MUL: result = v1*v2; break;
                case TapeOp::POW: result = std::pow(v1, v2); break;
                case TapeOp::DIV:
                    if (v2 == 0) return std::make_shared<OpBinary>(bin->getOp(), left, right);
                    result = v1/v2;
                    break;
                case TapeOp::FLOOR_DIV:
                    if (v2 == 0) return std::make_shared<OpBinary>(bin->getOp(), left, right);
                    result = std::floor(v1/v2);
                    break;
                case TapeOp::FRAC_DIV:
                    if (v2 == 0) return std::make_shared<OpBinary>(bin->getOp(), left, right);
                    result = (v1 / v2 - std::floor(v1/v2));
                    break;
                case TapeOp::AND: result = (double)((long long)v1 & (long long)v2); break;
                case TapeOp::OR:  result = (double)((long long)v1 | (long long)v2); break;
                case TapeOp::XOR: result = (double)((long long)v1 ^ (long long)v2); break;
                case TapeOp::MODULO:
                    if ((long long)v2 == 0) return std::make_shared<OpBinary>(bin->getOp(), left, right);
                    result = (double)((long long)v1 % (long long)v2);
                    break;
                case TapeOp::SLL: result = (double)((long long)v1 << (long long)v2); break;
                case TapeOp::SRL: result = (double)((uint32_t)((long long)v1) >> (((long long)v2) & 0x1F)); break;
                case TapeOp::LOGICAL_AND: result = (v1 != 0 && v2 != 0) ? 1.0 : 0.0; break;
                case TapeOp::LOGICAL_OR: result = (v1 != 0 || v2 != 0) ? 1.0 : 0.0; break;
                case TapeOp::SLT:
                case TapeOp::CMP_LT:
                    result = (v1 < v2) ? 1.0 : 0.0;
                    break;
                case TapeOp::CMP_LET: result = (v1 <= v2) ? 1.0 : 0.0; break;
                case TapeOp::CMP_GT: result = (v1 > v2) ? 1.0 : 0.0; break;
                case TapeOp::CMP_GET: result = (v1 >= v2) ? 1.0 : 0.0; break;
                case TapeOp::CMP_EQ: result = (v1 == v2) ? 1.0 : 0.0; break;
                case TapeOp::CMP_NEQ: result = (v1 != v2) ? 1.0 : 0.0; break;
                case TapeOp::CONST_E: result = 2.718281828459045; break;
                case TapeOp::CONST_PI: result = 3.14159265358979323846; break;
                default: break;
            }
            return std::make_shared<LitNumber>(result);
        }
        return std::make_shared<OpBinary>(bin->getOp(), left, right);
    }
    if(auto un = std::dynamic_pointer_cast<OpUnary>(node)) {
        auto child = optimize(un -> getChild());
        auto num = std::dynamic_pointer_cast<LitNumber>(child);
        if(num) {
            if(un -> getOp() == "-" || un -> getOp() == "_") {
                return std::make_shared<LitNumber>(-num -> getValue());
            }
            if(un -> getOp() == "+" || un -> getOp() == "#") {
                return num;
            }
            if(un -> getOp() == "not") {
                return std::make_shared<LitNumber>(num -> getValue() == 0 ? 1.0 : 0.0);
            }
            if(un -> getOp() == "~") {
                long long val = static_cast<long long>(num -> getValue());
                return std::make_shared<LitNumber>(static_cast<double>(~val));
            }
        }
        return std::make_shared<OpUnary>(un -> getOp(), child);
    }
    if(auto block = std::dynamic_pointer_cast<StmtBlock>(node)) {
        auto optimizedBlock = std::make_shared<StmtBlock>();
        optimizedBlock->lineNumber = block->lineNumber;
        for(auto& s : block->getStatements()) {
            auto optStmt = std::dynamic_pointer_cast<Stmt>(optimize(s));
            if(optStmt) optimizedBlock->addStatement(optStmt);
        }
        return optimizedBlock;
    }
    if(auto assign = std::dynamic_pointer_cast<StmtAssign>(node)) {
        std::shared_ptr<StmtAssign> n;
        if (assign->isLocal()) {
            n = std::make_shared<StmtAssign>(assign->getOffset(), optimize(assign->getValue()));
        } else {
            n = std::make_shared<StmtAssign>(assign->getAddress(), optimize(assign->getValue()));
        }
        n->lineNumber = assign->lineNumber;
        return n;
    }
    if(auto ifStmt = std::dynamic_pointer_cast<IfStmt>(node)) {
        auto cond   = optimize(ifStmt->getCondition());
        auto thenBr = std::dynamic_pointer_cast<Stmt>(optimize(ifStmt->getThenBr()));
        auto elseBr = std::dynamic_pointer_cast<Stmt>(optimize(ifStmt->getElseBr()));
        if(auto condNum = std::dynamic_pointer_cast<LitNumber>(cond))
            return condNum->getValue() != 0 ? thenBr : (elseBr ? elseBr : nullptr);
        auto n = std::make_shared<IfStmt>(cond, thenBr, elseBr);
        n->lineNumber = ifStmt->lineNumber;
        return n;
    }
    if(auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(node)) {
        auto cond = optimize(whileStmt->getCondition());
        auto body = std::dynamic_pointer_cast<Stmt>(optimize(whileStmt->getBody()));
        if(auto condNum = std::dynamic_pointer_cast<LitNumber>(cond))
            if(condNum->getValue() == 0) return nullptr;
        auto n = std::make_shared<WhileStmt>(cond, body);
        n->lineNumber = whileStmt->lineNumber;
        return n;
    }
    if(auto printStmt = std::dynamic_pointer_cast<StmtEmit>(node)) {
        std::vector<std::shared_ptr<SynNode>> exprs;
        for(const auto& e : printStmt->getExpressions()) exprs.push_back(optimize(e));
        auto n = std::make_shared<StmtEmit>(std::move(exprs));
        n->lineNumber = printStmt->lineNumber;
        return n;
    }
    if(auto forStmt = std::dynamic_pointer_cast<ForStmt>(node)) {
        auto init = std::dynamic_pointer_cast<Stmt>(optimize(forStmt -> getInit()));
        auto cond = optimize(forStmt -> getCondition());
        auto update = std::dynamic_pointer_cast<Stmt>(optimize(forStmt -> getUpdate()));
        auto body = std::dynamic_pointer_cast<Stmt>(optimize(forStmt -> getBody()));
        auto n = std::make_shared<ForStmt>(init, cond, update, body);
        n->lineNumber = forStmt->lineNumber;
        return n;
    }
    if(auto strNode = std::dynamic_pointer_cast<LitText>(node)) {
        return strNode;
    }
    return node;
}

TapeBundle Emitter::compile(
    std::shared_ptr<SynNode> root,
    bool allowUnresolvedCalls,
    bool emitMainFramePrologue
) {
    constantPool.clear();
    lineNumbers.clear();
    stringPool.clear();
    nextTempIndex = 0;
    functionTable.clear();
    forwardCalls.clear();
    
    std::vector<TapeInstr> insts;
    if(!root) {
        TapeBundle empty;
        empty.instructions = insts;
        empty.constants = constantPool;
        empty.strings = stringPool;
        return empty;
    }
    
    auto optimizedRoot = optimize(root);
    if(emitMainFramePrologue) {
        emitMainPrologue(insts);
    }

    if (auto block = std::dynamic_pointer_cast<StmtBlock>(optimizedRoot)) {
        for (auto& s : block->getStatements()) {
            compileStatement(s, insts);
        }
    } else if (auto stmt = std::dynamic_pointer_cast<Stmt>(optimizedRoot)) {
        compileStatement(stmt, insts);
    }

    std::vector<std::pair<size_t, std::string>> unresolvedCalls;
    for(auto& [idx, name] : forwardCalls) {
        if(functionTable.count(name)) {
            setAddress(insts[idx], (uint16_t)functionTable[name].address);
        } else if(!allowUnresolvedCalls) {
            throw std::runtime_error("Undefined function: " + name);
        } else {
            unresolvedCalls.push_back({idx, name});
        }
    }

    TapeBundle bc;
    while(lineNumbers.size() < insts.size()) {
        int fallBack = lineNumbers.empty() ? 0 : lineNumbers.back();
        lineNumbers.push_back(fallBack);
    }

    if(lineNumbers.size() > insts.size()) {
        lineNumbers.reserve(insts.size());
    }

    bc.instructions = std::move(insts);
    bc.constants = constantPool;
    bc.strings = stringPool;
    bc.lineNumbers = std::move(lineNumbers);
    for(const auto& [name, info] : functionTable) {
        bc.functionSymbols[name] = info.address;
    }
    bc.unresolvedCalls = std::move(unresolvedCalls);
    return bc;
}

std::vector<TapeInstr> Emitter::generateTapeBundle(const std::vector<std::shared_ptr<SynNode>>& nodes) {
    std::vector<TapeInstr> code;
    std::stack<int> storage;
    
    for(const auto& node : nodes) {
        if(auto num = std::dynamic_pointer_cast<LitNumber>(node)) {
            double val = num -> getValue();
            int reg = allocateTempRegister();
            int idx = (int)constantPool.size();
            constantPool.push_back(val);
            code.push_back({(uint32_t)TapeOp::LOAD_CONST, (uint32_t)reg, (uint32_t)idx, 0});
            storage.push(reg);
        }
        else if(auto var = std::dynamic_pointer_cast<RefSlot>(node)) {
            int rd = allocateTempRegister();
            if (var->getIsLocal()) {
                int32_t off = var->getLocalOffset();
                code.push_back({(uint32_t)TapeOp::LOAD, (uint32_t)rd, (uint32_t)FP, (uint32_t)off});
            } else {
                size_t addr = var->getGlobalAddr();
                code.push_back({(uint32_t)TapeOp::LOAD_VAR, (uint32_t)rd, (uint32_t)addr, 0});
            }
            storage.push(rd);
        }
        else if(auto bin = std::dynamic_pointer_cast<OpBinary>(node)) {
            int r = storage.top(); storage.pop();
            int l = storage.top(); storage.pop();
            int target = allocateTempRegister();
            code.push_back({(uint32_t)bin->getOpCode(), (uint32_t)target, (uint32_t)l, (uint32_t)r});
            freeTempRegister(l);
            freeTempRegister(r);
            storage.push(target);
        }
        else if(auto un = std::dynamic_pointer_cast<OpUnary>(node)) {
            int childIdx = storage.top(); storage.pop();
            int target = allocateTempRegister();
            TapeOp opcode;
            if(un -> getOp() == "not") {
                opcode = TapeOp::LOGICAL_NOT;
            } else if(un -> getOp() == "~") {
                opcode = TapeOp::NOT;
            } else {
                opcode = TapeOp::UNARY;
            }
            code.push_back({(uint32_t)opcode, (uint32_t)target, (uint32_t)childIdx, 0});
            freeTempRegister(childIdx);
            storage.push(target);
        }
        else if(auto strNode = std::dynamic_pointer_cast<LitText>(node)) {
            int strIdx = (int)stringPool.size();
            stringPool.push_back(strNode->getValue());
            int reg = allocateTempRegister();
            code.push_back({(uint32_t)TapeOp::LOAD_STR, (uint32_t)reg, (uint32_t)strIdx, 0});
            storage.push(reg);
        }
        else if(auto callExpr = std::dynamic_pointer_cast<CallRoutine>(node)) {
            int builtinResultReg = 0;
            if(tryEmitMathBuiltinCall(callExpr->getName(), callExpr->getArgs(), code, builtinResultReg)) {
                storage.push(builtinResultReg);
                continue;
            }

            for(const auto& arg : callExpr->getArgs()) {
                globalCtx.consts.clear();
                globalCtx.vars.clear();
                auto argCode = generateTapeBundle(postOrderTraverse(arg));
                rebaseJumpTargets(argCode, static_cast<uint16_t>(code.size()));
                code.insert(code.end(), argCode.begin(), argCode.end());
                int argReg = argCode.empty() ? 0 : argCode.back().dst;
                code.push_back({(uint32_t)TapeOp::PUSH_ARG, (uint32_t)argReg, 0, 0});
                freeTempRegister(argReg);
            }
        
            int resultReg = allocateTempRegister();
            TapeInstr callInst;
            callInst.op  = (uint32_t)TapeOp::CALL;
            callInst.dst = (uint32_t)resultReg;
            if(functionTable.count(callExpr->getName())) {
                setAddress(callInst, (uint16_t)functionTable[callExpr->getName()].address);
            } else {
                setAddress(callInst, 0);
                forwardCalls.push_back({code.size(), callExpr->getName()});
            }
            code.push_back(callInst);
            storage.push(resultReg);
        }
        else if(auto lengthNode = std::dynamic_pointer_cast<BuiltinLen>(node)) {
            int argReg = storage.top(); storage.pop();
            int resultReg = allocateTempRegister();
            code.push_back({(uint32_t)TapeOp::LENGTH, (uint32_t)resultReg, (uint32_t)argReg, 0});
            freeTempRegister(argReg);
            storage.push(resultReg);
        }
        else if(auto mathConst = std::dynamic_pointer_cast<LitMathConst>(node)) {
            int reg = allocateTempRegister();
            code.push_back({(uint32_t)mathConst->getConstant(), (uint32_t)reg, 0, 0});
            storage.push(reg);
        } else if(auto ternary = std::dynamic_pointer_cast<OpPick>(node)) {
            int resultReg = allocateTempRegister();
                
            // Generate condition
            auto condCode = generateTapeBundle(postOrderTraverse(ternary->getCondition()));
            rebaseJumpTargets(condCode, static_cast<uint16_t>(code.size()));
            code.insert(code.end(), condCode.begin(), condCode.end());
            int condReg = condCode.empty() ? 0 : condCode.back().dst;
                
            // Generate true branch and store to resultReg
            size_t jzIdx = code.size();
            code.push_back({(uint32_t)TapeOp::JZ, (uint32_t)condReg, 0, 0});
                
            auto trueCode = generateTapeBundle(postOrderTraverse(ternary->getTrueExpr()));
            rebaseJumpTargets(trueCode, static_cast<uint16_t>(code.size()));
            code.insert(code.end(), trueCode.begin(), trueCode.end());
            int trueReg = trueCode.empty() ? 0 : trueCode.back().dst;
            code.push_back({(uint32_t)TapeOp::MOV, (uint32_t)resultReg, (uint32_t)trueReg, 0});
            freeTempRegister(trueReg);
                
            size_t jmpIdx = code.size();
            code.push_back({(uint32_t)TapeOp::JMP, 0, 0, 0});
            setAddress(code[jzIdx], (uint16_t)code.size());
                
            // Generate false branch and store to resultReg
            auto falseCode = generateTapeBundle(postOrderTraverse(ternary->getFalseExpr()));
            rebaseJumpTargets(falseCode, static_cast<uint16_t>(code.size()));
            code.insert(code.end(), falseCode.begin(), falseCode.end());
            int falseReg = falseCode.empty() ? 0 : falseCode.back().dst;
            code.push_back({(uint32_t)TapeOp::MOV, (uint32_t)resultReg, (uint32_t)falseReg, 0});
            freeTempRegister(falseReg);
                
            setAddress(code[jmpIdx], (uint16_t)code.size());
            freeTempRegister(condReg);
                
            storage.push(resultReg);
        } else if(auto noneNode = std::dynamic_pointer_cast<LitEmpty>(node)) {
            int reg = allocateTempRegister();
            code.push_back({(uint32_t)TapeOp::LOAD_NONE, (uint32_t)reg, 0, 0});
            storage.push(reg);
        }
    }
    return code;
}

void Emitter::compileStatement(std::shared_ptr<Stmt> stmt, std::vector<TapeInstr>& code) {
    if(!stmt) return;
    
    if (auto assign = std::dynamic_pointer_cast<StmtAssign>(stmt)) {
        globalCtx.consts.clear();
        globalCtx.vars.clear();
        auto exprCode = generateTapeBundle(postOrderTraverse(assign->getValue()));
        rebaseJumpTargets(exprCode, static_cast<uint16_t>(code.size()));
        code.insert(code.end(), exprCode.begin(), exprCode.end());
        addLineNumbers(stmt->lineNumber, exprCode.size());

        int srcReg = exprCode.empty() ? 0 : exprCode.back().dst;
        
        if (assign->isLocal()) {
            int32_t offset = assign->getOffset();
            code.push_back({(uint32_t)TapeOp::STORE, 
                            (uint32_t)srcReg,
                            (uint32_t)FP,
                            (uint32_t)offset});
            lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        } 
        else {
            size_t addr = assign->getAddress();
            code.push_back({(uint32_t)TapeOp::STORE_VAR, 
                            0,
                            (uint32_t)addr,
                            (uint32_t)srcReg});
            lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        }
        freeTempRegister(srcReg);
    }
    
    else if(auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt)) {
        globalCtx.consts.clear(); globalCtx.vars.clear();
        auto condCode = generateTapeBundle(postOrderTraverse(ifStmt->getCondition()));
        rebaseJumpTargets(condCode, static_cast<uint16_t>(code.size()));
        code.insert(code.end(), condCode.begin(), condCode.end());
        addLineNumbers(stmt->lineNumber, condCode.size());
        int condReg = condCode.empty() ? 0 : condCode.back().dst;
        size_t jzIdx = code.size();
        code.push_back({(uint32_t)TapeOp::JZ, (uint32_t)condReg, 0, 0});
        lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        freeTempRegister(condReg);
        
        compileStatement(ifStmt->getThenBr(), code);
        size_t jmpIdx = code.size();
        code.push_back({(uint32_t)TapeOp::JMP, 0, 0, 0});
        lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        setAddress(code[jzIdx], (uint16_t)code.size());
        if(ifStmt->getElseBr()) compileStatement(ifStmt->getElseBr(), code);
        setAddress(code[jmpIdx], (uint16_t)code.size());
    }
    else if(auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt)) {
        // Push new break/continue lists for this loop
        breakStack.push({});
        continueStack.push({});
        
        size_t startAddr = code.size();
        globalCtx.consts.clear(); globalCtx.vars.clear();
        auto condCode = generateTapeBundle(postOrderTraverse(whileStmt->getCondition()));
        rebaseJumpTargets(condCode, static_cast<uint16_t>(code.size()));
        code.insert(code.end(), condCode.begin(), condCode.end());
        addLineNumbers(stmt -> lineNumber, condCode.size());
        int condReg = condCode.empty() ? 0 : condCode.back().dst;
        size_t jzIdx = code.size();
        code.push_back({(uint32_t)TapeOp::JZ, (uint32_t)condReg, 0, 0});
        lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        freeTempRegister(condReg);
        
        compileStatement(whileStmt->getBody(), code);
        
        // Jump back to condition
        TapeInstr jmpBack = {(uint32_t)TapeOp::JMP, 0, 0, 0};
        setAddress(jmpBack, (uint16_t)startAddr);
        code.push_back(jmpBack);
        lineNumbers.push_back(stmt -> lineNumber);
        size_t afterLoopAddr = code.size();
        
        // False branch: JZ jumps to after the loop body
        setAddress(code[jzIdx], (uint16_t)afterLoopAddr);
        
        // Patch all break jumps to point after the loop
        while (!breakStack.top().empty()) {
            size_t breakIdx = breakStack.top().back();
            breakStack.top().pop_back();
            setAddress(code[breakIdx], (uint16_t)afterLoopAddr);
        }
        
        // Patch all continue jumps to point to condition check (startAddr)
        while (!continueStack.top().empty()) {
            size_t continueIdx = continueStack.top().back();
            continueStack.top().pop_back();
            setAddress(code[continueIdx], (uint16_t)startAddr);
        }
        
        breakStack.pop();
        continueStack.pop();
    }
    else if(auto block = std::dynamic_pointer_cast<StmtBlock>(stmt)) {
        for(auto& s : block->getStatements()) compileStatement(s, code);
    }
    else if(auto printStmt = std::dynamic_pointer_cast<StmtEmit>(stmt)) {
        for(const auto& expr : printStmt->getExpressions()) {
            if(auto strNode = std::dynamic_pointer_cast<LitText>(expr)) {
                int strIdx = (int)stringPool.size();
                stringPool.push_back(strNode->getValue());
                code.push_back({(uint32_t)TapeOp::PRINT_STR, (uint32_t)strIdx, 0, 0});
                lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
            } else {
                globalCtx.consts.clear(); globalCtx.vars.clear();
                auto exprCode = generateTapeBundle(postOrderTraverse(expr));
                rebaseJumpTargets(exprCode, static_cast<uint16_t>(code.size()));
                code.insert(code.end(), exprCode.begin(), exprCode.end());
                for(size_t i = 0; i < exprCode.size(); ++i) {
                    lineNumbers.push_back(stmt -> lineNumber);
                }
                int lastReg = exprCode.empty() ? 0 : exprCode.back().dst;
                code.push_back({(uint32_t)TapeOp::PRINT, (uint32_t)lastReg, 0, 0});
                lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
                // PRINT-ից հետո ազատել
                freeTempRegister(lastReg);
            }
        }
    }
    else if(auto forStmt = std::dynamic_pointer_cast<ForStmt>(stmt)) {
        // Push new break/continue lists for this loop
        breakStack.push({});
        continueStack.push({});
        
        compileStatement(forStmt->getInit(), code);
        size_t startAddr = code.size();
        globalCtx.consts.clear(); globalCtx.vars.clear();

        auto condCode = generateTapeBundle(postOrderTraverse(forStmt->getCondition()));
        rebaseJumpTargets(condCode, static_cast<uint16_t>(code.size()));
        code.insert(code.end(), condCode.begin(), condCode.end());
        addLineNumbers(stmt -> lineNumber, condCode.size());
        int condReg = condCode.empty() ? 0 : condCode.back().dst;
        size_t jzIdx = code.size();
        code.push_back({(uint32_t)TapeOp::JZ, (uint32_t)condReg, 0, 0});
        lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        freeTempRegister(condReg);

        compileStatement(forStmt->getBody(), code);
        
        size_t updateAddr = code.size();
        
        // Patch all continue jumps to point to update statement
        while (!continueStack.top().empty()) {
            size_t continueIdx = continueStack.top().back();
            continueStack.top().pop_back();
            setAddress(code[continueIdx], (uint16_t)updateAddr);
        }

        compileStatement(forStmt->getUpdate(), code);

        // Jump back to condition
        TapeInstr jmpFor = {(uint32_t)TapeOp::JMP, 0, 0, 0};
        setAddress(jmpFor, (uint16_t)startAddr);
        code.push_back(jmpFor);
        lineNumbers.push_back(stmt -> lineNumber);
        
        size_t afterLoopAddr = code.size();
        
        setAddress(code[jzIdx], (uint16_t)afterLoopAddr);
        
        // Patch all break jumps to point after the loop
        while (!breakStack.top().empty()) {
            size_t breakIdx = breakStack.top().back();
            breakStack.top().pop_back();
            setAddress(code[breakIdx], (uint16_t)afterLoopAddr);
        }
        
        breakStack.pop();
        continueStack.pop();
    }   
    else if (auto funcDef = std::dynamic_pointer_cast<StmtRoutine>(stmt)) {
        size_t jmpIdx = code.size();
        code.push_back({(uint32_t)TapeOp::JMP, 0, 0, 0});
        lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        size_t funcAddr = code.size();
        functionTable[funcDef->getName()] = {funcAddr, (int)funcDef->getParams().size()};

        int slots = funcDef->getLocalSlotCount();
        if (slots < 1) slots = 1;
        int frameSize = (slots + 4) * 4;
        code.push_back({(uint32_t)TapeOp::ADDI, SP, SP, (uint32_t)(int32_t)(-frameSize)});
        lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        code.push_back({(uint32_t)TapeOp::ADDI, FP, SP, (uint32_t)frameSize});
        lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);

        for (int i = 0; i < (int)funcDef->getParams().size(); i++) {
            int32_t off = -4 * (i + 1);
            int reg = allocateTempRegister();
            code.push_back({(uint32_t)TapeOp::LOAD_PARAM, (uint32_t)reg, (uint32_t)i, 0});
            lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
            code.push_back({(uint32_t)TapeOp::STORE, (uint32_t)reg, (uint32_t)FP, (uint32_t)off});
            lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
            freeTempRegister(reg);
        }

        compileStatement(funcDef->getBody(), code);
        if (funcDef->getIsVoid()) {
            code.push_back({(uint32_t)TapeOp::RETURN, 0, 0, 0});
            lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        }
        setAddress(code[jmpIdx], (uint16_t)code.size());
    } 
    else if(auto callStmt = std::dynamic_pointer_cast<StmtCallExpr>(stmt)) {
        auto call = callStmt->getCall();
        int builtinResultReg = 0;
        size_t instCountBefore = code.size();
        if(tryEmitMathBuiltinCall(call->getName(), call->getArgs(), code, builtinResultReg)) {
            addLineNumbers(callStmt->lineNumber, code.size() - instCountBefore);
            freeTempRegister(builtinResultReg);
            return;
        }
        for(const auto& arg : call->getArgs()) {
            globalCtx.consts.clear();
            globalCtx.vars.clear();
            auto exprCode = generateTapeBundle(postOrderTraverse(arg));
            rebaseJumpTargets(exprCode, static_cast<uint16_t>(code.size()));
            code.insert(code.end(), exprCode.begin(), exprCode.end());
            addLineNumbers(stmt->lineNumber, exprCode.size());
            int argReg = exprCode.empty() ? 0 : exprCode.back().dst;
            code.push_back({(uint32_t)TapeOp::PUSH_ARG, (uint32_t)argReg, 0, 0});
            lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
            freeTempRegister(argReg);
        }
        int resultReg = allocateTempRegister();
        TapeInstr callInst;
        callInst.op  = (uint32_t)TapeOp::CALL;
        callInst.dst = (uint32_t)resultReg;
        if(functionTable.count(call->getName())) {
            setAddress(callInst, (uint16_t)functionTable[call->getName()].address);
        } else {
            setAddress(callInst, 0);
            forwardCalls.push_back({code.size(), call->getName()});
        }
        code.push_back(callInst);
        lineNumbers.push_back(stmt -> lineNumber);
        
        freeTempRegister(resultReg);
    }
    else if(auto retStmt = std::dynamic_pointer_cast<StmtGive>(stmt)) {
        if(retStmt->getExpression()) {
            globalCtx.consts.clear();
            globalCtx.vars.clear();
            auto exprCode = generateTapeBundle(postOrderTraverse(retStmt->getExpression()));
            rebaseJumpTargets(exprCode, static_cast<uint16_t>(code.size()));
            code.insert(code.end(), exprCode.begin(), exprCode.end());
            addLineNumbers(stmt -> lineNumber, exprCode.size());
            int lastReg = exprCode.empty() ? 0 : exprCode.back().dst;
            code.push_back({(uint32_t)TapeOp::RETURN, (uint32_t)lastReg, 0, 0});
            lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        } else {
            code.push_back({(uint32_t)TapeOp::RETURN, 0, 0, 0});
            lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        }
    } else if(auto breakStmt = std::dynamic_pointer_cast<StmtHalt>(stmt)) {
        if(breakStack.empty()) {
            throw std::runtime_error("halt outside span, during, or pick");
        }
        size_t jmpIdx = code.size();
        code.push_back({(uint32_t)TapeOp::JMP, 0, 0, 0});
        lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        breakStack.top().push_back(jmpIdx);
    } else if(auto continueStmt = std::dynamic_pointer_cast<StmtSkip>(stmt)) {
        if(continueStack.empty()) {
            throw std::runtime_error("skip outside span or during loop");
        }
        size_t jmpIdx = code.size();
        code.push_back({(uint32_t)TapeOp::JMP, 0, 0, 0});
        lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
        continueStack.top().push_back(jmpIdx);
    } else if(auto switchNode = std::dynamic_pointer_cast<StmtPick>(stmt)) {
        breakStack.push({});  // for breaks
        
        // 1. Switch expression
        globalCtx.consts.clear(); globalCtx.vars.clear();
        auto exprCode = generateTapeBundle(postOrderTraverse(switchNode->getExpression()));
        rebaseJumpTargets(exprCode, static_cast<uint16_t>(code.size()));
        code.insert(code.end(), exprCode.begin(), exprCode.end());
        addLineNumbers(stmt -> lineNumber, exprCode.size());
        int switchValReg = exprCode.empty() ? 0 : exprCode.back().dst;
        
        const auto& cases = switchNode->getCases();
        size_t numCases = cases.size();
        bool hasDefault = (switchNode->getDefaultBody() != nullptr);
        
        std::vector<size_t> caseCheckStart(numCases);
        
        std::vector<std::vector<size_t>> caseJumpTargets(numCases);
        
        std::vector<size_t> nextCheckJumps;
        
        // 2. Checking all cases' values
        for(size_t i = 0; i < numCases; ++i) {
            caseCheckStart[i] = code.size();
        
            const auto& caseItem = cases[i];
            for(const auto& valueExpr : caseItem.values) {
                auto valCode = generateTapeBundle(postOrderTraverse(valueExpr));
                rebaseJumpTargets(valCode, static_cast<uint16_t>(code.size()));
                code.insert(code.end(), valCode.begin(), valCode.end());
                addLineNumbers(stmt -> lineNumber, valCode.size());
                int valReg = valCode.empty() ? 0 : valCode.back().dst;
            
                // compare switchValReg == valReg
                int cmpReg = allocateTempRegister();
                code.push_back({(uint32_t)TapeOp::CMP_EQ, (uint32_t)cmpReg, (uint32_t)switchValReg, (uint32_t)valReg});
                lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
            
                // (JZ false -> jump if zero)
                size_t jzIdx = code.size();
                code.push_back({(uint32_t)TapeOp::JZ, (uint32_t)cmpReg, 0, 0});
                lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
                // if true jump to case-body
                size_t jmpToBodyIdx = code.size();
                code.push_back({(uint32_t)TapeOp::JMP, 0, 0, 0});
                lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
            
                setAddress(code[jzIdx], (uint16_t)code.size());
            
                caseJumpTargets[i].push_back(jmpToBodyIdx);
            
                freeTempRegister(valReg);
                freeTempRegister(cmpReg);
            }
        
            size_t jmpNextIdx = code.size();
            code.push_back({(uint32_t)TapeOp::JMP, 0, 0, 0});
            lineNumbers.push_back(stmt ? stmt -> lineNumber : 0);
            nextCheckJumps.push_back(jmpNextIdx);
        }
    
        // 3. Cases' bodies
        std::vector<size_t> bodyAddrs(numCases);
        for(size_t i = 0; i < numCases; ++i) {
            bodyAddrs[i] = code.size();
            
            for(size_t jmpIdx : caseJumpTargets[i]) {
                setAddress(code[jmpIdx], (uint16_t)bodyAddrs[i]);
            }
            compileStatement(cases[i].body, code);
        }
    
        // 4. default body (if there's)
        size_t defaultAddr = code.size();
        if(hasDefault) {
            compileStatement(switchNode->getDefaultBody(), code);
        }
        size_t endAddr = code.size();
    
        // 5. Patch nextCheckJumps
        for(size_t i = 0; i < numCases; ++i) {
            size_t target;
            if(i + 1 < numCases) {
                target = caseCheckStart[i+1];
            } else {
                target = hasDefault ? defaultAddr : endAddr;
            }
            setAddress(code[nextCheckJumps[i]], (uint16_t)target);
        }
    
        // 6. Patch breaks at the end of switch
        while(!breakStack.top().empty()) {
            size_t brkIdx = breakStack.top().back();
            breakStack.top().pop_back();
            setAddress(code[brkIdx], (uint16_t)endAddr);
        }
        breakStack.pop();
    
        freeTempRegister(switchValReg);
    } 
}

void Emitter::printTapeBundle(const std::vector<TapeInstr>& code) const {
    for(const auto& inst : code)
        std::cout << "Op: " << (int)inst.op << " | L: " << inst.left
                  << " | R: " << inst.right << " | Dst: " << inst.dst << std::endl;
}

void writeTapeBundleToFile(const TapeBundle& bc, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if(!out.is_open()) {
        throw std::runtime_error("Cannot open output file: " + path);
    }

    const char magic[4] = {'E', 'D', 'B', '1'};
    out.write(magic, sizeof(magic));

    uint32_t instructionCount = static_cast<uint32_t>(bc.instructions.size());
    uint32_t constantCount = static_cast<uint32_t>(bc.constants.size());
    uint32_t stringCount = static_cast<uint32_t>(bc.strings.size());

    out.write(reinterpret_cast<const char*>(&instructionCount), sizeof(instructionCount));
    out.write(reinterpret_cast<const char*>(&constantCount), sizeof(constantCount));
    out.write(reinterpret_cast<const char*>(&stringCount), sizeof(stringCount));

    uint32_t lineCount = static_cast<uint32_t>(bc.lineNumbers.size());
    out.write(reinterpret_cast<const char*>(&lineCount), sizeof(lineCount));
    for(int line : bc.lineNumbers) {
        uint32_t l = static_cast<uint32_t>(line);
        out.write(reinterpret_cast<const char*>(&l), sizeof(l));
    }

    for(const auto& inst : bc.instructions) {
        uint8_t op = static_cast<uint8_t>(inst.op);
        uint8_t dst = static_cast<uint8_t>(inst.dst);
        uint8_t left = static_cast<uint8_t>(inst.left);
        uint8_t right = static_cast<uint8_t>(inst.right);
        out.write(reinterpret_cast<const char*>(&op), sizeof(op));
        out.write(reinterpret_cast<const char*>(&dst), sizeof(dst));
        out.write(reinterpret_cast<const char*>(&left), sizeof(left));
        out.write(reinterpret_cast<const char*>(&right), sizeof(right));
    }

    for(double value : bc.constants) {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    for(const auto& str : bc.strings) {
        uint32_t len = static_cast<uint32_t>(str.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        out.write(str.data(), len);
    }

    if(!out.good()) {
        throw std::runtime_error("Failed writing bytecode file: " + path);
    }
}

TapeBundle readTapeBundleFromFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if(!in.is_open()) {
        throw std::runtime_error("Cannot open bytecode file: " + path);
    }

    char magic[4] = {};
    in.read(magic, sizeof(magic));
    const char expected[4] = {'E', 'D', 'B', '1'};
    if(std::memcmp(magic, expected, sizeof(expected)) != 0) {
        throw std::runtime_error("Invalid bytecode format: " + path);
    }

    uint32_t instructionCount = 0;
    uint32_t constantCount = 0;
    uint32_t stringCount = 0;
    in.read(reinterpret_cast<char*>(&instructionCount), sizeof(instructionCount));
    in.read(reinterpret_cast<char*>(&constantCount), sizeof(constantCount));
    in.read(reinterpret_cast<char*>(&stringCount), sizeof(stringCount));

    TapeBundle bc;
    bc.instructions.reserve(instructionCount);
    bc.constants.resize(constantCount);
    bc.strings.reserve(stringCount);

    uint32_t lineCount = 0;
    in.read(reinterpret_cast<char*>(&lineCount), sizeof(lineCount));
    bc.lineNumbers.resize(lineCount);
    for(uint32_t i = 0; i < lineCount; ++i) {
        uint32_t l = 0;
        in.read(reinterpret_cast<char*>(&l), sizeof(l));
        bc.lineNumbers[i] = static_cast<int>(l);
    }

    for(uint32_t i = 0; i < instructionCount; ++i) {
        uint8_t op = 0, dst = 0, left = 0, right = 0;
        in.read(reinterpret_cast<char*>(&op), sizeof(op));
        in.read(reinterpret_cast<char*>(&dst), sizeof(dst));
        in.read(reinterpret_cast<char*>(&left), sizeof(left));
        in.read(reinterpret_cast<char*>(&right), sizeof(right));
        bc.instructions.push_back({op, dst, left, right});
    }

    for(uint32_t i = 0; i < constantCount; ++i) {
        in.read(reinterpret_cast<char*>(&bc.constants[i]), sizeof(double));
    }

    for(uint32_t i = 0; i < stringCount; ++i) {
        uint32_t len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string s(len, '\0');
        if(len > 0) {
            in.read(&s[0], len);
        }
        bc.strings.push_back(std::move(s));
    }

    if(!in.good() && !in.eof()) {
        throw std::runtime_error("Failed reading bytecode file: " + path);
    }
    return bc;
}