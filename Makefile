# ED language interpreter — build with GNU Make + g++ (C++17)
# Windows: use "mingw32-make" or "make" from MSYS2 / Git Bash if "make" is not in PATH.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CPPFLAGS ?= -Ied

# On Windows, g++ typically writes ed.exe when asked for -o ed; use explicit .exe for clarity.
ifeq ($(OS),Windows_NT)
  TARGET := ed.exe
else
  TARGET := ed_run
endif

SOURCES := \
	ed/main.cpp \
	ed/stack_emitter.cpp \
	ed/syntax.cpp \
	ed/source_scan.cpp \
	ed/lexicon.cpp \
	ed/phrase_book.cpp \
	ed/tape_runner.cpp

.PHONY: all clean run compile-example help

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $(SOURCES)

clean:
	rm -f ed.exe $(TARGET)

compile-example: $(TARGET)
	./$(TARGET) compile ed_examples/program.ed

run: $(TARGET)
	./$(TARGET) ed_examples/program.ed

help:
	@echo "ED interpreter — targets:"
	@echo "  make          Build $(TARGET) (same as: make all)"
	@echo "  make clean    Remove built executable"
	@echo "  make run      Run ed_examples/program.ed (needs sh-style path; or run manually)"
	@echo "  make compile-example  Build program.ed -> program.edb"
	@echo "PowerShell (repo root): .\\ed.exe ed_examples\\program.ed"
