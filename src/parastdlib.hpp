#include <llvm/ADT/APInt.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallString.h>

extern "C" void pcl_output_int__(uint64_t *buffer, uint32_t bit_width);
extern "C" void pcl_input_int__(uint64_t *buffer, uint32_t bit_width);
