#include "phrase_book.hpp"
#include <cmath>

std::shared_ptr<SynNode> PhraseBook::createBinaryNode(const std::string& op,
    std::shared_ptr<SynNode> left, std::shared_ptr<SynNode> right) {
    auto leftNum  = std::dynamic_pointer_cast<LitNumber>(left);
    auto rightNum = std::dynamic_pointer_cast<LitNumber>(right);
    if(leftNum && rightNum) {
        double l = leftNum->getValue(), r = rightNum->getValue();
        if(op == "+") return std::make_shared<LitNumber>(l + r);
        if(op == "-") return std::make_shared<LitNumber>(l - r);
        if(op == "*") return std::make_shared<LitNumber>(l * r);
        if(op == "**") return std::make_shared<LitNumber>(std::pow(l,r));
        if(op == "/") {
            if(r == 0) return std::make_shared<OpBinary>(op, left, right);
            return std::make_shared<LitNumber>(l / r);
        }
        if(op == "//") {
            if(r == 0) return std::make_shared<OpBinary>(op, left, right);
            return std::make_shared<LitNumber>(std::floor(l/r));
        }
        long long li = (long long)l, ri = (long long)r;
        if(op == "&")  return std::make_shared<LitNumber>((double)(li & ri));
        if(op == "|")  return std::make_shared<LitNumber>((double)(li | ri));
        if(op == "^")  return std::make_shared<LitNumber>((double)(li ^ ri));
        if(op == "<<") return std::make_shared<LitNumber>((double)(li << ri));
        if(op == ">>") return std::make_shared<LitNumber>((double)((uint32_t)li >> (ri & 0x1F)));
        if(op == "%")  {
            if(ri == 0) return std::make_shared<OpBinary>(op, left, right);
            return std::make_shared<LitNumber>((double)(li % ri));
        }
        if(op == "%/") {
            if(r == 0) return std::make_shared<OpBinary>(op, left, right);
            return std::make_shared<LitNumber>(l / r - std::floor(l / r));
        }
        if(op == "and") {
            return std::make_shared<LitNumber>((l != 0 && r != 0) ? 1.0 : 0.0);
        }
        if(op == "or") {
            return std::make_shared<LitNumber>((l != 0 || r != 0) ? 1.0 : 0.0);
        }
        if (op == ">")  return std::make_shared<LitNumber>(l > r ? 1.0 : 0.0);
        if (op == "<")  return std::make_shared<LitNumber>(l < r ? 1.0 : 0.0);
        if (op == ">=") return std::make_shared<LitNumber>(l >= r ? 1.0 : 0.0);
        if (op == "<=") return std::make_shared<LitNumber>(l <= r ? 1.0 : 0.0);
        if (op == "==") return std::make_shared<LitNumber>(l == r ? 1.0 : 0.0);
        if (op == "!=") return std::make_shared<LitNumber>(l != r ? 1.0 : 0.0);
    }
    return std::make_shared<OpBinary>(op, left, right);
}

std::shared_ptr<SynNode> PhraseBook::resolveRefSlot(const std::string& name) {
    int32_t localOffset = 0;
    if (symTable.tryGetLocalOffset(name, localOffset)) {
        return std::make_shared<RefSlot>(localOffset);
    }

    size_t globalAddr = 0;
    if (symTable.tryGetGlobalAddress(name, globalAddr)) {
        return std::make_shared<RefSlot>(globalAddr);
    }

    error("Undefined variable: " + name);
    return nullptr;
}

bool PhraseBook::shouldDefaultToLocal(bool explicitGlobal) const {
    // return !explicitGlobal && symTable.hasActiveScope();
    if(explicitGlobal) return false;
    return symTable.isInsideFunction() || (symTable.getScopeDepth() > 1);
}

std::shared_ptr<SynNode> PhraseBook::createUnaryNode(const std::string& op,
    std::shared_ptr<SynNode> child) {
    auto num = std::dynamic_pointer_cast<LitNumber>(child);
    if(num) {
        if(op == "-" || op == "_") return std::make_shared<LitNumber>(-num->getValue());
        if(op == "+" || op == "#") return num;
        if(op == "~") {
            long long val = static_cast<long long>(num -> getValue());
            return std::make_shared<LitNumber>(static_cast<double>(~val));
        }
    }
    if(op == "not") {
        if(num) return std::make_shared<LitNumber>(num -> getValue() == 0 ? 1.0 : 0.0);
        return std::make_shared<OpUnary>("not", child);
    }
    return std::make_shared<OpUnary>(op, child);
}

int PhraseBook::precedence(const std::string& op) const {
    if(op == "or") return 0;
    if(op == "and") return 1;
    if(op == "==" || op == "!=" || op == ">" || op == "<" || op == ">=" || op == "<=") return 2;
    if(op == "|" || op == "^") return 3;
    if(op == "&") return 4;
    if(op == "<<" || op == ">>") return 5;
    if(op == "+" || op == "-") return 6;
    if(op == "*" || op == "/" || op == "%" || op == "//" || op == "%/") return 7;
    if(op == "not" || op == "_" || op == "#" || op == "~") return 8;
    if(op == "**") return 9;
    return -1;
}

void PhraseBook::createNodeFromOp() {
    if(ops.empty()) return;
    std::string op = ops.top(); ops.pop();
    if(op == "_" || op == "#" || op == "not" || op == "~") {
        if(nodes.empty()) { state = PhraseState::Error; return; }
        auto operand = nodes.top(); nodes.pop();
        nodes.push(createUnaryNode(op, operand));
    } else {
        if(nodes.size() < 2) { state = PhraseState::Error; return; }
        auto right = nodes.top(); nodes.pop();
        auto left  = nodes.top(); nodes.pop();
        nodes.push(createBinaryNode(op, left, right));
    }
}

void PhraseBook::processOperatorStack(const std::string& currentOp) {
    while(!ops.empty() && ops.top() != "(") {
        int topPrec = precedence(ops.top());
        int currentPrec = precedence(currentOp);
        if(topPrec < currentPrec) break;

        if(topPrec == currentPrec) {
            if(currentOp == "**") break;
        }
        createNodeFromOp();
    }
}

std::shared_ptr<Stmt> PhraseBook::parseProgram() {
    symTable.beginProgramParse();
    auto block = std::make_shared<StmtBlock>();
    while(currentLexeme.type != LexKind::EndOfExpr) {
        auto stmt = parseStatement();
        if(stmt) block->addStatement(stmt);
        else if(state == PhraseState::Error) break;
        else nextLexeme();
    }
    symTable.endProgramParse();
    return block;
}

std::shared_ptr<Stmt> PhraseBook::parseStatement() {
    int stmtLine = currentLexeme.lineNumber;
    switch(currentLexeme.type) {
        case LexKind::Choose: { 
            auto s = parseIf();
            if(s) s -> lineNumber = stmtLine;
            return s;
        }
        case LexKind::During: {
            auto s = parseWhile();
            if(s) s -> lineNumber = stmtLine;
            return s;
        }     
        case LexKind::OpenBrace: {
            int blockLine = currentLexeme.lineNumber;
            auto s = parseBlock();
            if(s) s -> lineNumber = blockLine;
            return s;
        }
        case LexKind::Emit: {
            auto s = parsePrint();
            if(s) s -> lineNumber = stmtLine;
            return s;
        }
        case LexKind::Span: {
            auto s = parseFor();
            if(s) s -> lineNumber = stmtLine;
            return s;
        }
        case LexKind::Proc:
        case LexKind::Fn: {
            auto s = parseFunction();
            if(s) s -> lineNumber = stmtLine;
            return s;
        }
        case LexKind::Give: {
            auto s = parseReturn();
            if(s) s -> lineNumber = stmtLine;
            return s;
        }
        case LexKind::Name: {
            auto s = parseAssignment();
            if(s) s -> lineNumber = stmtLine;
            return s;
        }
        case LexKind::Pick: {
            auto s = parseSwitch();
            if(s) s -> lineNumber = stmtLine;
            return s;
        }

        case LexKind::Cell: case LexKind::Sheet: {
            auto s = parseAssignment();
            if(s) s -> lineNumber = stmtLine;
            return s;
        }
        case LexKind::Halt: {
            if(!insideLoop && !insideSwitch) {
                error("halt outside pick or span/during loop");
            }
            auto s = std::make_shared<StmtHalt>();
            nextLexeme(); // skip 'halt'
            if(currentLexeme.type == LexKind::Semicolon) {
                nextLexeme(); // skip ';'
            }
            s -> lineNumber = stmtLine;
            return s;
        }

        case LexKind::Skip: {
            if(!insideLoop) {
                error("skip outside span/during loop");
            }
            auto s = std::make_shared<StmtSkip>();
            nextLexeme(); // skip 'skip'
            if(currentLexeme.type == LexKind::Semicolon) {
                nextLexeme(); // skip ';'
            }
            s -> lineNumber = stmtLine;
            return s;
        }

        default:
            parseExpression();
            if(currentLexeme.type == LexKind::Semicolon) nextLexeme();
            return nullptr;
    }
}

std::shared_ptr<Stmt> PhraseBook::parseSwitch() {
    nextLexeme(); // skip 'pick'
    if(currentLexeme.value != "(") {
        error("Expected '(' after pick");
    }
    nextLexeme(); // skip '('
    auto expr = parseExpression();
    if(!expr) { 
        state = PhraseState::Error; 
        return nullptr;
    }
    if(currentLexeme.value != ")") { 
        error("Expected ')' after pick expression");
    }
    nextLexeme(); // skip ')'
    if(currentLexeme.type != LexKind::OpenBrace) {
        error("Expected '{' for pick body");
    }

    bool oldInsideSwitch = insideSwitch;
    insideSwitch = true;
    symTable.enterBlockScope();

    nextLexeme(); // skip '{'
    std::vector<PickArm> cases;
    std::shared_ptr<Stmt> defaultBody = nullptr;

    while(currentLexeme.type == LexKind::On || currentLexeme.type == LexKind::Fallback) {
        if(currentLexeme.type == LexKind::On) {
            nextLexeme(); // skip 'on'
            std::vector<std::shared_ptr<SynNode>> values;
            do {
                auto val = parseExpression();
                if(!val) { state = PhraseState::Error; return nullptr; }
                values.push_back(val);
                if(currentLexeme.type == LexKind::Comma)
                    nextLexeme();
                else
                    break;
            } while(true);

            if(currentLexeme.type != LexKind::Colon) {
                error("Expected ':' after on values");
            }
            nextLexeme(); // skip ':'

            
            auto caseBody = std::make_shared<StmtBlock>();
            while(currentLexeme.type != LexKind::On &&
                  currentLexeme.type != LexKind::Fallback &&
                  currentLexeme.type != LexKind::CloseBrace) {
                auto stmt = parseStatement();
                if(stmt) caseBody->addStatement(stmt);
                else if(state == PhraseState::Error) break;
            }
            cases.push_back({std::move(values), caseBody});
        }
        else if(currentLexeme.type == LexKind::Fallback) {
            if(defaultBody) {
                error("Multiple fallback arms in pick");
            }
            nextLexeme(); // skip 'fallback'
            if(currentLexeme.type != LexKind::Colon) {
                error("Expected ':' after fallback");
            }
            nextLexeme(); // skip ':'
            auto defBlock = std::make_shared<StmtBlock>();
            while(currentLexeme.type != LexKind::On &&
                  currentLexeme.type != LexKind::Fallback &&
                  currentLexeme.type != LexKind::CloseBrace) {
                auto stmt = parseStatement();
                if(stmt) defBlock->addStatement(stmt);
                else if(state == PhraseState::Error) break;
            }
            defaultBody = defBlock;
        }
    }

    if(currentLexeme.type != LexKind::CloseBrace) {
        error("Expected '}' at end of pick");
    }
    nextLexeme(); // skip '}'

    symTable.exitBlockScope();
    insideSwitch = oldInsideSwitch;
    return std::make_shared<StmtPick>(expr, std::move(cases), defaultBody);
}

std::shared_ptr<Stmt> PhraseBook::parseIf() {
    nextLexeme(); // skip 'choose'
    if(currentLexeme.value != "(") { state = PhraseState::Error; return nullptr; }
    nextLexeme();
    auto cond = parseExpression();
    if(currentLexeme.value != ")") { state = PhraseState::Error; return nullptr; }
    nextLexeme();
    
    // Enter new scope for 'then' branch
    symTable.enterBlockScope();
    auto thenBr = parseStatement();
    symTable.exitBlockScope();
    
    std::shared_ptr<Stmt> elseBr = nullptr;
    if(currentLexeme.type == LexKind::Otherwise) {
        nextLexeme();
        // Enter new scope for 'else' branch
        symTable.enterBlockScope();
        elseBr = parseStatement();
        symTable.exitBlockScope();
    }
    return std::make_shared<IfStmt>(cond, thenBr, elseBr);
}

std::shared_ptr<Stmt> PhraseBook::parseWhile() {
    nextLexeme(); // skip 'during'
    if(currentLexeme.value != "(") { state = PhraseState::Error; return nullptr; }
    nextLexeme();
    auto cond = parseExpression();
    if(currentLexeme.value != ")") { state = PhraseState::Error; return nullptr; }
    nextLexeme();
    
    // Enter new scope for while body
    symTable.enterBlockScope();
    
    bool wasInloop = insideLoop;
    insideLoop = true;

    auto body = parseStatement();
    
    insideLoop = wasInloop;
    
    symTable.exitBlockScope();
    
    return std::make_shared<WhileStmt>(cond, body);
}

std::shared_ptr<Stmt> PhraseBook::parseBlock() {
    auto block = std::make_shared<StmtBlock>();
    nextLexeme(); // skip '{'
    
    // Enter new scope for this block
    symTable.enterBlockScope();

    while(currentLexeme.type != LexKind::CloseBrace &&
          currentLexeme.type != LexKind::EndOfExpr) {
        auto stmt = parseStatement();
        if(stmt) block->addStatement(stmt);
        else if(state == PhraseState::Error) {
            break;
        }
    }

    if(currentLexeme.type == LexKind::CloseBrace) {
        nextLexeme();
    }
    
    // Exit block scope
    symTable.exitBlockScope();
    
    return block;
}

std::shared_ptr<Stmt> PhraseBook::parseAssignment() {
    bool explicitLocal = false;
    bool explicitGlobal = false;
    std::string name;

    if (currentLexeme.type == LexKind::Cell) {
        explicitLocal = true;
        nextLexeme();
    } 
    else if (currentLexeme.type == LexKind::Sheet) {
        explicitGlobal = true;
        nextLexeme();
    }

    if (currentLexeme.type != LexKind::Name) {
        state = PhraseState::Error;
        return nullptr;
    }

    name = currentLexeme.value;
    nextLexeme();

    if(currentLexeme.type == LexKind::Semicolon) {
        bool isLocal = shouldDefaultToLocal(explicitGlobal);
        std::shared_ptr<SynNode> zeroNode = std::make_shared<LitEmpty>();
        if(isLocal) {
            int32_t off = symTable.getLocalOffset(name); // local, default 0
            nextLexeme(); // skip ';'
            return std::make_shared<StmtAssign>(off,zeroNode);
        } else {
            size_t addr = symTable.getGlobalAddress(name); // global, default 0
            nextLexeme(); // skip ';'
            return std::make_shared<StmtAssign>(addr, zeroNode);
        }
    }

    if (currentLexeme.type == LexKind::OpenParen) {
        auto callNode = parseFunctionCall(name);
        if (currentLexeme.type == LexKind::Semicolon) nextLexeme();
        return std::make_shared<StmtCallExpr>(callNode);
    }

    bool isLocalVar = false;

    if (explicitLocal) {
        isLocalVar = true;
    } 
    else if (explicitGlobal) {
        isLocalVar = false;
    } 
    else {
        int32_t localOffset = 0;
        size_t globalAddr = 0;
        if(symTable.tryGetLocalOffset(name, localOffset)) {
            isLocalVar = true;
        } else if(symTable.tryGetGlobalAddress(name, globalAddr)) {
            isLocalVar = false;
        } else {
            isLocalVar = shouldDefaultToLocal(false);
        }
    }

    if(currentLexeme.type != LexKind::Assign && 
       currentLexeme.type != LexKind::CompoundAssign) {
        state = PhraseState::Error;
        return nullptr;
    }

    std::string assignOp = currentLexeme.value;
    nextLexeme();

    auto valueExpr = parseExpression();

    if(currentLexeme.type == LexKind::Semicolon) {
        nextLexeme();
    }

    if(assignOp != "=") {
        int32_t localOffset = 0;
        size_t globalAddr = 0;

        if (explicitLocal) {
            if (!symTable.tryGetLocalOffset(name, localOffset)) {
                error("Undefined local variable in compound assignment: " + name);
            }
            isLocalVar = true;
        } else if (explicitGlobal) {
            if (!symTable.tryGetGlobalAddress(name, globalAddr)) {
                error("Undefined global variable in compound assignment: " + name);
            }
            isLocalVar = false;
        } else if (symTable.tryGetLocalOffset(name, localOffset)) {
            isLocalVar = true;
        } else if (symTable.tryGetGlobalAddress(name, globalAddr)) {
            isLocalVar = false;
        } else {
            if(symTable.hasActiveScope()) {
                error("Undefined variable in compound assignment: " + name);
            } else {
                globalAddr = symTable.getGlobalAddress(name);
                isLocalVar = false;
            }
        }

        std::string mathOp;
        if(assignOp == "+=")      mathOp = "+";
        else if(assignOp == "-=") mathOp = "-";
        else if(assignOp == "*=") mathOp = "*";
        else if(assignOp == "/=") mathOp = "/";
        else if(assignOp == "%=") mathOp = "%";
        else if(assignOp == "^=") mathOp = "^";

        std::shared_ptr<SynNode> varNode;
        if (isLocalVar) {
            varNode = std::make_shared<RefSlot>(localOffset);
        } else {
            varNode = std::make_shared<RefSlot>(globalAddr);
        }

        valueExpr = std::make_shared<OpBinary>(mathOp, varNode, valueExpr);
    } else {
        if (isLocalVar) {
            symTable.getLocalOffset(name);
        } else {
            symTable.getGlobalAddress(name);
        }
    }

    if (isLocalVar) {
        int32_t off = symTable.getLocalOffset(name);
        return std::make_shared<StmtAssign>(off, valueExpr);
    } else {
        size_t addr = symTable.getGlobalAddress(name);
        return std::make_shared<StmtAssign>(addr, valueExpr);
    }
}

std::shared_ptr<Stmt> PhraseBook::parsePrint() {
    nextLexeme(); // skip 'emit'
    if(currentLexeme.value != "(") { state = PhraseState::Error; return nullptr; }
    nextLexeme(); // skip '('

    std::vector<std::shared_ptr<SynNode>> exprs;
    while(currentLexeme.value != ")" && currentLexeme.type != LexKind::EndOfExpr) {
        exprs.push_back(parseExpression());
        if(currentLexeme.type == LexKind::Comma) nextLexeme();
    }

    if(currentLexeme.value != ")") { state = PhraseState::Error; return nullptr; }
    nextLexeme(); // skip ')'

    if(exprs.size() <= 1) {
        exprs.push_back(std::make_shared<LitText>("\n"));
    }

    if(currentLexeme.type == LexKind::Semicolon) {
        nextLexeme();
    }

    return std::make_shared<StmtEmit>(std::move(exprs));
}

std::shared_ptr<SynNode> PhraseBook::parseBuiltInCall(const std::string& name) {
    if(name == "len") {
        nextLexeme(); // skip '('
        
        // Save parser state before parseExpression()
        auto savedOps = ops;
        auto savedNodes = nodes;
        auto savedState = state;
        
        auto arg = parseExpression();
        
        // Restore parser state after recursive call
        ops = savedOps;
        nodes = savedNodes;
        state = savedState;
        
        if(!arg) return nullptr;
        if(currentLexeme.value != ")") {
            state = PhraseState::Error;
            return nullptr;
        }
        nextLexeme(); // skip ')'
        return std::make_shared<BuiltinLen>(arg);
    }
    // future built-ins
    state = PhraseState::Error;
    return nullptr;
}

std::shared_ptr<SynNode> PhraseBook::parseExpression() {
    state = PhraseState::ExpectOperand;
    while(!ops.empty()) ops.pop();
    while(!nodes.empty()) nodes.pop();

    while(true) {
        Lexeme token = currentLexeme;
        
        if(token.type == LexKind::EndOfExpr ||
           token.type == LexKind::Semicolon ||
           token.type == LexKind::Comma ||
           token.type == LexKind::Colon ||
           (token.type == LexKind::CloseParen && ops.empty())) {
            break;
        }

        switch(state) {
            case PhraseState::ExpectOperand:
                if(token.type == LexKind::Number) {
                    nodes.push(std::make_shared<LitNumber>(std::stod(token.value)));
                    state = PhraseState::ExpectOperator;
                    nextLexeme();
                } else if(token.type == LexKind::Boolean) {
                    double val = (token.value == "yes") ? 1.0 : 0.0;
                    nodes.push(std::make_shared<LitNumber>(val));
                    state = PhraseState::ExpectOperator;
                    nextLexeme();
                
                    } else if(token.type == LexKind::Name) {
                        std::string name = token.value;
                        nextLexeme();
                        if(currentLexeme.type == LexKind::OpenParen) {
                            std::shared_ptr<SynNode> node;
                            if(name == "len") { // or any other built-in
                                node = parseBuiltInCall(name);
                            } else {
                                node = parseFunctionCall(name);
                            }
                            if(!node) return nullptr;
                            nodes.push(node);
                            state = PhraseState::ExpectOperator;
                        } else {
                            nodes.push(resolveRefSlot(name));
                            state = PhraseState::ExpectOperator;
                        }
                } else if(token.type == LexKind::StringLiteral) {
                    nodes.push(std::make_shared<LitText>(token.value));
                    state = PhraseState::ExpectOperator;
                    nextLexeme();
                }  else if(token.type == LexKind::ConstMath) {
                    TapeOp constOp = (token.value == "pi") ? TapeOp::CONST_PI : TapeOp::CONST_E;
                    nodes.push(std::make_shared<LitMathConst>(constOp));
                    state = PhraseState::ExpectOperator;
                    nextLexeme();
                } else if(token.type == LexKind::OpenParen) {
                    ops.push("("); 
                    nextLexeme();
                } else if(token.type == LexKind::Operator &&
                          (token.value == "-" || token.value == "+" || token.value == "~")) {
                    ops.push(token.value == "-" ? "_" : (token.value == "+" ? "#" : "~"));
                    nextLexeme();
                } else if(token.type == LexKind::Not) {
                    ops.push("not"); nextLexeme();
                } else if(token.type == LexKind::Empty) {
                    nodes.push(std::make_shared<LitEmpty>());
                    state = PhraseState::ExpectOperator;
                    nextLexeme();
                } else {
                    state = PhraseState::Error;
                }
                break;

            case PhraseState::ExpectOperator:
                if(token.type == LexKind::QuestionMark) {
                    while(!ops.empty() && ops.top() != "(") {
                        createNodeFromOp();
                    }
                    if(state == PhraseState::Error || nodes.empty()) {
                        state = PhraseState::Error;
                        return nullptr;
                    }
                    nextLexeme(); // skip "?"
                    auto savedOps = std::move(ops);
                    auto savedNodes = std::move(nodes);
                    auto trueExpr = parseExpression();
                    ops = std::move(savedOps);
                    nodes = std::move(savedNodes);
                    if(currentLexeme.type != LexKind::Colon) {
                        state = PhraseState::Error;
                        return nullptr;
                    }
                    nextLexeme(); // skip ":"
                    savedOps = std::move(ops);
                    savedNodes = std::move(nodes);
                    auto falseExpr = parseExpression();
                    ops = std::move(savedOps);
                    nodes = std::move(savedNodes);
                    if(!trueExpr || !falseExpr || nodes.empty()) {
                        state = PhraseState::Error;
                        return nullptr;
                    }
                    auto cond = nodes.top(); nodes.pop();
                    nodes.push(std::make_shared<OpPick>(cond, trueExpr, falseExpr));
                    state = PhraseState::ExpectOperator;
                    break;
                } else if(token.type == LexKind::Operator ||
                   token.type == LexKind::CompareOp ||
                   token.type == LexKind::And ||
                   token.type == LexKind::Or ||
                   token.value == "and" || token.value == "or") {
                    processOperatorStack(token.value);
                    ops.push(token.value);
                    state = PhraseState::ExpectOperand; 
                    nextLexeme();
                } else if(token.type == LexKind::StringLiteral) {
                    processOperatorStack("+");
                    ops.push("+");
                    nodes.push(std::make_shared<LitText>(token.value));
                    state = PhraseState::ExpectOperator; 
                    nextLexeme();
                } else if(token.type == LexKind::CloseParen) {
                    while(!ops.empty() && ops.top() != "(") createNodeFromOp();
                    if(!ops.empty() && ops.top() == "(") {
                        ops.pop();
                        state = PhraseState::ExpectOperator; nextLexeme();
                    } else if (ops.empty()) {
                        break;
                    } else {
                        state = PhraseState::Error;
                    }
                } else if(token.type == LexKind::Number ||
                          token.type == LexKind::Name   ||
                          token.type == LexKind::OpenParen ||
                          token.type == LexKind::StringLiteral) {
                    processOperatorStack("*"); ops.push("*");
                    if(token.type == LexKind::Number) {
                        nodes.push(std::make_shared<LitNumber>(std::stod(token.value)));
                        state = PhraseState::ExpectOperator; nextLexeme();
                        } else if(token.type == LexKind::Name) {
                            std::string name = token.value;
                            nextLexeme();
                            if(currentLexeme.type == LexKind::OpenParen) {
                                std::shared_ptr<SynNode> node;
                                if(name == "len") {
                                    node = parseBuiltInCall(name);
                                } else {
                                    node = parseFunctionCall(name);
                                }
                                if(!node) return nullptr;
                                nodes.push(node);
                            } else {
                                nodes.push(resolveRefSlot(name));
                            }
                            state = PhraseState::ExpectOperator;
                        } else if(token.type == LexKind::StringLiteral) {
                            nodes.push(std::make_shared<LitText>(token.value));
                            state = PhraseState::ExpectOperator;
                            nextLexeme();
                    } else {
                        ops.push("(");
                        state = PhraseState::ExpectOperand; nextLexeme();
                    }
                } else {
                    state = PhraseState::Error;
                }
                break;
            default: break;
        }
        if(state == PhraseState::Error) break;
    }
    while(state != PhraseState::Error && !ops.empty()) {
        if(ops.top() == "(") {
            state = PhraseState::Error;
            break;
        }
        createNodeFromOp();
    }
    if(state == PhraseState::Error || nodes.size() != 1) {
        state = PhraseState::Error;
        return nullptr;
    }
    return nodes.top();
}

std::shared_ptr<Stmt> PhraseBook::parseFor() {
    nextLexeme(); // skip 'span'
    if(currentLexeme.value != "(") { state = PhraseState::Error; return nullptr; }
    nextLexeme(); // skip '('

    // Enter new scope for the entire for loop.
    symTable.enterBlockScope();

    // i = 0;
    auto init = parseAssignment();

    // i <= 10;
    auto cond = parseExpression();
    if(currentLexeme.type == LexKind::Semicolon) nextLexeme();

    // i += 1;
    auto update = parseAssignment();

    if(currentLexeme.value != ")") {
        state = PhraseState::Error;
        symTable.exitBlockScope();
        return nullptr;
    }
    nextLexeme();

    bool wasInLoop = insideLoop;
    insideLoop = true;

    // {...}
    auto body = parseStatement();

    insideLoop = wasInLoop;
    // Exit the for loop scope
    symTable.exitBlockScope();
    
    return std::make_shared<ForStmt>(init, cond, update, body);
}

std::shared_ptr<Stmt> PhraseBook::parseFunction() {
    bool isVoid = false;
    if(currentLexeme.type == LexKind::Proc) {
        isVoid = true;
        nextLexeme();
    } else if(currentLexeme.type == LexKind::Fn) {
        isVoid = false;
        nextLexeme();
    } else {
        error("Expected proc or fn before routine name");
        return nullptr;
    }

    if(currentLexeme.type != LexKind::Name) {
        error("Expected routine name");
        return nullptr;
    }
    std::string name = currentLexeme.value;
    nextLexeme();

    if (currentLexeme.value != "(") {
        error("Expected '(' after routine name");
    }
    nextLexeme(); // skip '('

    std::vector<std::string> params;
    while (currentLexeme.value != ")" && currentLexeme.type != LexKind::EndOfExpr) {
        if (currentLexeme.type != LexKind::Name) {
            error("Expected parameter name");
        }
        params.push_back(currentLexeme.value);
        nextLexeme();
        if (currentLexeme.type == LexKind::Comma) {
            nextLexeme();
        }
    }

    if (currentLexeme.value != ")") {
        error("Expected ')' after parameters");
    }
    nextLexeme(); // skip ')'

    if (currentLexeme.type != LexKind::OpenBrace) {
        error("Expected '{' before routine body");
    }

    bool wasInsideFunction = insideFunction;
    insideFunction = true;
    symTable.enterFunctionScope();

    for (const auto& p : params) {
        symTable.getLocalOffset(p);
    }

    auto body = parseBlock();

    if (!isVoid) {

        auto block = std::dynamic_pointer_cast<StmtBlock>(body);
        if (!block) {
            symTable.exitFunctionScope();
            insideFunction = false;
            error("Routine body must be a block");
        }

        const auto& statements = block->getStatements();
        bool hasReturn = false;
        if (!statements.empty()) {
            if (std::dynamic_pointer_cast<StmtGive>(statements.back())) {
                hasReturn = true;
            }
        }
        if (!hasReturn) {
            symTable.exitFunctionScope();
            insideFunction = false;
            error("fn '" + name + "' must end with give");
        }
    }

    int slotCount = symTable.getLocalSlotCountForFrame();

    symTable.exitFunctionScope();
    insideFunction = wasInsideFunction;

    return std::make_shared<StmtRoutine>(name, params, body, slotCount, isVoid);
}

std::shared_ptr<SynNode> PhraseBook::parseFunctionCall(const std::string& name) {
    nextLexeme(); // skip '('
    std::vector<std::shared_ptr<SynNode>> args;
    auto savedOps = ops;
    auto savedNodes = nodes;
    auto savedState = state;
    while(currentLexeme.value != ")" && currentLexeme.type != LexKind::EndOfExpr) {
        auto arg = parseExpression();
        if (state == PhraseState::Error || !arg) {
            return nullptr;
        }
        args.push_back(arg);
        ops = savedOps;
        nodes = savedNodes;
        state = savedState;
        if(currentLexeme.type == LexKind::Comma) nextLexeme();
    }
    if(currentLexeme.value != ")") {
        state = PhraseState::Error;
        return nullptr;
    }
    ops = savedOps;
    nodes = savedNodes;
    state = savedState;
    nextLexeme(); // skip ')'
    return std::make_shared<CallRoutine>(name, std::move(args));
}

std::shared_ptr<Stmt> PhraseBook::parseReturn() {
    if (!insideFunction) {
        error("give is only allowed inside routines");
    }

    nextLexeme(); // skip 'give'

    std::shared_ptr<SynNode> expr = nullptr;
    if (currentLexeme.type != LexKind::Semicolon && 
        currentLexeme.type != LexKind::EndOfExpr &&
        currentLexeme.type != LexKind::CloseBrace) {
        expr = parseExpression();
    }

    if (currentLexeme.type == LexKind::Semicolon) {
        nextLexeme();
    }

    return std::make_shared<StmtGive>(expr);
}