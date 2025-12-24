module;

export module ParaCompiler:Types;

namespace ParaCompiler::Symbols {
struct Symbol;
}

import std;
import std.compat;


export namespace ParaCompiler::Types {

struct Type {
    static std::string ptr_to_str(const Types::Type *t) {
        return t ? std::string(*t) : "nullType";
    }

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
        throw std::runtime_error("unable to get width of flexible type!");
    }
};

struct StructType : Type {
    std::vector<const Type *> fields;
    std::unordered_map<std::string, size_t> names;

    StructType(std::vector<const Type *> fields_,
               std::unordered_map<std::string, size_t> names_)
        : fields(std::move(fields_)), names(std::move(names_)) {}

    operator std::string() const override {
        std::stringstream out;
        out << "StructType: [";
        for (auto &field : fields) out << ptr_to_str(field) << ' ';
        out << "] names: [";
        for (auto &name : names) out << name.first << "=" << name.second << ' ';
        return out.str();
    }

    virtual size_t get_width() const override {
        throw std::runtime_error("unable to get width of struct type!");
    }
};

struct FuncType : Type {
    std::vector<std::pair<Symbols::Symbol *, const Type *>> args;
    const Type *res_type = nullptr;

    FuncType(std::vector<std::pair<Symbols::Symbol *, const Type *>> args_,
             const Type *res_type_)
        : args(std::move(args_)), res_type(res_type_) {}

    operator std::string() const override {
        std::stringstream out;
        out << "FuncType: [";
        for (auto &arg : args) out << arg.first << ": " << ptr_to_str(arg.second) << ", ";
        out << "] res_type: " << ptr_to_str(res_type);
        return out.str();
    }

    virtual size_t get_width() const override {
        throw std::runtime_error("unable to get width of func type!");
    }

    bool is_similar(const FuncType &other) const {
        if (res_type != other.res_type) return false;
        if (args.size() != other.args.size()) return false;
        for (int i = 0; i < args.size(); i++)
            if (args[i].second != other.args[i].second) return false;
        return true;
    }
};

struct TypeManager {
    std::vector<std::unique_ptr<Type>> types;
    std::unordered_map<size_t, IntType *> ints;
    // map so no need for hash
    std::map<
        std::pair<std::vector<std::pair<Symbols::Symbol *, const Type *>>, const Type *>,
        FuncType *>
        funcs;

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
        if (!t1) return t2;
        if (!t2) return t1;
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

    const StructType *get_struct_type(std::vector<const Type *> fields,
                                      std::unordered_map<std::string, size_t> names) {
        auto structt = std::make_unique<StructType>(std::move(fields), std::move(names));
        auto structp = structt.get();
        types.push_back(std::move(structt));
        return structp;
    }

    const FuncType *get_func_type(
        std::vector<std::pair<Symbols::Symbol *, const Type *>> args,
        const Type *res_type) {
        if (auto it = funcs.find(std::make_pair(args, res_type)); it != funcs.end())
            return it->second;

        auto funct = std::make_unique<FuncType>(std::move(args), res_type);
        auto funcp = funct.get();
        types.push_back(std::move(funct));
        return funcp;
    }
};

}  // namespace ParaCompiler::Types
