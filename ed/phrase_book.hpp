#pragma once
#include <stack>
#include <memory>
#include <string>
#include "lexicon.hpp"
#include "syntax.hpp"
#include "binding_table.hpp"

enum class PhraseState { ExpectOperand, ExpectOperator, Done, Error };

class PhraseBook {
private:
    bool insideFunction = false;
    bool insideLoop = false;
    bool insideSwitch = false;
    Lexicon& tokenizer;
    BindingTable& symTable;
    Lexeme currentLexeme;
    std::stack<std::string> ops;
    std::stack<std::shared_ptr<SynNode>> nodes;
    PhraseState state;

    void nextLexeme() { currentLexeme = tokenizer.getNextLexeme(); }
    int precedence(const std::string& op) const;
    void processOperatorStack(const std::string& currentOp);
    void createNodeFromOp();
    std::shared_ptr<SynNode> createBinaryNode(const std::string& op,
        std::shared_ptr<SynNode> left, std::shared_ptr<SynNode> right);
    std::shared_ptr<SynNode> createUnaryNode(const std::string& op,
        std::shared_ptr<SynNode> child);
    std::shared_ptr<SynNode> resolveRefSlot(const std::string& name);
    bool shouldDefaultToLocal(bool explicitGlobal) const;

    std::shared_ptr<Stmt> parseStatement();
    std::shared_ptr<Stmt> parseIf();
    std::shared_ptr<Stmt> parseWhile();
    std::shared_ptr<Stmt> parseBlock();
    std::shared_ptr<Stmt> parseAssignment();
    std::shared_ptr<Stmt> parsePrint();
    std::shared_ptr<SynNode> parseExpression();
    std::shared_ptr<Stmt> parseFor();
    std::shared_ptr<Stmt> parseFunction();
    std::shared_ptr<Stmt> parseReturn();
    std::shared_ptr<SynNode> parseFunctionCall(const std::string& name);
    std::shared_ptr<SynNode> parseBuiltInCall(const std::string& name);
    std::shared_ptr<Stmt> parseSwitch();
public:
    PhraseBook(Lexicon& tok, BindingTable& st)
        : tokenizer(tok), symTable(st), state(PhraseState::ExpectOperand) { nextLexeme(); }

    std::shared_ptr<Stmt> parseProgram();
    void error(const std::string& message) {
        state = PhraseState::Error;
        throw std::runtime_error(
            "ED (line " + std::to_string(currentLexeme.lineNumber) + "): " + message
        );
    }
};