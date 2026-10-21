#ifndef SYMTAB_H
#define SYMTAB_H

#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include "type.hpp"

struct STEntry {
    std::string name;
    Type *type;
};

class SymbolTable {
    std::vector<std::unique_ptr<STEntry>> entries;

public:
    STEntry* insert(std::string name, Type *type) {
        if (lookup(name) != nullptr) {
            throw std::runtime_error("'" + name + "' is already declared");
        }
        auto new_entry = std::make_unique<STEntry>(STEntry{name, type});
        STEntry* ptr = new_entry.get();
        entries.push_back(std::move(new_entry));
        return ptr;
    }

    STEntry* lookup(std::string name) {
        for (auto &el : entries) {
            if (el->name == name) return el.get();
        }
        return nullptr;
    }
};

inline SymbolTable ST;

#endif // SYMTAB_H