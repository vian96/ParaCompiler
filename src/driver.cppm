module;

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

export module ParaCompiler:Driver;

import std;

import :Compiler;
import :LLVMEmitter;

namespace fs = std::filesystem;

export namespace ParaCompiler {

class Driver {
public:
    struct Options {
        std::string input_file;
        std::string output_binary;
        std::string custom_clang_path;
        bool dump_ast = false;
        bool print_ir = true;
    };

    int run(int argc, char* argv[]) {
        auto opts = parse_args(argc, argv);
        if (!opts) return 1;

        if (!compile(*opts)) return 1;

        return 0;
    }

private:
    std::optional<Options> parse_args(int argc, char* argv[]) {
        Options opts;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-o") {
                if (i + 1 < argc) opts.output_binary = argv[++i];
                else { std::cerr << "Error: -o requires an argument\n"; return std::nullopt; }
            } else if (arg == "--dump-ast") {
                opts.dump_ast = true;
            } else if (arg == "--no-ir") {
                opts.print_ir = false;
            } else if (arg == "--clang-path") {
                if (i + 1 < argc) opts.custom_clang_path = argv[++i];
                else { std::cerr << "Error: --clang-path requires an argument\n"; return std::nullopt; }
            } else if (arg.starts_with("-")) {
                std::cerr << "Unknown option: " << arg << "\n";
                return std::nullopt;
            } else {
                opts.input_file = arg;
            }
        }

        if (opts.input_file.empty()) {
            std::cerr << "Usage: paracl <input.pcl> [-o output] [--no-ir] [--dump-ast]\n";
            return std::nullopt;
        }
        return opts;
    }

    bool compile(const Options& opts) {
        std::ifstream stream(opts.input_file);
        if (!stream) {
            std::cerr << "File not found: " << opts.input_file << "\n";
            return false;
        }

        Compiler compiler;
        if (!compiler.compile_tu(stream)) {
            return false;
        }

        if (opts.dump_ast) {
            compiler.dump_ast();
        }

        std::unique_ptr<llvm::raw_fd_ostream> file_os;
        llvm::raw_ostream* os = nullptr;
        std::string ir_temp_path;

        if (!opts.output_binary.empty()) {
            ir_temp_path = opts.output_binary + ".ll";
            std::error_code ec;
            file_os = std::make_unique<llvm::raw_fd_ostream>(ir_temp_path, ec);
            if (ec) {
                std::cerr << "Error creating temp IR file: " << ec.message() << "\n";
                return false;
            }
            os = file_os.get();
        } else if (opts.print_ir) {
            os = &llvm::outs();
        }

        if (os) {
            LLVMEmitter::generate_llvm_ir(*compiler.ast, compiler.type_manager, *os);
        }

        if (file_os) {
            file_os.reset();
        }

        if (!opts.output_binary.empty() && opts.print_ir) {
            std::ifstream ir_in(ir_temp_path);
            std::cout << ir_in.rdbuf();
        }

        if (!opts.output_binary.empty()) {
            bool ok = run_clang(opts, ir_temp_path);
            // fs::remove(ir_temp_path);
            return ok;
        }

        return true;
    }

    bool run_clang(const Options& opts, const std::string& ir_file) {
        std::string clang_cmd = "clang++";
        if (!opts.custom_clang_path.empty()) {
            clang_cmd = opts.custom_clang_path;
        } else if (const char* env = std::getenv("PARACL_CLANG")) {
            clang_cmd = env;
        }

        auto lib_path = find_parastdlib();
        if (!lib_path) {
            std::cerr << "Error: parastdlib.a (static) not found. Build target 'parastdlib_static'.\n";
            return false;
        }

        std::string cmd = clang_cmd + " " + ir_file + " -o " + opts.output_binary +
                          " " + lib_path->string();

        // std::cerr << "[Driver] Executing: " << cmd << "\n";

        int res = std::system(cmd.c_str());

        if (res != 0) {
            std::cerr << "Linking failed. Make sure clang++ is installed.\n";
            return false;
        }
        return true;
    }

    std::optional<fs::path> find_parastdlib() {
        std::error_code ec;
        fs::path exe_path = fs::canonical("/proc/self/exe", ec);
        if (ec) exe_path = fs::current_path();
        else exe_path = exe_path.parent_path();

        fs::path p1 = exe_path / "libtinyparastdlib.a";
        if (fs::exists(p1)) return p1;

        fs::path p2 = exe_path.parent_path() / "src" / "libtinyparastdlib.a";
        if (fs::exists(p2)) return p2;

        fs::path p3 = exe_path / "src" / "libtinyparastdlib.a";
        if (fs::exists(p3)) return p3;

        if (fs::exists("libtinyparastdlib.a")) return "libtinyparastdlib.a";

        return std::nullopt;
    }
};

}  // namespace ParaCompiler
