SRC_DIR   = src
BUILD_DIR = build

compile: a.out

run: a.out
	./a.out input.txt

$(BUILD_DIR)/lex.yy.c: $(SRC_DIR)/icg.l $(BUILD_DIR)/y.tab.c | $(BUILD_DIR)
	lex -o $@ $<

$(BUILD_DIR)/y.tab.c: $(SRC_DIR)/icg.y | $(BUILD_DIR)
	yacc -d -o $@ $<

a.out: $(BUILD_DIR)/lex.yy.c $(BUILD_DIR)/y.tab.c $(wildcard $(SRC_DIR)/*.hpp)
	g++ -g -I$(SRC_DIR) $(BUILD_DIR)/y.tab.c -o a.out

wasm: web/icg.js

web/icg.js: $(BUILD_DIR)/lex.yy.c $(BUILD_DIR)/y.tab.c $(wildcard $(SRC_DIR)/*.hpp)
	em++ -O2 -std=c++17 -DICG_LIB -I$(SRC_DIR) -x c++ $(BUILD_DIR)/y.tab.c -o $@ \
		-fexceptions \
		-sMODULARIZE=1 -sEXPORT_NAME=createIcg \
		-sEXPORTED_FUNCTIONS=_icg_compile \
		-sEXPORTED_RUNTIME_METHODS=cwrap \
		-sALLOW_MEMORY_GROWTH=1 \
		-sENVIRONMENT=web,node


$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) a.out web/icg.js web/icg.wasm

.PHONY: compile run clean wasm
