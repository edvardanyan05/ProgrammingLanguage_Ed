#include "source_scan.hpp"

SourceScan::SourceScan(std::istream& s) : stream(s) {
    advance();
}
void SourceScan::advance() {
    if(currentChar == '\n') {
        lineNumber++;
    }
    currentChar = stream.get();
}

int SourceScan::peek() const {
    return currentChar;
}

bool SourceScan::isEOF() const {
    return currentChar == EOF;
}

