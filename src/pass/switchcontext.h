#ifndef EGALITO_PASS_SWITCHCONTEXT_PASS_H
#define EGALITO_PASS_SWITCHCONTEXT_PASS_H

#include "pass/stackextend.h"

/* Make a context that allows a function to be executed
 * without destroying the original context. */

// This will become an unexpected call to the caller, so all registers
// that are not saved by the called function must be saved.

#ifdef ARCH_X86_64
// Save R11, R10, R9, R8, RDI, RSI, RDX, RCX, RAX
#define EGALITO_CONTEXT_SIZE    (8*9)
#define REGISTER_SAVE_LIST      {11, 10, 9, 8, 7, 6, 2, 1, 0}
#elif defined(ARCH_AARCH64)
// For AARCH64, this means all non-callee-saved registers, except for
// X29 and X30 which will be saved by InstrumentCalls pass.
// This includes NZCV and FPSR, which will be held in the first
// callee-saved registers (save them on stack instead).
#define EGALITO_CONTEXT_SIZE    (8*22)
#define REGISTER_SAVE_LIST      {0, 1}
#endif

class SwitchContextPass : public StackExtendPass {
private:
    const size_t contextSize;
public:
    SwitchContextPass()
        : StackExtendPass(EGALITO_CONTEXT_SIZE, REGISTER_SAVE_LIST),
          contextSize(EGALITO_CONTEXT_SIZE) {}
private:
    void useStack(Function *function, FrameType *frame);
    void addSaveContextAt(Function *function, FrameType *frame);
    void addRestoreContextAt(Instruction *instruction, FrameType *frame);
};

#endif
