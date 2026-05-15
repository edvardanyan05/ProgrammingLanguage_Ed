#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include "stack_emitter.hpp"
#include "binding_table.hpp"
#include "source_scan.hpp"
#include "lexicon.hpp"
#include "phrase_book.hpp"

struct CallFrame {
    size_t returnAddress;
    size_t returnDest;
    Value callerSp;
    Value callerFp;
    std::vector<Value> args;
    std::vector<Value> callerRegisters;
};

class RuntimeEngine {
    private:
        std::vector<Value> registers;
        std::vector<Value> memory;
        std::vector<std::string> current_strings;
        std::vector<TapeInstr> current_program;
        std::vector<double> current_consants;
        bool debug_mode;
        void visualize(const std::vector<TapeInstr>& program) const;
        void loadTapeBundle(const TapeBundle& bc);
        std::stack<CallFrame> callStack;
        std::vector<Value> argBuffer;
        std::vector<int> current_lineNumbers;

        // // Debugger
        bool debug_step_mode = false;
        bool debug_continue = false;

        void debugPrompt(size_t pc, const TapeInstr& inst);
        void printTapeInstrCompact(size_t pc, const TapeInstr& inst) const;
    public:
        RuntimeEngine(bool dm = false) : debug_mode(dm) {
            registers.resize(256, 0.0);
            memory.resize(20000, 0.0);
        }
        void load(const std::string& expr, BindingTable& st);
        void load(const TapeBundle& bc);
        void loadFromFile(const std::string& byteCodePath);
        double run();
};