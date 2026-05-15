#include "lexicon.hpp"
#include <algorithm>
#include <cctype>
#include <unordered_set>

const std::unordered_map<std::string, LexKind> operations = {
    {"+", LexKind::Operator}, {"-", LexKind::Operator},
    {"*", LexKind::Operator}, {"/", LexKind::Operator},
    {"&", LexKind::Operator}, {"|", LexKind::Operator},
    {"^", LexKind::Operator}, {"%", LexKind::Operator},
    {">>", LexKind::Operator}, {"<<", LexKind::Operator},
    {"**", LexKind::Operator}, {"//", LexKind::Operator},
    {"%/", LexKind::Operator}, {"~", LexKind::Operator},
};

namespace {
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

const std::unordered_set<std::string> BuiltIns = {
    "sin", "cos", "tan",
    "asin", "acos", "atan", "atan2",
    "sqrt", "exp", "log", "ln", "log10",
    "ceil", "floor", "abs", "round",
    "fmod", "cbrt", "log2", "pow", "log_ab",
    "input",
    "ord", "chr", "type",
    "bin", "oct", "hex", "dec",
    "len",
};
}

Lexicon::Lexicon(SourceScan& l) : lexer(l) {}

bool Lexicon::isOperator(const std::string& s) const {
    return operations.find(s) != operations.end();
}

Lexeme Lexicon::getNextLexeme() {
    while(!lexer.isEOF()) {
        char current = static_cast<char>(lexer.peek());
        if(isspace(static_cast<unsigned char>(current))) {
            lexer.advance();
            continue;
        }
        if(current == '/') {
            lexer.markLexemeStart();
            lexer.advance();
            if(lexer.isEOF()) {
                return {LexKind::Operator, "/", lexer.getLineNumber()};
            }
            char n = static_cast<char>(lexer.peek());
            if(n == '/') {
                lexer.advance();
                while(!lexer.isEOF() && lexer.peek() != '\n') lexer.advance();
                continue;
            }
            if(n == '*') {
                lexer.advance();
                while(!lexer.isEOF()) {
                    if(lexer.peek() == '*') {
                        lexer.advance();
                        if(!lexer.isEOF() && lexer.peek() == '/') {
                            lexer.advance();
                            break;
                        }
                    } else {
                        lexer.advance();
                    }
                }
                continue;
            }
            return {LexKind::Operator, "/", lexer.getLineNumber()};
        }
        break;
    }

    lexer.markLexemeStart();

    if(lexer.isEOF()) return {LexKind::EndOfExpr, "", lexer.getLineNumber()};

    char current = static_cast<char>(lexer.peek());

    if(current == '"' || current == '\'') {
        char openQuote = current;
        lexer.advance();
        std::string str;
        while(!lexer.isEOF() && lexer.peek() != openQuote) {
            char c = (char)lexer.peek();
            if(c == '\\') {
                lexer.advance();
                char esc = (char)lexer.peek();
                switch(esc) {
                    case 'n':  str += '\n'; break;
                    case 't':  str += '\t'; break;
                    case '"':  str += '"';  break;
                    case '\'': str += '\''; break;
                    case '\\': str += '\\'; break;
                    default:   str += esc;  break;
                }
            } else {
                str += c;
            }
            lexer.advance();
        }
        if(!lexer.isEOF()) lexer.advance();
        return {LexKind::StringLiteral, str, lexer.getLineNumber()};
    }

    if(current == '(') { lexer.advance(); return {LexKind::OpenParen,  "(", lexer.getLineNumber()}; }
    if(current == ')') { lexer.advance(); return {LexKind::CloseParen,  ")", lexer.getLineNumber()}; }
    if(current == '{') { lexer.advance(); return {LexKind::OpenBrace,   "{", lexer.getLineNumber()}; }
    if(current == '}') { lexer.advance(); return {LexKind::CloseBrace,  "}", lexer.getLineNumber()}; }
    if(current == ';') { lexer.advance(); return {LexKind::Semicolon,   ";", lexer.getLineNumber()}; }
    if(current == ',') { lexer.advance(); return {LexKind::Comma,       ",", lexer.getLineNumber()}; }
    if(current == '?') { lexer.advance(); return {LexKind::QuestionMark, "?", lexer.getLineNumber()}; }
    if(current == ':') { lexer.advance(); return {LexKind::Colon, ":", lexer.getLineNumber()}; }

    if(current == '!') {
        lexer.advance();
        if(!lexer.isEOF() && lexer.peek() == '=') {
            lexer.advance();
            return {LexKind::CompareOp, "!=", lexer.getLineNumber()};
        }
        return {LexKind::Not, "not", lexer.getLineNumber()};
    }

    if(current == '&') {
        lexer.advance();
        if(!lexer.isEOF() && lexer.peek() == '&') {
            lexer.advance();
            return {LexKind::And, "and", lexer.getLineNumber()};
        }
        return {LexKind::Operator, "&", lexer.getLineNumber()};
    }
    if(current == '|') {
        lexer.advance();
        if(!lexer.isEOF() && lexer.peek() == '|') {
            lexer.advance();
            return {LexKind::Or, "or", lexer.getLineNumber()};
        }
        return {LexKind::Operator, "|", lexer.getLineNumber()};
    }

    if(isdigit(static_cast<unsigned char>(current)) || current == '.') {
        std::string val;
        bool hasDot = false;
        while(!lexer.isEOF() && (isdigit(static_cast<unsigned char>(lexer.peek())) || lexer.peek() == '.')) {
            if(lexer.peek() == '.') {
                if(hasDot) break;
                hasDot = true;
            }
            val += (char)lexer.peek();
            lexer.advance();
        }
        return {LexKind::Number, val, lexer.getLineNumber()};
    }

    if(isalpha(static_cast<unsigned char>(current)) || current == '_') {
        std::string name;
        while(!lexer.isEOF() && (isalnum(static_cast<unsigned char>(lexer.peek())) || lexer.peek() == '_')) {
            name += (char)lexer.peek();
            lexer.advance();
        }
        const std::string lowered = toLower(name);
        if(lowered == "choose")   return {LexKind::Choose, lowered, lexer.getLineNumber()};
        if(lowered == "otherwise") return {LexKind::Otherwise, lowered, lexer.getLineNumber()};
        if(lowered == "during")   return {LexKind::During, lowered, lexer.getLineNumber()};
        if(lowered == "span")     return {LexKind::Span, lowered, lexer.getLineNumber()};
        if(lowered == "emit")     return {LexKind::Emit, lowered, lexer.getLineNumber()};
        if(lowered == "fn")       return {LexKind::Fn, lowered, lexer.getLineNumber()};
        if(lowered == "proc")     return {LexKind::Proc, lowered, lexer.getLineNumber()};
        if(lowered == "give")     return {LexKind::Give, lowered, lexer.getLineNumber()};
        if(lowered == "cell")     return {LexKind::Cell, lowered, lexer.getLineNumber()};
        if(lowered == "sheet")    return {LexKind::Sheet, lowered, lexer.getLineNumber()};
        if(lowered == "pi" || lowered == "euler") return {LexKind::ConstMath, lowered, lexer.getLineNumber()};
        if(lowered == "yes" || lowered == "no") return {LexKind::Boolean, lowered, lexer.getLineNumber()};
        if(BuiltIns.find(lowered) != BuiltIns.end()) return {LexKind::Name, lowered, lexer.getLineNumber()};
        if(lowered == "empty") return {LexKind::Empty, lowered, lexer.getLineNumber()};
        if(lowered == "halt") return {LexKind::Halt, lowered, lexer.getLineNumber()};
        if(lowered == "skip") return {LexKind::Skip, lowered, lexer.getLineNumber()};
        if(lowered == "pick") return {LexKind::Pick, lowered, lexer.getLineNumber()};
        if(lowered == "on") return {LexKind::On, lowered, lexer.getLineNumber()};
        if(lowered == "fallback") return {LexKind::Fallback, lowered, lexer.getLineNumber()};
        return {LexKind::Name, name, lexer.getLineNumber()};
    }

    std::string op;
    op += current;
    lexer.advance();
    if(!lexer.isEOF()) {
        char next = static_cast<char>(lexer.peek());
        if((current == '=' && next == '=') ||
           (current == '!' && next == '=') ||
           (current == '<' && next == '=') ||
           (current == '>' && next == '=') ||
           (current == '<' && next == '<') ||
           (current == '>' && next == '>') ||
           (current == '+' && next == '=') ||
           (current == '-' && next == '=') ||
           (current == '/' && next == '=') ||
           (current == '*' && next == '=') ||
           (current == '%' && next == '=') ||
           (current == '^' && next == '=') ||
           (current == '*' && next == '*') ||
           (current == '/' && next == '/') ||
           (current == '%' && next == '/')) {
            op += next;
            lexer.advance();
        }
    }
    if(op == "=")  return {LexKind::Assign, op, lexer.getLineNumber()};
    if(op == "==" || op == "!=" || op == ">" || op == "<" || op == ">=" || op == "<=") {
        return {LexKind::CompareOp, op, lexer.getLineNumber()};
    }

    if(op == "+=" || op == "-=" || op == "/=" || op == "*=" || op == "%=" || op == "^=") {
        return {LexKind::CompoundAssign, op, lexer.getLineNumber()};
    }
    if(isOperator(op)) return {LexKind::Operator, op, lexer.getLineNumber()};

    return {LexKind::Error, op, lexer.getLineNumber()};
}
