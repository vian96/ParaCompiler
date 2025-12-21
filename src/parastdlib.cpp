#include <llvm/ADT/APInt.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallString.h>

#include <iostream>

static llvm::APInt loadAPInt(uint64_t *buffer, uint32_t bit_width) {
    unsigned num_words = (bit_width + 63) / 64;
    return llvm::APInt(bit_width, llvm::ArrayRef<uint64_t>(buffer, num_words));
}

static void storeAPInt(llvm::APInt &val, uint64_t *buffer) {
    unsigned num_words = (val.getBitWidth() + 63) / 64;
    const uint64_t *raw_data = val.getRawData();
    for (unsigned i = 0; i < num_words; ++i) buffer[i] = raw_data[i];
}

extern "C" void pcl_output_int__(uint64_t *buffer, uint32_t bit_width) {
    if (bit_width == 0) return;
    llvm::APInt val = loadAPInt(buffer, bit_width);
    llvm::SmallString<64> str;
    val.toString(str, 10, true);
    std::cout << str.c_str() << std::endl;
}

extern "C" void pcl_input_int__(uint64_t *buffer, uint32_t bit_width) {
    if (bit_width == 0) return;
    std::string token;
    if (!(std::cin >> token)) return;
    llvm::APInt val(bit_width, token, 10);
    storeAPInt(val, buffer);
}
