#ifndef SYMTAB_H
#define SYMTAB_H

#include <string>
#include <unordered_map>
#include "type.hpp"

struct STEntry {
    std::string name;
    Type *type;
};

class SymbolTable {
    std::unordered_map<std::string, STEntry> map;
public:
    void insert(std::string name, Type *type) {
        map[name] = {
            .name = name,
            .type = type
        };
    }
    Type *getType(std::string name) {
        if (map.count(name)) {
            return map[name].type;
        } else {
            return nullptr;
        }
    }
};


inline SymbolTable ST;

#endif // SYMTAB_H
