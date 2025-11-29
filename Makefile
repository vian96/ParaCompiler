# Compiler Settings
CXX := g++

# CRITICAL FIXES:
# 1. -include vector -include string: Fixes the "could not convert" error in ancient ANTLR code.
# 2. -Wno-attributes: Silences the ANTLR4CPP_PUBLIC warnings.
# 3. -std=c++17: C++23 is too strict for ANTLR 4.7.2 (apt version). C++17 is sufficient.
CXXFLAGS := -std=c++17 -g -Wall -Wextra \
            -Isrc \
            -Isrc/generated \
            -I/usr/include/antlr4-runtime \
            -include vector \
            -include string \
            -Wno-attributes

LDFLAGS := -lantlr4-runtime

# Directories
SRC_DIR := src
GRAMMAR_DIR := $(SRC_DIR)/grammar
GEN_DIR := $(SRC_DIR)/generated
OBJ_DIR := build
GRAMMAR_FILE := ParaCL.g4

# Files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
ANTLR_SRCS := $(GEN_DIR)/ParaCLLexer.cpp \
              $(GEN_DIR)/ParaCLParser.cpp \
              $(GEN_DIR)/ParaCLBaseVisitor.cpp \
              $(GEN_DIR)/ParaCLVisitor.cpp

OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS)) \
        $(patsubst $(GEN_DIR)/%.cpp, $(OBJ_DIR)/gen/%.o, $(ANTLR_SRCS))

TARGET := paracl

# --- Rules ---

all: $(TARGET)

$(TARGET): $(ANTLR_SRCS) $(OBJS)
	@echo "[LINK] $@"
	@$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(ANTLR_SRCS): $(GRAMMAR_DIR)/$(GRAMMAR_FILE)
	@echo "[ANTLR] Generating sources..."
	@mkdir -p $(GEN_DIR)
	cd $(GRAMMAR_DIR) && antlr4 -Dlanguage=Cpp -visitor -no-listener -o ../generated $(GRAMMAR_FILE)

# Compile Main Sources
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(ANTLR_SRCS)
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile Generated Sources (Suppress warnings)
$(OBJ_DIR)/gen/%.o: $(GEN_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $< (Generated)"
	@$(CXX) $(CXXFLAGS) -w -c $< -o $@

clean:
	@echo "[CLEAN] Removing artifacts..."
	@rm -rf $(OBJ_DIR) $(GEN_DIR) $(TARGET)

.PHONY: all clean
