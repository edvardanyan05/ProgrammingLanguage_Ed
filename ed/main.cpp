#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <regex>
#include <unordered_set>
#include <filesystem>
#include "source_scan.hpp"
#include "lexicon.hpp"
#include "phrase_book.hpp"
#include "stack_emitter.hpp"
#include "binding_table.hpp"
#include "tape_runner.hpp"

namespace {
std::string readAllText(const std::filesystem::path& path) {
    std::ifstream file(path);
    if(!file.is_open()) {
        throw std::runtime_error("Cannot open file '" + path.string() + "'");
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string expandUses(
    const std::filesystem::path& path,
    std::unordered_set<std::string>& visiting,
    std::unordered_set<std::string>& seen) {
    const std::filesystem::path normalized = std::filesystem::absolute(path).lexically_normal();
    const std::string key = normalized.generic_string();
    if(visiting.count(key) > 0) {
        throw std::runtime_error("Circular use detected at '" + normalized.string() + "'");
    }
    if(seen.count(key) > 0) {
        return "";
    }

    visiting.insert(key);
    const std::string source = readAllText(normalized);
    std::istringstream sourceLines(source);
    std::ostringstream merged;
    std::string line;
    const std::regex usePattern(R"re(^\s*use\s+['"]([^'"]+)['"]\s*;?\s*$)re");

    while(std::getline(sourceLines, line)) {
        std::smatch match;
        if(std::regex_match(line, match, usePattern)) {
            const std::filesystem::path next = (normalized.parent_path() / match[1].str()).lexically_normal();
            merged << expandUses(next, visiting, seen);
            continue;
        }
        merged << line << '\n';
    }

    visiting.erase(key);
    seen.insert(key);
    return merged.str();
}

TapeBundle compileWithUses(const std::string& inputPath) {
    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> seen;
    const std::string source = expandUses(inputPath, visiting, seen);
    std::istringstream stream(source);
    SourceScan scan(stream);
    Lexicon words(scan);
    BindingTable bindings;
    PhraseBook phrases(words, bindings);
    auto root = std::static_pointer_cast<SynNode>(phrases.parseProgram());
    if(!root) {
        throw std::runtime_error("Parsing failed for '" + inputPath + "'");
    }
    Emitter emitter(bindings);
    return emitter.compile(root);
}
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cerr << "ED — calculator-style language\n"
                  << "Usage:\n"
                  << "  " << argv[0] << " <file.ed>\n"
                  << "  " << argv[0] << " compile <input.ed> [output.edb]\n"
                  << "  " << argv[0] << " run <input.edb> [--debug]\n";
        return 1;
    }

    try {
        std::string modeOrFile = argv[1];

        if(modeOrFile == "compile") {
            if(argc < 3) {
                throw std::runtime_error("compile mode requires a .ed source file");
            }
            const std::string inputPath = argv[2];
            if(inputPath.size() < 4 || inputPath.substr(inputPath.size() - 3) != ".ed") {
                throw std::runtime_error("compile input must use extension .ed");
            }
            std::string outputPath = (argc >= 4)
                ? std::string(argv[3])
                : inputPath.substr(0, inputPath.size() - 3) + ".edb";

            TapeBundle bc = compileWithUses(inputPath);
            writeTapeBundleToFile(bc, outputPath);
            std::cout << "Tape bundle written to: " << outputPath << std::endl;
            return 0;
        }

        if(modeOrFile == "run") {
            if(argc < 3) {
                throw std::runtime_error("run mode requires a .edb tape file");
            }
            bool debugFlag = false;
            std::string inputTape;
            for(int i = 2; i < argc; ++i) {
                std::string arg = argv[i];
                if(arg == "--debug") {
                    debugFlag = true;
                } else {
                    inputTape = arg;
                }
            }
            RuntimeEngine engine(debugFlag);
            engine.loadFromFile(inputTape);
            engine.run();
            return 0;
        }

        std::string filename = modeOrFile;
        if(filename.size() < 4 || filename.substr(filename.size() - 3) != ".ed") {
            throw std::runtime_error("Expected a .ed source file, or use compile / run subcommands");
        }
        TapeBundle bc = compileWithUses(filename);
        RuntimeEngine engine(false);
        engine.load(bc);
        engine.run();
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
