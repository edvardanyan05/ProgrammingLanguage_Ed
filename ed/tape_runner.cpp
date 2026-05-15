#include "tape_runner.hpp"
#include <cmath>

namespace {
    int32_t toInt32(const Value& v) {
        return static_cast<int32_t>(asNumber(v));
    }

    double fromInt32(int32_t v) {
        return static_cast<double>(v);
    }
}

void RuntimeEngine::load(const std::string& expr, BindingTable& symtable) {
    std::istringstream stream(expr);
    SourceScan lexer(stream);
    Lexicon tokenizer(lexer);
    PhraseBook parser(tokenizer, symtable);

    auto root = std::static_pointer_cast<SynNode>(parser.parseProgram());
    if(!root) throw std::runtime_error("Parsing failed!");
    Emitter compiler(symtable);
    TapeBundle bc = compiler.compile(root);
    loadTapeBundle(bc);

    if(debug_mode) {
        root->print();
        visualize(current_program);
    }
}

void RuntimeEngine::loadFromFile(const std::string& byteCodePath) {
    TapeBundle bc = readTapeBundleFromFile(byteCodePath);
    loadTapeBundle(bc);
    if(debug_mode) {
        visualize(current_program);
    }
}

void RuntimeEngine::load(const TapeBundle& bc) {
    loadTapeBundle(bc);
    if(debug_mode) {
        visualize(current_program);
    }
}

void RuntimeEngine::loadTapeBundle(const TapeBundle& bc) {
    current_program  = bc.instructions;
    current_consants = bc.constants;
    current_strings = bc.strings;
    current_lineNumbers = bc.lineNumbers;

    int maxReg = 0;
    for(const auto& inst : current_program)
        maxReg = std::max({maxReg, (int)inst.left, (int)inst.right, (int)inst.dst});
    const int kRv32RegCount = 32;
    const int kSpReg = 2; // x2 (sp)
    const int kFpReg = 8; // x8 (s0/fp)
    registers.assign(std::max(kRv32RegCount, maxReg + 1), 0.0);
    registers[kSpReg] = 10000.0;
    registers[kFpReg] = 10000.0;
    memory.resize(65536);
}

void RuntimeEngine::printTapeInstrCompact(size_t pc, const TapeInstr& inst) const {
    TapeOp op = static_cast<TapeOp>(inst.op);
    std::cout << "[" << pc << "] ";
    std::cout << std::left;
    
    switch (op) {
        case TapeOp::ADD:         std::cout << "ADD r" << inst.dst << " = r" << inst.left << " + r" << inst.right; break;
        case TapeOp::MOV:         std::cout << "MOV r" << inst.dst << " = r" << inst.left; break;
        case TapeOp::SUB:         std::cout << "SUB r" << inst.dst << " = r" << inst.left << " - r" << inst.right; break;
        case TapeOp::AND:         std::cout << "AND r" << inst.dst << " = r" << inst.left << " & r" << inst.right; break;
        case TapeOp::OR:          std::cout << "OR r" << inst.dst << " = r" << inst.left << " | r" << inst.right; break;
        case TapeOp::XOR:         std::cout << "XOR r" << inst.dst << " = r" << inst.left << " ^ r" << inst.right; break;
        case TapeOp::NOT:         std::cout << "NOT r" << inst.dst << " = ~r" << inst.left; break;
        case TapeOp::SLL:         std::cout << "SLL r" << inst.dst << " = r" << inst.left << " << r" << inst.right; break;
        case TapeOp::SRL:         std::cout << "SRL r" << inst.dst << " = r" << inst.left << " >> r" << inst.right; break;
        case TapeOp::SRA:         std::cout << "SRA r" << inst.dst << " = r" << inst.left << " >>> r" << inst.right; break;
        case TapeOp::SLT:         std::cout << "SLT r" << inst.dst << " = r" << inst.left << " < r" << inst.right; break;
        case TapeOp::SLTU:        std::cout << "SLTU r" << inst.dst << " = r" << inst.left << " < r" << inst.right; break;
        
        case TapeOp::MUL:         std::cout << "MUL r" << inst.dst << " = r" << inst.left << " * r" << inst.right; break;
        case TapeOp::DIV:         std::cout << "DIV r" << inst.dst << " = r" << inst.left << " / r" << inst.right; break;
        case TapeOp::MODULO:      std::cout << "MOD r" << inst.dst << " = r" << inst.left << " % r" << inst.right; break;
        case TapeOp::POW:         std::cout << "POW r" << inst.dst << " = r" << inst.left << " ** r" << inst.right; break;
        case TapeOp::FLOOR_DIV:   std::cout << "FLOOR_DIV r" << inst.dst << " = r" << inst.left << " // r" << inst.right; break;
        case TapeOp::FRAC_DIV:    std::cout << "FRAC_DIV r" << inst.dst << " = frac(r" << inst.left << " / r" << inst.right << ")"; break;
        case TapeOp::UNARY:       std::cout << "NEG r" << inst.dst << " = -r" << inst.left; break;
        
        case TapeOp::LOAD_CONST:  std::cout << "LOAD_CONST r" << inst.dst << " = " << current_consants[inst.left]; break;
        case TapeOp::LOAD_VAR:    std::cout << "LOAD_VAR r" << inst.dst << " = mem[" << inst.left << "]"; break;
        case TapeOp::LOAD_STR:    std::cout << "LOAD_STR r" << inst.dst << " = \"" << current_strings[inst.left] << "\""; break;
        case TapeOp::LOAD_NONE:   std::cout << "LOAD_NONE r" << inst.dst; break;
        case TapeOp::LOAD:        std::cout << "LOAD r" << inst.dst << " = [fp" << ((int8_t)inst.right >= 0 ? "+" : "") << (int)(int8_t)inst.right << "]"; break;
        case TapeOp::STORE:       std::cout << "STORE r" << inst.dst << " -> fp" << ((int8_t)inst.right >= 0 ? "+" : "") << (int)(int8_t)inst.right; break;
        case TapeOp::STORE_VAR:   std::cout << "STORE_VAR r" << inst.right << " -> mem[" << inst.left << "]"; break;
        
        case TapeOp::CMP_GT:      std::cout << "CMP_GT r" << inst.dst << " = r" << inst.left << " > r" << inst.right; break;
        case TapeOp::CMP_LT:      std::cout << "CMP_LT r" << inst.dst << " = r" << inst.left << " < r" << inst.right; break;
        case TapeOp::CMP_GET:     std::cout << "CMP_GET r" << inst.dst << " = r" << inst.left << " >= r" << inst.right; break;
        case TapeOp::CMP_LET:     std::cout << "CMP_LET r" << inst.dst << " = r" << inst.left << " <= r" << inst.right; break;
        case TapeOp::CMP_EQ:      std::cout << "CMP_EQ r" << inst.dst << " = r" << inst.left << " == r" << inst.right; break;
        case TapeOp::CMP_NEQ:     std::cout << "CMP_NEQ r" << inst.dst << " = r" << inst.left << " != r" << inst.right; break;
        
        case TapeOp::JMP:         std::cout << "JMP " << getAddress(inst); break;
        case TapeOp::JZ:          std::cout << "JZ r" << inst.dst << " -> " << getAddress(inst); break;
        case TapeOp::JNZ:         std::cout << "JNZ r" << inst.dst << " -> " << getAddress(inst); break;
        
        case TapeOp::PRINT:       std::cout << "PRINT r" << inst.dst; break;
        case TapeOp::PRINT_STR:   std::cout << "PRINT_STR \"" << current_strings[inst.dst] << "\""; break;
        
        case TapeOp::LOGICAL_AND: std::cout << "LOG_AND r" << inst.dst << " = r" << inst.left << " && r" << inst.right; break;
        case TapeOp::LOGICAL_OR:  std::cout << "LOG_OR r" << inst.dst << " = r" << inst.left << " || r" << inst.right; break;
        case TapeOp::LOGICAL_NOT: std::cout << "LOG_NOT r" << inst.dst << " = !r" << inst.left; break;
        
        case TapeOp::CALL:        std::cout << "CALL r" << inst.dst << " -> " << getAddress(inst); break;
        case TapeOp::RETURN:      std::cout << "RETURN r" << inst.dst; break;
        case TapeOp::PUSH_ARG:    std::cout << "PUSH_ARG r" << inst.dst; break;
        case TapeOp::LOAD_PARAM:  std::cout << "LOAD_PARAM r" << inst.dst << " = arg[" << inst.left << "]"; break;
        
        case TapeOp::INPUT:       std::cout << "INPUT r" << inst.dst; break;
        case TapeOp::LENGTH:      std::cout << "LENGTH r" << inst.dst << " = len(r" << inst.left << ")"; break;
        
        case TapeOp::SIN:         std::cout << "SIN r" << inst.dst << " = sin(r" << inst.left << ")"; break;
        case TapeOp::COS:         std::cout << "COS r" << inst.dst << " = cos(r" << inst.left << ")"; break;
        case TapeOp::TAN:         std::cout << "TAN r" << inst.dst << " = tan(r" << inst.left << ")"; break;
        case TapeOp::ASIN:        std::cout << "ASIN r" << inst.dst << " = asin(r" << inst.left << ")"; break;
        case TapeOp::ACOS:        std::cout << "ACOS r" << inst.dst << " = acos(r" << inst.left << ")"; break;
        case TapeOp::ATAN:        std::cout << "ATAN r" << inst.dst << " = atan(r" << inst.left << ")"; break;
        case TapeOp::ATAN2:       std::cout << "ATAN2 r" << inst.dst << " = atan2(r" << inst.left << ", r" << inst.right << ")"; break;
        case TapeOp::SQRT:        std::cout << "SQRT r" << inst.dst << " = sqrt(r" << inst.left << ")"; break;
        case TapeOp::EXP:         std::cout << "EXP r" << inst.dst << " = exp(r" << inst.left << ")"; break;
        case TapeOp::LOG:         std::cout << "LOG r" << inst.dst << " = log(r" << inst.left << ")"; break;
        case TapeOp::LOG10:       std::cout << "LOG10 r" << inst.dst << " = log10(r" << inst.left << ")"; break;
        case TapeOp::LOG2:        std::cout << "LOG2 r" << inst.dst << " = log2(r" << inst.left << ")"; break;
        case TapeOp::CEIL:        std::cout << "CEIL r" << inst.dst << " = ceil(r" << inst.left << ")"; break;
        case TapeOp::FLOOR:       std::cout << "FLOOR r" << inst.dst << " = floor(r" << inst.left << ")"; break;
        case TapeOp::ABS:         std::cout << "ABS r" << inst.dst << " = abs(r" << inst.left << ")"; break;
        case TapeOp::ROUND:       std::cout << "ROUND r" << inst.dst << " = round(r" << inst.left << ")"; break;
        case TapeOp::FMOD:        std::cout << "FMOD r" << inst.dst << " = fmod(r" << inst.left << ", r" << inst.right << ")"; break;
        case TapeOp::CBRT:        std::cout << "CBRT r" << inst.dst << " = cbrt(r" << inst.left << ")"; break;
        case TapeOp::MATH_POW:    std::cout << "POW r" << inst.dst << " = pow(r" << inst.left << ", r" << inst.right << ")"; break;
        case TapeOp::LOG_AB:      std::cout << "LOG_AB r" << inst.dst << " = log(r" << inst.right << ") / log(r" << inst.left << ")"; break;
        
        case TapeOp::CONST_PI:    std::cout << "CONST_PI r" << inst.dst; break;
        case TapeOp::CONST_E:     std::cout << "CONST_E r" << inst.dst; break;
        
        case TapeOp::ADDI:        std::cout << "ADDI r" << inst.dst << " = r" << inst.left << " + " << (int32_t)(int8_t)inst.right; break;
        case TapeOp::ANDI:        std::cout << "ANDI r" << inst.dst << " = r" << inst.left << " & " << (int32_t)(int8_t)inst.right; break;
        case TapeOp::ORI:         std::cout << "ORI r" << inst.dst << " = r" << inst.left << " | " << (int32_t)(int8_t)inst.right; break;
        case TapeOp::XORI:        std::cout << "XORI r" << inst.dst << " = r" << inst.left << " ^ " << (int32_t)(int8_t)inst.right; break;
        case TapeOp::SLLI:        std::cout << "SLLI r" << inst.dst << " = r" << inst.left << " << " << inst.right; break;
        case TapeOp::SRLI:        std::cout << "SRLI r" << inst.dst << " = r" << inst.left << " >> " << inst.right; break;
        case TapeOp::SRAI:        std::cout << "SRAI r" << inst.dst << " = r" << inst.left << " >>> " << inst.right; break;
        case TapeOp::LW:          std::cout << "LW r" << inst.dst << " = mem[" << inst.left << "]"; break;
        case TapeOp::SW:          std::cout << "SW mem[" << inst.left << "] = r" << inst.dst; break;
        
        case TapeOp::TYPE:        std::cout << "TYPE r" << inst.dst << " = type(r" << inst.left << ")"; break;
        case TapeOp::ORD:         std::cout << "ORD r" << inst.dst << " = ord(r" << inst.left << ")"; break;
        case TapeOp::CHR:         std::cout << "CHR r" << inst.dst << " = chr(r" << inst.left << ")"; break;
        case TapeOp::BIN:         std::cout << "BIN r" << inst.dst << " = bin(r" << inst.left << ")"; break;
        case TapeOp::HEX:         std::cout << "HEX r" << inst.dst << " = hex(r" << inst.left << ")"; break;
        case TapeOp::OCT:         std::cout << "OCT r" << inst.dst << " = oct(r" << inst.left << ")"; break;
        case TapeOp::DEC:         std::cout << "DEC r" << inst.dst << " = dec(r" << inst.left << ")"; break;

        default:                  std::cout << "OP(" << (int)op << ")"; break;
    }
    std::cout << std::endl;
}

void RuntimeEngine::debugPrompt(size_t nextPc, const TapeInstr& nextInst) {
    if (debug_continue) return; // go on without asking
    
    std::cout << "\n--- DEBUG STEP ---\n";
    std::cout << "Next instruction: ";
    printTapeInstrCompact(nextPc, nextInst);
    std::cout << "Registers (non-zero): ";
    for (size_t i = 0; i < registers.size(); ++i) {
        if (i == 2 || i == 8) continue; // sp/fp
        if (!isNone(registers[i]) && (isNumber(registers[i]) ? asNumber(registers[i]) != 0.0 : true)) {
            std::cout << "r" << i << "=" << valueToString(registers[i]) << " ";
        }
    }
    std::cout << "\nCommands: [Enter]=step, c=continue, q=quit, r[n]=print reg n, m<addr>=memory\n> ";
    std::string cmd;
    std::getline(std::cin, cmd);
    
    if (cmd == "c") {
        debug_continue = true;
    } else if (cmd == "q") {
        throw std::runtime_error("Debug quit by user");
    } else if (!cmd.empty() && cmd[0] == 'r') {
        // print specific register
        int regNum = -1;
        if (cmd.size() > 1) regNum = std::stoi(cmd.substr(1));
        if (regNum >= 0 && regNum < (int)registers.size()) {
            std::cout << "r" << regNum << " = " << valueToString(registers[regNum]) << std::endl;
        } else {
            std::cout << "Invalid register\n";
        }
        // after command, stay in prompt (recall)
        debugPrompt(nextPc, nextInst); // recursive, but okay for simplicity
    } else if (!cmd.empty() && cmd[0] == 'm') {
        int addr = -1;
        if (cmd.size() > 1) addr = std::stoi(cmd.substr(1));
        if (addr >= 0 && addr < (int)memory.size()) {
            std::cout << "mem[" << addr << "] = " << valueToString(memory[addr]) << std::endl;
        } else {
            std::cout << "Invalid address\n";
        }
        debugPrompt(nextPc, nextInst);
    } else {
        // step (Enter)
    }
}

double RuntimeEngine::run() {
    size_t pc = 0;
    int lastDest = 0;
    try {

        while(pc < current_program.size()) {
            const auto& inst = current_program[pc];
    
            if(debug_mode) {
                if(pc < current_program.size()) {
                    debugPrompt(pc, current_program[pc]);
                } else {
                    std::cout<<"[DEBUG] Program end reached."<<std::endl;
                }
            }
    
            TapeOp op = static_cast<TapeOp>(inst.op);
            lastDest = inst.dst;
            bool jumped = false;
            switch(op) {
                case TapeOp::LOAD_CONST: registers[inst.dst] = current_consants[inst.left]; break;
                case TapeOp::LOAD_VAR:   registers[inst.dst] = memory[inst.left]; break;
                case TapeOp::LOAD_STR: registers[inst.dst] = current_strings[inst.left]; break;
                case TapeOp::LOAD_NONE: registers[inst.dst] = std::monostate{}; break;
                case TapeOp::STORE_VAR:  memory[inst.left] = registers[inst.right]; break;
                case TapeOp::MOV: registers[inst.dst] = registers[inst.left]; break;
                
                case TapeOp::ADD:  {
                    if(isNone(registers[inst.left]) || isNone(registers[inst.right])) {
                        throw std::runtime_error("Cannot add with empty");
                    }
                    if(isString(registers[inst.left]) || isString(registers[inst.right])) {
                        registers[inst.dst] = valueToString(registers[inst.left]) + valueToString(registers[inst.right]);
                    } else {
                        registers[inst.dst] = asNumber(registers[inst.left]) + asNumber(registers[inst.right]);
                    }
                }
            break;
            case TapeOp::SUB: {
                if(isNone(registers[inst.left]) || isNone(registers[inst.right])) {
                    throw std::runtime_error("Cannot sub with empty");
                }
                registers[inst.dst] = asNumber(registers[inst.left]) - asNumber(registers[inst.right]); 
            }
            break;
            case TapeOp::MUL: {
                if(isNone(registers[inst.left]) || isNone(registers[inst.right])) {
                    throw std::runtime_error("Cannot mul with empty");
                }
                if(isString(registers[inst.left]) && isNumber(registers[inst.right])) {
                    const std::string& str = asString(registers[inst.left]);
                    double count = asNumber(registers[inst.right]);
                    int intCount = static_cast<int>(count);
                    if(intCount < 0) {
                        throw std::runtime_error("Cannot multiply string by negative number");
                    }
                    std::string result;
                    for(int i=0; i<intCount; ++i) {
                        result += str;
                    }
                    registers[inst.dst] = result;
                } else if(isNumber(registers[inst.left]) && isString(registers[inst.right])) {
                    const std::string& str = asString(registers[inst.right]);
                    double count = asNumber(registers[inst.left]);
                    int intCount = static_cast<int>(count);
                    if(intCount < 0) {
                        throw std::runtime_error("Cannot multiply string by negative number");
                    }
                    std::string result;
                    for(int i = 0; i < intCount; ++i) {
                        result += str;
                    }
                    registers[inst.dst] = result;
                } else {
                    registers[inst.dst] = asNumber(registers[inst.left]) * asNumber(registers[inst.right]);
                }
            }   
            break;
            case TapeOp::POW: {
                if(isNone(registers[inst.left]) || isNone(registers[inst.right])) {
                    throw std::runtime_error("Cannot pow with empty");
                }
                registers[inst.dst] = std::pow(asNumber(registers[inst.left]), asNumber(registers[inst.right]));
            }  
            break;
            case TapeOp::DIV: {
                if(isNone(registers[inst.left]) || isNone(registers[inst.right])) {
                    throw std::runtime_error("Cannot div with empty");
                }
                if(asNumber(registers[inst.right]) == 0) throw std::runtime_error("Division by zero");
                registers[inst.dst] = asNumber(registers[inst.left]) / asNumber(registers[inst.right]); 
            }
            break;
            case TapeOp::FLOOR_DIV: {
                if(isNone(registers[inst.left]) || isNone(registers[inst.right])) {
                    throw std::runtime_error("Cannot floor_div with empty");
                }
                if(asNumber(registers[inst.right]) == 0) throw std::runtime_error("Division by zero");
                registers[inst.dst] = std::floor(asNumber(registers[inst.left]) / asNumber(registers[inst.right]));
            }
            break;
            case TapeOp::FRAC_DIV: {
                if(isNone(registers[inst.left]) || isNone(registers[inst.right])) {
                    throw std::runtime_error("Cannot frac_div with empty");
                }
                double l = asNumber(registers[inst.left]);
                double r = asNumber(registers[inst.right]);
                if(r == 0) throw std::runtime_error("Division by zero");
                registers[inst.dst] = l/r - std::floor(l/r);
            }
            break;
            case TapeOp::CONST_PI: {
                registers[inst.dst] = 3.14159265358979323846;
            }
            break;
            case TapeOp::CONST_E: {
                registers[inst.dst] = 2.718281828459045;
            }
            break;
            case TapeOp::UNARY:  
                if (isNone(registers[inst.left]))
                    throw std::runtime_error("Cannot apply unary minus to 'empty'");
                registers[inst.dst] = -asNumber(registers[inst.left]); 
            break;
    
            case TapeOp::MODULO: 
                if (isNone(registers[inst.left]) || isNone(registers[inst.right]))
                    throw std::runtime_error("Cannot use modulo with 'empty'");
                registers[inst.dst] = fromInt32(toInt32(registers[inst.left]) % toInt32(registers[inst.right])); 
            break;
    
            case TapeOp::NOT: {
                if(isNone(registers[inst.left])) {
                    throw std::runtime_error("Cannot apply bitwise NOT to 'empty'");
                }
                int32_t val = toInt32(registers[inst.left]);
                registers[inst.dst] = fromInt32(~val);
            }
            break;
    
            case TapeOp::AND:    
                if (isNone(registers[inst.left]) || isNone(registers[inst.right]))
                    throw std::runtime_error("Cannot use bitwise AND with 'empty'");
                registers[inst.dst] = fromInt32(toInt32(registers[inst.left]) & toInt32(registers[inst.right])); 
            break;
    
            case TapeOp::OR:     
                if (isNone(registers[inst.left]) || isNone(registers[inst.right]))
                    throw std::runtime_error("Cannot use bitwise OR with 'empty'");
                registers[inst.dst] = fromInt32(toInt32(registers[inst.left]) | toInt32(registers[inst.right])); 
            break;
    
            case TapeOp::XOR:    
                if (isNone(registers[inst.left]) || isNone(registers[inst.right]))
                    throw std::runtime_error("Cannot use bitwise XOR with 'empty'");
                registers[inst.dst] = fromInt32(toInt32(registers[inst.left]) ^ toInt32(registers[inst.right])); 
            break;
    
            case TapeOp::SLL:    
                if (isNone(registers[inst.left]) || isNone(registers[inst.right]))
                    throw std::runtime_error("Cannot use left shift with 'empty'");
                registers[inst.dst] = fromInt32(toInt32(registers[inst.left]) << (toInt32(registers[inst.right]) & 0x1F)); 
            break;
    
            case TapeOp::SRL:    
                if (isNone(registers[inst.left]) || isNone(registers[inst.right]))
                    throw std::runtime_error("Cannot use right shift (logical) with 'empty'");
                registers[inst.dst] = fromInt32((uint32_t)toInt32(registers[inst.left]) >> (toInt32(registers[inst.right]) & 0x1F)); 
            break;
    
            case TapeOp::SRA:    
                if (isNone(registers[inst.left]) || isNone(registers[inst.right]))
                    throw std::runtime_error("Cannot use right shift (arithmetic) with 'empty'");
                registers[inst.dst] = fromInt32(toInt32(registers[inst.left]) >> (toInt32(registers[inst.right]) & 0x1F)); 
            break;
    
            case TapeOp::SLT:    
                if (isNone(registers[inst.left]) || isNone(registers[inst.right]))
                    throw std::runtime_error("Cannot compare 'empty' with <");
                registers[inst.dst] = toInt32(registers[inst.left]) < toInt32(registers[inst.right]) ? 1.0 : 0.0; 
            break;
    
            case TapeOp::SLTU:   
                if (isNone(registers[inst.left]) || isNone(registers[inst.right]))
                    throw std::runtime_error("Cannot compare 'empty' with unsigned <");
                registers[inst.dst] = (uint32_t)toInt32(registers[inst.left]) < (uint32_t)toInt32(registers[inst.right]) ? 1.0 : 0.0; 
            break;
    
            case TapeOp::CMP_LT: {
                if(isNone(registers[inst.left]) || isNone(registers[inst.right])) {
                    throw std::runtime_error("Cannot compare empty with <");
                }
                registers[inst.dst] = asNumber(registers[inst.left]) < asNumber(registers[inst.right]) ? 1.0 : 0.0;
            }
            break;
            case TapeOp::CMP_GT: {
                if(isNone(registers[inst.left]) || isNone(registers[inst.right])) {
                    throw std::runtime_error("Cannot compare empty with >");
                }
                registers[inst.dst] = asNumber(registers[inst.left]) >  asNumber(registers[inst.right]) ? 1.0 : 0.0; 
            }
            break;
            case TapeOp::CMP_GET: {
                if(isNone(registers[inst.left]) || isNone(registers[inst.right])) {
                    throw std::runtime_error("Cannot compare empty with >=");
                }
                registers[inst.dst] = asNumber(registers[inst.left]) >= asNumber(registers[inst.right]) ? 1.0 : 0.0;
            }
            break;
            case TapeOp::CMP_LET: {
                if(isNone(registers[inst.left]) || isNone(registers[inst.right])) {
                    throw std::runtime_error("Cannot compare empty with <=");
                }
                registers[inst.dst] = asNumber(registers[inst.left]) <= asNumber(registers[inst.right]) ? 1.0 : 0.0; break;
            }
            case TapeOp::CMP_EQ:  registers[inst.dst] = registers[inst.left] == registers[inst.right] ? 1.0 : 0.0; break;
            case TapeOp::CMP_NEQ: registers[inst.dst] = registers[inst.left] != registers[inst.right] ? 1.0 : 0.0; break;
            case TapeOp::JZ: {
                    if(isFalsy(registers[inst.dst])) {
                        pc = getAddress(inst);
                        jumped = true;
                }
            }
            break;
            case TapeOp::JMP: pc = getAddress(inst); jumped = true;
            break;
            case TapeOp::PRINT: {
                const Value& val = registers[inst.dst];
                if (isString(val)) {
                    std::cout << asString(val);
                }
                else if (isNumber(val)) {
                    double num = asNumber(val);
                    if (num == (long long)num) {
                        std::cout << (long long)num;
                    } else {
                        std::cout << num;
                    }
                }
                else {
                    std::cout << valueToString(val);
                }
                break;
            }
            case TapeOp::PRINT_STR: std::cout<<current_strings[inst.dst]; break;
            
            case TapeOp::LOGICAL_AND: registers[inst.dst] = (isTruthy(registers[inst.left]) && isTruthy(registers[inst.right])) ? 1.0 : 0.0; break;
            case TapeOp::LOGICAL_OR: registers[inst.dst] = (isTruthy(registers[inst.left]) || isTruthy(registers[inst.right])) ? 1.0 : 0.0; break;
            case TapeOp::LOGICAL_NOT: registers[inst.dst] = isFalsy(registers[inst.left]) ? 1.0 : 0.0; break;
    
            case TapeOp::CALL: {
                size_t retAddr = pc + 1;
                uint16_t funcAddr = getAddress(inst);
                callStack.push({retAddr, inst.dst, registers[2], registers[8], argBuffer, registers});
                argBuffer.clear();
    
                pc = funcAddr;
                jumped = true;
            }
            break;
    
            case TapeOp::LENGTH: {
                const Value& val = registers[inst.left];
                if(isString(val)) {
                    registers[inst.dst] = static_cast<double>(asString(val).size());
                } else {
                    throw std::runtime_error("length() excepts a string argument");
                }
            }
            break;
    
            case TapeOp::TYPE: {
                const Value& val = registers[inst.left];
                if(isString(val)) {
                    registers[inst.dst] = std::string("string");
                } else if(isNumber(val)) {
                    registers[inst.dst] = std::string("number");
                } else if(isNone(val)) {
                    registers[inst.dst] = std::string("empty");
                } else {
                    registers[inst.dst] = std::string("unknown");
                }
            }
            break;
    
        case TapeOp::ORD: {
            const Value& val = registers[inst.left];
            if (isString(val)) {
                const std::string& str = asString(val);
                if (str.empty()) {
                    registers[inst.dst] = 0.0;
                } else {
                    // UTF-8 decode first code point
                    unsigned char c = str[0];
                    int codePoint = c;

                    if ((c & 0xE0) == 0xC0 && str.size() >= 2) {
                        // 2-byte sequence
                        codePoint = ((c & 0x1F) << 6) | (str[1] & 0x3F);
                    } else if ((c & 0xF0) == 0xE0 && str.size() >= 3) {
                        // 3-byte sequence
                        codePoint = ((c & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
                    } else if ((c & 0xF8) == 0xF0 && str.size() >= 4) {
                        // 4-byte sequence
                        codePoint = ((c & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
                    }

                    registers[inst.dst] = static_cast<double>(codePoint);
                }
            } else {
                throw std::runtime_error("ord() expects a string argument");
            }
        }
        break;
    
            case TapeOp::CHR: {
                if(isNone(registers[inst.dst])) throw std::runtime_error("chr() expects a number argument, got empty");
                int code = static_cast<int>(asNumber(registers[inst.left]));
    
                if(code < 0 || code > 255) {
                    throw std::runtime_error("chr() argument out of range (0-255)");
                }
                std::string result;
                if(code <= 0x7F) { // 1 byte
                    result = static_cast<char>(code);
                } else if(code <= 0x7FF) { // 2 bytes
                    result += static_cast<char>(0xC0 | (code >> 6));
                    result += static_cast<char>(0x80 | (code & 0x3F));
                } else if(code <= 0x7FFF) { // 3 bytes
                    result += static_cast<char>(0xE0 | (code >> 12));
                    result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (code & 0x3F));
                } else if(code <= 0x7FFFF) { // 4 bytes
                    result += static_cast<char>(0xF0 | (code >> 18));
                    result += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
                    result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (code & 0x3F));
                }
                registers[inst.dst] = result;
            }
            break;
    
            case TapeOp::BIN: {
                if(isNone(registers[inst.left])) throw std::runtime_error("bin() expects a number");
                int value = toInt32(registers[inst.left]);
                std::string result;
                if(value == 0) result = "0";
                else {
                    unsigned int u = static_cast<unsigned int>(value);
                    while(u > 0) {
                        result = (char)('0' + (u % 2)) + result;
                        u /= 2;
                    }
                }
                registers[inst.dst] = "0b" + result;
            }
            break;
    
            case TapeOp::HEX: {
                if(isNone(registers[inst.left])) throw std::runtime_error("hex() expects a number");
                int val = toInt32(registers[inst.left]);
                if(val == 0) {
                    registers[inst.dst] = "0x0";
                } else {
                    std::stringstream ss;
                    ss << "0x" << std::hex << val;
                    registers[inst.dst] = ss.str();
                }
            }
            break;
    
            case TapeOp::OCT: {
                if(isNone(registers[inst.left])) throw std::runtime_error("oct() expects a number");
                int val = toInt32(registers[inst.left]);
                if(val == 0) {
                    registers[inst.dst] = "0o0";
                } else {
                    std::stringstream ss;
                    ss << "0o" << std::oct << val;
                    registers[inst.dst] = ss.str();
                }
            }
            break;
    
            case TapeOp::DEC: {
                Value& val = registers[inst.left];
                if(isString(val)) {
                    const std::string& str = asString(val);
                    try {
                        long long result = 0;
                        bool negative = false;
                        std::string numStr = str;
    
                        if(numStr[0] == '-') {
                            negative = true;
                            numStr = numStr.substr(1);
                        }
                        // auto-detect base
                        if(numStr.size() > 2 && numStr[0] == '0' && (numStr[1] == 'b' || numStr[1] == 'B')) {
                            result = std::stoll(numStr.substr(2), nullptr, 2);
                        } else if(numStr.size() > 2 && numStr[0] == '0' && (numStr[1] == 'o' || numStr[1] == 'O')) {
                            result = std::stoll(numStr.substr(2), nullptr, 8);
                        } else if(numStr.size() > 2 && numStr[0] == '0' && (numStr[1] == 'x' || numStr[1] == 'X')) {
                            result = std::stoll(numStr.substr(2), nullptr, 16);
                        } else {
                            result = std::stoll(numStr);
                        }
    
                        if(negative) result = -result;
                        registers[inst.dst] = static_cast<double>(result);
                    } catch(...) {
                        registers[inst.dst] = 0.0;
                    }
                } else if(isNumber(val)) {
                    registers[inst.dst] = asNumber(val);
                } else {
                    registers[inst.dst] = 0.0;
                }
            }
            break;
    
            case TapeOp::INPUT: {
                std::cout << std::flush;
                std::string inputStr;
    
                if (!std::getline(std::cin, inputStr)) {
                    registers[inst.dst] = "";
                    break;
                }
    
                if (!inputStr.empty() && inputStr.back() == '\r') {
                    inputStr.pop_back();
                }
    
                size_t start = inputStr.find_first_not_of(" \t");
                if (start == std::string::npos) {
                    registers[inst.dst] = "";
                    break;
                }
    
                const char* str = inputStr.c_str() + start;
                char* endPtr;
                double numValue = std::strtod(str, &endPtr);
    
                while (*endPtr == ' ' || *endPtr == '\t') {
                    ++endPtr;
                }
    
                if (endPtr != str && *endPtr == '\0') {
                    registers[inst.dst] = numValue;
                } else {
                    registers[inst.dst] = inputStr;
                }
            }
            break;
            case TapeOp::SIN:   registers[inst.dst] = std::sin(asNumber(registers[inst.left])); break;
            case TapeOp::COS:   registers[inst.dst] = std::cos(asNumber(registers[inst.left])); break;
            case TapeOp::TAN:   registers[inst.dst] = std::tan(asNumber(registers[inst.left])); break;
            case TapeOp::ASIN:  registers[inst.dst] = std::asin(asNumber(registers[inst.left])); break;
            case TapeOp::ACOS:  registers[inst.dst] = std::acos(asNumber(registers[inst.left])); break;
            case TapeOp::ATAN:  registers[inst.dst] = std::atan(asNumber(registers[inst.left])); break;
            case TapeOp::ATAN2: registers[inst.dst] = std::atan2(asNumber(registers[inst.left]), asNumber(registers[inst.right])); break;
            case TapeOp::SQRT:  registers[inst.dst] = std::sqrt(asNumber(registers[inst.left])); break;
            case TapeOp::EXP:   registers[inst.dst] = std::exp(asNumber(registers[inst.left])); break;
            case TapeOp::LOG:   registers[inst.dst] = std::log(asNumber(registers[inst.left])); break;
            case TapeOp::LOG10: registers[inst.dst] = std::log10(asNumber(registers[inst.left])); break;
            case TapeOp::CEIL:  registers[inst.dst] = std::ceil(asNumber(registers[inst.left])); break;
            case TapeOp::FLOOR: registers[inst.dst] = std::floor(asNumber(registers[inst.left])); break;
            case TapeOp::ABS:   registers[inst.dst] = std::fabs(asNumber(registers[inst.left])); break;
            case TapeOp::ROUND: registers[inst.dst] = std::round(asNumber(registers[inst.left])); break;
            case TapeOp::FMOD:  registers[inst.dst] = std::fmod(asNumber(registers[inst.left]), asNumber(registers[inst.right])); break;
            case TapeOp::CBRT: registers[inst.dst] = std::cbrt(asNumber(registers[inst.left])); break;
            case TapeOp::MATH_POW: registers[inst.dst] = std::pow(asNumber(registers[inst.left]), asNumber(registers[inst.right])); break;
            case TapeOp::LOG2: registers[inst.dst] = std::log2(asNumber(registers[inst.left])); break;
            case TapeOp::LOG_AB: registers[inst.dst] 
                = std::log(asNumber(registers[inst.right])) / std::log(asNumber(registers[inst.left])); break;
    
            case TapeOp::RETURN: {
                    Value retVal = registers[inst.dst];
                    if(callStack.empty()) break;
                    CallFrame frame = callStack.top();
                    callStack.pop();
                    registers = frame.callerRegisters;
                    registers[2] = frame.callerSp;
                    registers[8] = frame.callerFp;
                    registers[frame.returnDest] = retVal;
                    pc = frame.returnAddress;
                    jumped = true;
            }
            break;
            
            case TapeOp::PUSH_ARG:
                argBuffer.push_back(registers[inst.dst]);
            break;
            case TapeOp::LOAD_PARAM: {
                if(!callStack.empty()) {
                    const auto& args = callStack.top().args;
                    registers[inst.dst] = inst.left < args.size() ? args[inst.left] : Value(0.0);
                }
            }
            break;
    
            case TapeOp::LOAD: {
                int32_t base   = (int32_t)asNumber(registers[inst.left]); // FP
                int32_t offset = static_cast<int8_t>(inst.right);
                int32_t addr   = base + offset;
    
                if (addr < 0 || addr >= (int32_t)memory.size()) {
                    std::cerr << "LOAD out of range: addr=" << addr << std::endl;
                    throw std::runtime_error("LOAD out of range");
                }
                registers[inst.dst] = memory[addr];
                break;
            }
    
            case TapeOp::STORE: {
                int32_t base   = (int32_t)asNumber(registers[inst.left]); // FP
                int32_t offset = static_cast<int8_t>(inst.right);
                int32_t addr   = base + offset;
    
                if (addr < 0 || addr >= (int32_t)memory.size()) {
                    std::cerr << "STORE out of range: addr=" << addr << std::endl;
                    throw std::runtime_error("STORE out of range");
                }
                memory[addr] = registers[inst.dst];
                break;
            }
    
            case TapeOp::ADDI: {
                int8_t signedImm = static_cast<int8_t>(inst.right);
                int32_t imm = signedImm;
                registers[inst.dst] = fromInt32(toInt32(registers[inst.left]) + imm);
                break;
            }
            case TapeOp::ANDI: {
                int8_t signedImm = static_cast<int8_t>(inst.right);
                registers[inst.dst] = fromInt32(toInt32(registers[inst.left]) & signedImm);
                break;
            }
            case TapeOp::ORI: {
                int8_t signedImm = static_cast<int8_t>(inst.right);
                registers[inst.dst] = fromInt32(toInt32(registers[inst.left]) | signedImm);
                break;
            }
            case TapeOp::XORI: {
                int8_t signedImm = static_cast<int8_t>(inst.right);
                registers[inst.dst] = fromInt32(toInt32(registers[inst.left]) ^ signedImm);
                break;
            }
            case TapeOp::SLLI: {
                registers[inst.dst] = fromInt32(toInt32(registers[inst.left]) << (inst.right & 0x1F));
                break;
            }
            case TapeOp::SRLI: {
                registers[inst.dst] = fromInt32((uint32_t)toInt32(registers[inst.left]) >> (inst.right & 0x1F));
                break;
            }
            case TapeOp::SRAI: {
                registers[inst.dst] = fromInt32(toInt32(registers[inst.left]) >> (inst.right & 0x1F));
                break;
            }
            case TapeOp::LW:
                registers[inst.dst] = memory[inst.left];
                break;
            case TapeOp::SW:
                memory[inst.left] = registers[inst.dst];
                break;
            default: break;
        }
        if(!jumped) pc++;
    }
    if(isNumber(registers[lastDest])) {
            return asNumber(registers[lastDest]) == -0.0 ? 0.0 : asNumber(registers[lastDest]);
    }
    return 0.0;
} catch(const std::exception& e) {
    int line = (pc < current_lineNumbers.size() ? current_lineNumbers[pc] : 0);
    throw std::runtime_error(
        "Line " + std::to_string(line) + ": " + e.what()
    );
}
}

void RuntimeEngine::visualize(const std::vector<TapeInstr>& program) const {
    std::cout << "\n[VM Bytecode Visualization]" << std::endl;
    std::cout << std::left << std::setw(6)  << "Addr"
              << std::setw(12) << "TapeOp" << std::setw(6)  << "L" 
              << std::setw(6)  << "R" << std::setw(6)  << "Dst" << " Value" << std::endl; 
    std::cout << std::string(50, '-') << std::endl;

    for(size_t i = 0; i < program.size(); ++i) {
        const auto& inst = program[i];
        TapeOp op = static_cast<TapeOp>(inst.op);

        std::cout << "[" << std::setw(3) << i << "]  ";
        std::cout << std::left << std::setw(12);

        switch(op) {
            case TapeOp::LOAD_CONST:
                std::cout << "LOAD_CONST" << std::setw(6) << "-" << std::setw(6) << "-"
                          << std::setw(6) << inst.dst << current_consants[inst.left];
                break;

            case TapeOp::LOAD_VAR:
                std::cout << "LOAD_VAR" << std::setw(6) << inst.left << std::setw(6) << "-"
                          << std::setw(6) << inst.dst;
                break;

            case TapeOp::LOAD_STR:
                std::cout << "LOAD_STR" << std::setw(6) << inst.left << std::setw(6) << "-"
                          << std::setw(6) << inst.dst << "\"" << current_strings[inst.left] << "\"";
                break;
            
            case TapeOp::LOAD_NONE:
                std::cout << "LOAD_NONE" << std::setw(6) << inst.dst;
                break;
            case TapeOp::STORE_VAR:
                std::cout << "STORE_VAR" << std::setw(6) << inst.left << std::setw(6) << inst.right
                          << std::setw(6) << "-" << " mem[" << inst.left << "] = r" << inst.right;
                break;

            case TapeOp::ADD:     std::cout << "ADD";     break;
            case TapeOp::MOV:     std::cout << "MOV";     break;
            case TapeOp::SUB:     std::cout << "SUB";     break;
            case TapeOp::MUL:     std::cout << "MUL";     break;
            case TapeOp::DIV:     std::cout << "DIV";     break;
            case TapeOp::FLOOR_DIV: std::cout << "FLOOR_DIV"; break;
            case TapeOp::FRAC_DIV: std::cout << "FRAC_DIV"; break;
            case TapeOp::POW:     std::cout << "POW";     break;
            case TapeOp::MODULO:  std::cout << "MODULO";  break;
            case TapeOp::UNARY:   std::cout << "NEG";     break;

            case TapeOp::NOT:     std::cout << "NOT";     break;
            case TapeOp::AND:     std::cout << "AND";     break;
            case TapeOp::OR:      std::cout << "OR";      break;
            case TapeOp::XOR:     std::cout << "XOR";     break;
            case TapeOp::SLL:     std::cout << "SLL";     break;
            case TapeOp::SRL:     std::cout << "SRL";     break;
            case TapeOp::SRA:     std::cout << "SRA";     break;
            case TapeOp::SLT:     std::cout << "SLT";     break;
            case TapeOp::SLTU:    std::cout << "SLTU";    break;

            case TapeOp::CMP_GT:  std::cout << "CMP_GT";  break;
            case TapeOp::CMP_LT:  std::cout << "CMP_LT";  break;
            case TapeOp::CMP_GET: std::cout << "CMP_GET"; break;
            case TapeOp::CMP_LET: std::cout << "CMP_LET"; break;
            case TapeOp::CMP_EQ:  std::cout << "CMP_EQ";  break;
            case TapeOp::CMP_NEQ: std::cout << "CMP_NEQ"; break;

            case TapeOp::LOGICAL_AND:
                std::cout << "LOGICAL_AND" << std::setw(6) << inst.left 
                          << std::setw(6) << inst.right << std::setw(6) << inst.dst;
                break;
            case TapeOp::LOGICAL_OR:
                std::cout << "LOGICAL_OR" << std::setw(6) << inst.left 
                          << std::setw(6) << inst.right << std::setw(6) << inst.dst;
                break;
            case TapeOp::LOGICAL_NOT:
                std::cout << "LOGICAL_NOT" << std::setw(6) << inst.left 
                          << std::setw(6) << "-" << std::setw(6) << inst.dst;
                break;

            case TapeOp::JZ:
                std::cout << "JZ" << std::setw(6) << inst.dst << " TO ADDR: " << getAddress(inst);
                break;
            case TapeOp::JMP:
                std::cout << "JMP" << std::setw(6) << inst.dst << " TO: " << getAddress(inst);
                break;

            case TapeOp::PRINT:
                std::cout << "PRINT" << std::setw(6) << inst.dst;
                break;

            case TapeOp::PRINT_STR:
                std::cout<<"PRINT_STR" << std::setw(6) << inst.dst;
                break;

            case TapeOp::CALL:
                std::cout << "CALL" << std::setw(6) << "-" << std::setw(6) << "-"
                          << std::setw(6) << inst.dst << " ADDR: " << getAddress(inst);
                break;

            case TapeOp::LENGTH:
                std::cout << "LENGTH" << std::setw(6) << inst.left << std::setw(6) << "-"
                          << std::setw(6) << inst.dst;
            break;
            case TapeOp::SIN:   std::cout << "SIN"; break;
            case TapeOp::COS:   std::cout << "COS"; break;
            case TapeOp::TAN:   std::cout << "TAN"; break;
            case TapeOp::ASIN:  std::cout << "ASIN"; break;
            case TapeOp::ACOS:  std::cout << "ACOS"; break;
            case TapeOp::ATAN:  std::cout << "ATAN"; break;
            case TapeOp::ATAN2: std::cout << "ATAN2"; break;
            case TapeOp::SQRT:  std::cout << "SQRT"; break;
            case TapeOp::EXP:   std::cout << "EXP"; break;
            case TapeOp::LOG:   std::cout << "LOG"; break;
            case TapeOp::LOG10: std::cout << "LOG10"; break;
            case TapeOp::CEIL:  std::cout << "CEIL"; break;
            case TapeOp::FLOOR: std::cout << "FLOOR"; break;
            case TapeOp::ABS:   std::cout << "ABS"; break;
            case TapeOp::ROUND: std::cout << "ROUND"; break;
            case TapeOp::FMOD:  std::cout << "FMOD"; break;

            case TapeOp::CONST_PI:
                std::cout << "LOAD_CONST PI" << std::setw(6);
            break;

            case TapeOp::CONST_E:
                std::cout << "LOAD_CONST E" << std::setw(6);
            break;

            case TapeOp::RETURN:
                std::cout << "RETURN" << std::setw(6) << inst.dst << std::setw(6) << "-" << std::setw(6) << "-";
                break;

            case TapeOp::PUSH_ARG:
                std::cout << "PUSH_ARG" << std::setw(6) << inst.dst << std::setw(6) << "-" << std::setw(6) << "-";
                break;

            case TapeOp::LOAD_PARAM:
                std::cout << "LOAD_PARAM" << std::setw(6) << inst.left << std::setw(6) << "-" 
                          << std::setw(6) << inst.dst;
                break;

            case TapeOp::LOAD:
                std::cout << "LOAD" << std::setw(6) << inst.left << std::setw(6) << inst.right 
                          << std::setw(6) << inst.dst;
                break;

            case TapeOp::STORE:
                std::cout << "STORE" << std::setw(6) << inst.left << std::setw(6) << inst.right 
                          << std::setw(6) << inst.dst;
                break;

            case TapeOp::ADDI:
                std::cout << "ADDI" 
                          << std::setw(6) << inst.left 
                          << std::setw(6) << inst.right 
                          << std::setw(6) << inst.dst 
                          << " (imm=" << (int32_t)inst.right << ")";
                break;
            case TapeOp::ANDI:
                std::cout << "ANDI" << std::setw(6) << inst.left << std::setw(6) << inst.right
                          << std::setw(6) << inst.dst;
                break;
            case TapeOp::ORI:
                std::cout << "ORI" << std::setw(6) << inst.left << std::setw(6) << inst.right
                          << std::setw(6) << inst.dst;
                break;
            case TapeOp::XORI:
                std::cout << "XORI" << std::setw(6) << inst.left << std::setw(6) << inst.right
                          << std::setw(6) << inst.dst;
                break;
            case TapeOp::SLLI:
                std::cout << "SLLI" << std::setw(6) << inst.left << std::setw(6) << inst.right
                          << std::setw(6) << inst.dst;
                break;
            case TapeOp::SRLI:
                std::cout << "SRLI" << std::setw(6) << inst.left << std::setw(6) << inst.right
                          << std::setw(6) << inst.dst;
                break;
            case TapeOp::SRAI:
                std::cout << "SRAI" << std::setw(6) << inst.left << std::setw(6) << inst.right
                          << std::setw(6) << inst.dst;
                break;
            case TapeOp::LW:
                std::cout << "LW" << std::setw(6) << inst.left << std::setw(6) << "-"
                          << std::setw(6) << inst.dst;
                break;
            case TapeOp::SW:
                std::cout << "SW" << std::setw(6) << inst.left << std::setw(6) << "-"
                          << std::setw(6) << inst.dst;
                break;

            case TapeOp::TYPE: std::cout<<"TYPE";   break;
            case TapeOp::ORD:  std::cout << "ORD r" << inst.dst << " = ord(r" << inst.left << ")"; break;
            case TapeOp::CHR:  std::cout << "CHR r" << inst.dst << " = chr(r" << inst.left << ")"; break;
            case TapeOp::BIN:  std::cout << "BIN";     break;
            case TapeOp::HEX:  std::cout << "HEX";     break;
            case TapeOp::OCT:  std::cout << "OCT";     break;
            case TapeOp::DEC:  std::cout << "DEC";     break;

            default:
                std::cout << "UNKNOWN (op=" << (int)op << ")";
                break;
        }
        std::cout << std::endl;
    }
}