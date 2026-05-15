#pragma once
#include <vector>
#include <stack>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <string>
#include <map>
#include <unordered_map>
#include "syntax.hpp"
#include "binding_table.hpp"

struct TapeInstr {
    uint32_t op:    8;
    uint32_t dst:   8;
    uint32_t left:  8;
    uint32_t right: 8;
};

inline uint16_t getAddress(const TapeInstr& inst) {
    return (uint16_t)((inst.right << 8) | inst.left);
}
inline void setAddress(TapeInstr& inst, uint16_t addr) {
    inst.left = addr & 0xFF;
    inst.right = (addr >> 8) & 0xFF;
}

struct RoutineMeta {
    size_t address;
    int paramCount;
};

struct EmitScratch {
    std::map<size_t, int> vars;
    std::map<double, int> consts;
};

struct TapeBundle {
    std::vector<TapeInstr> instructions;
    std::vector<double> constants;
    std::vector<std::string> strings;
    std::vector<int> lineNumbers;
    std::unordered_map<std::string, size_t> functionSymbols;
    std::vector<std::pair<size_t, std::string>> unresolvedCalls;
};

class Emitter {
    private:
        BindingTable& symTable;
        int nextTempIndex = 0;
        std::stack<int> freeRegisters;
        EmitScratch globalCtx;
        std::vector<double> constantPool;
        std::vector<std::string> stringPool;
        // Function Table
        std::unordered_map<std::string, RoutineMeta> functionTable;

        // Forward declarations
        std::vector<std::pair<size_t, std::string>> forwardCalls;
        int allocateTempRegister();
        void freeTempRegister(int reg);
        void emitMainPrologue(std::vector<TapeInstr>& code);

        std::vector<std::shared_ptr<SynNode>> postOrderTraverse(std::shared_ptr<SynNode> root);
        std::vector<TapeInstr> generateTapeBundle(const std::vector<std::shared_ptr<SynNode>>& nodes);
        bool tryEmitMathBuiltinCall(
            const std::string& name,
            const std::vector<std::shared_ptr<SynNode>>& args,
            std::vector<TapeInstr>& code,
            int& resultReg
        );
        std::stack<std::vector<size_t>> breakStack;
        std::stack<std::vector<size_t>> continueStack;

        std::vector<int> lineNumbers;
        void addLineNumbers(int line, size_t count) {
            lineNumbers.insert(lineNumbers.end(), count, line);
        }
public:
    Emitter(BindingTable& st) : symTable(st) {}
    TapeBundle compile(
        std::shared_ptr<SynNode> root,
        bool allowUnresolvedCalls = false,
        bool emitMainFramePrologue = true
    );
    void printTapeBundle(const std::vector<TapeInstr>& code) const;
    std::shared_ptr<SynNode> optimize(std::shared_ptr<SynNode> node);
    void compileStatement(std::shared_ptr<Stmt> stmt, std::vector<TapeInstr>& code);
    const std::vector<std::string>& getStringPool() const {
        return stringPool;
    }
};

void writeTapeBundleToFile(const TapeBundle& bc, const std::string& path);
TapeBundle readTapeBundleFromFile(const std::string& path);