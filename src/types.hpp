#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ParaCompiler::Types {

struct Type {
    virtual ~Type() {}

    virtual operator std::string() const = 0;
    virtual size_t get_width() const = 0;
};

struct IntType : Type {
    size_t width;
    IntType(size_t w) : width(w) {}

    operator std::string() const override {
        return "intType(" + std::to_string(width) + ")";
    }
    virtual size_t get_width() const override { return width; }
};

struct BoolType : Type {
    operator std::string() const override { return "boolType"; }
    virtual size_t get_width() const override { return 1; }
};

// for literals and inputs
struct FlexibleType : Type {
    operator std::string() const override { return "flexType"; }
    virtual size_t get_width() const override {
        throw "unable to get width of flexible type!";
    }
};

struct TypeManager {
    std::vector<std::unique_ptr<Type>> types;
    std::unordered_map<size_t, IntType *> ints;
    BoolType *boolt = nullptr;
    FlexibleType *flext = nullptr;

    template <typename T>
    const T *get_singletone_type(T *&field) {
        if (field) return field;
        auto boolsp = std::make_unique<T>();
        field = boolsp.get();
        types.push_back(std::move(boolsp));
        return field;
    }

    const BoolType *get_boolt() { return get_singletone_type<BoolType>(boolt); }

    const FlexibleType *get_flexiblet() {
        return get_singletone_type<FlexibleType>(flext);
    }

    const IntType *get_intt(size_t width = 32) {
        if (auto it = ints.find(width); it != ints.end()) return it->second;

        auto intsp = std::make_unique<IntType>(width);
        auto intp = intsp.get();
        types.push_back(std::move(intsp));
        ints.emplace(width, intp);
        return intp;
    }

    const Type *get_common_type(const Type *t1, const Type *t2) {
        if (t1 == t2) return t1;
        // at least one is int
        // FIXME: if bool is with flexible??
        size_t width = 0;
        if (auto ti = dynamic_cast<const IntType *>(t1))
            width = std::max(width, ti->width);
        if (auto ti = dynamic_cast<const IntType *>(t2))
            width = std::max(width, ti->width);
        return get_intt(width);
    }
};

}  // namespace ParaCompiler::Types
