#pragma once
#include "source_scan.hpp"
#include <unordered_map>

enum class LexKind {
    Number, Name, Operator, OpenParen, CloseParen, EndOfExpr, Error,
    Choose, Otherwise, During, Span,
    OpenBrace, CloseBrace,
    Semicolon,
    CompareOp,
    Assign,
    Comma,
    Emit,
    StringLiteral,
    And,
    Or,
    Not,
    CompoundAssign,
    Boolean,
    Fn,
    Proc,
    Give,
    Sheet,
    Cell,
    ConstMath,
    QuestionMark,
    Colon,
    Empty,
    Halt,
    Skip,
    Pick,
    On,
    Fallback,
    Line,
};

struct Lexeme {
    LexKind type;
    std::string value;
    int lineNumber = 0;
};

class Lexicon {
    private:
        SourceScan& lexer;
        bool isOperator(const std::string& s) const;
    public:
        Lexicon(SourceScan&);
        Lexeme getNextLexeme();
        
};