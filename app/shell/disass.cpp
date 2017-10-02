#include <iostream>
#include <cstdio>
#include "disass.h"

#include "analysis/controlflow.h"
#include "conductor/setup.h"
#include "conductor/conductor.h"
#include "chunk/dump.h"
#include "chunk/concrete.h"
#include "generate/bingen.h"
#include "operation/find.h"
#include "operation/find2.h"
#include "pass/logcalls.h"
#include "pass/dumptlsinstr.h"
#include "pass/stackxor.h"
#include "pass/noppass.h"
#include "pass/detectnullptr.h"
#include "pass/stackextend.h"
#include "dwarf/parser.h"

static bool findInstrInModule(Module *module, address_t address) {
    for(auto f : CIter::functions(module)) {
        if(auto i = ChunkFind().findInnermostContaining(f, address)) {
            ChunkDumper dump;
            i->accept(&dump);
            return true;
        }
    }
    return false;
}

void DisassCommands::registerCommands(CompositeCommand *topLevel) {
    topLevel->add("disass", [&] (Arguments args) {
        if(!setup->getConductor()) {
            std::cout << "no ELF files loaded\n";
            return;
        }
        args.shouldHave(1);

        Function *func = nullptr;
        address_t addr;
        if(args.asHex(0, &addr)) {
            func = ChunkFind2(setup->getConductor())
                .findFunctionContaining(addr);
        }
        else {
            func = ChunkFind2(setup->getConductor())
                .findFunction(args.front().c_str());
        }

        if(func) {
            ChunkDumper dump;
            func->accept(&dump);
        }
        else {
            std::cout << "can't find function or address \"" << args.front() << "\"\n";
        }
    }, "disassembles a single function (like the GDB command)");

    topLevel->add("disass2", [&] (Arguments args) {
        if(!setup->getConductor()) {
            std::cout << "no ELF files loaded\n";
            return;
        }
        args.shouldHave(2);

        auto module = CIter::findChild(setup->getConductor()->getProgram(),
            args.front().c_str());

        if(module) {
            Function *func = ChunkFind2()
                .findFunctionInModule(args.get(1).c_str(), module);
            if(func) {
                ChunkDumper dump;
                func->accept(&dump);
            }
            else {
                std::cout << "can't find function \"" << args.front() << "\"\n";
            }
        }
    }, "disassembles a single function (like the GDB command) in the given module");

    topLevel->add("x/i", [&] (Arguments args) {
        if(!setup->getConductor()) {
            std::cout << "no ELF files loaded\n";
            return;
        }
        args.shouldHave(1);
        address_t addr;
        if(!args.asHex(0, &addr)) {
            std::cout << "invalid address, please use hex\n";
            return;
        }

        auto conductor = setup->getConductor();
        auto mainModule = conductor->getMainSpace()->getModule();
        if(findInstrInModule(mainModule, addr)) return;

        for(auto library : *conductor->getLibraryList()) {
            auto space = library->getElfSpace();
            if(!space) continue;

            if(findInstrInModule(space->getModule(), addr)) return;
        }
    }, "disassembles a single instruction");

    topLevel->add("cfgdot", [&] (Arguments args) {
        if(!setup->getConductor()) {
            std::cout << "no ELF files loaded\n";
            return;
        }
        args.shouldHave(1);

        Function *func = nullptr;
        address_t addr;
        if(args.asHex(0, &addr)) {
            func = ChunkFind2(setup->getConductor())
                .findFunctionContaining(addr);
        }
        else {
            func = ChunkFind2(setup->getConductor())
                .findFunction(args.front().c_str());
        }

        if(func) {
            ControlFlowGraph cfg(func);
            cfg.dumpDot();
        }
        else {
            std::cout << "can't find function or address \"" << args.front() << "\"\n";
        }

    }, "prints CFG in dot");

    topLevel->add("logcalls", [&] (Arguments args) {
        if(!setup->getConductor()) {
            std::cout << "no ELF files loaded\n";
            return;
        }
        args.shouldHave(0);
        LogCallsPass logCalls(setup->getConductor());
        // false = do not add tracing to Egalito's own functions
        setup->getConductor()->acceptInAllModules(&logCalls, false);
    }, "runs LogCallsPass to instrument function calls");

    topLevel->add("reassign", [&] (Arguments args) {
        if(!setup->getConductor()) {
            std::cout << "no ELF files loaded\n";
            return;
        }
        setup->makeLoaderSandbox();
        setup->moveCodeAssignAddresses(true);
    }, "allocates a sandbox and assigns functions new addresses");

    topLevel->add("generate", [&] (Arguments args) {
        args.shouldHave(1);
        setup->makeFileSandbox(args.front().c_str());
        setup->moveCode(false);  // calls sandbox->finalize()
    }, "writes out the current code to an ELF file");

    topLevel->add("bin", [&] (Arguments args) {
        args.shouldHave(1);
        BinGen(setup, args.front().c_str()).generate();
    }, "writes out the current image to a binary file");

    topLevel->add("dumptls", [&] (Arguments args) {
        args.shouldHave(0);
        DumpTLSInstrPass dumptls;
        setup->getConductor()->acceptInAllModules(&dumptls);
    }, "shows all instructions that refer to the TLS register");

    topLevel->add("stackxor", [&] (Arguments args) {
        args.shouldHave(0);
        StackXOR stackXOR(0x28);
        setup->getConductor()->acceptInAllModules(&stackXOR);
    }, "adds XOR to return addresses on the stack");

    topLevel->add("nop-pass", [&] (Arguments args) {
        if(!setup->getConductor()) {
            std::cout << "no ELF files loaded\n";
            return;
        }
        NopPass nopPass;
        if (args.size() == 1) {
            Function *func = nullptr;
            func = ChunkFind2(setup->getConductor())
                    .findFunction(args.front().c_str());
            if(func) {
                func->accept(&nopPass);
            }
            else {
                std::cout << "can't find function \"" << args.front() << "\"\n";
            }
        } else if (args.size() == 0) {
            setup->getConductor()->acceptInAllModules(&nopPass);
        } else {
            std::cout << "This pass only takes 0 or 1 arguments." << args.front() << "\"\n";
        }
    }, "adds nop instructions after every non-library instruction");

    topLevel->add("modules", [&] (Arguments args) {
        args.shouldHave(0);
        if(!setup->getConductor() || !setup->getConductor()->getProgram()) {
            std::cout << "no ELF files loaded\n";
            return;
        }
        for(auto module : CIter::children(setup->getConductor()->getProgram())) {
            std::cout << module->getName() << std::endl;
        }
    }, "shows a list of all loaded modules");

    topLevel->add("jumptables", [&] (Arguments args) {
        args.shouldHave(0);
        for(auto module : CIter::children(setup->getConductor()->getProgram())) {
            std::cout << "jumptables in " << module->getName() << "...\n";
            ChunkDumper dumper;
            module->getJumpTableList()->accept(&dumper);
        }
    }, "dumps all jump tables in all modules");

    topLevel->add("jumptables2", [&] (Arguments args) {
        args.shouldHave(1);
        auto module = CIter::findChild(setup->getConductor()->getProgram(),
            args.front().c_str());
        if(module) {
            ChunkDumper dumper;
            module->getJumpTableList()->accept(&dumper);
        }
    }, "dumps all jump tables in the given module");

    topLevel->add("functions", [&] (Arguments args) {
        args.shouldHave(1);
        auto module = CIter::findChild(setup->getConductor()->getProgram(),
            args.front().c_str());
        if(module) {
            for(auto func : CIter::functions(module)) {
                std::cout << func->getName() << std::endl;
            }
        }
    }, "shows a list of all functions in a module");
    topLevel->add("functions2", [&] (Arguments args) {
        args.shouldHave(1);
        auto module = CIter::findChild(setup->getConductor()->getProgram(),
            args.front().c_str());
        if(module) {
            std::vector<Function *> funcList;
            for(auto func : CIter::functions(module)) {
                funcList.push_back(func);
            }

            std::sort(funcList.begin(), funcList.end(),
                [](Function *a, Function *b) {
                    return a->getName() < b->getName();
                });

            for(auto func : funcList) {
                std::printf("0x%08lx %s\n",
                    func->getAddress(), func->getName().c_str());
            }
        }
    }, "shows a sorted list of all functions in a module, with addresses");
    topLevel->add("functions3", [&] (Arguments args) {
        args.shouldHave(1);
        auto module = CIter::findChild(setup->getConductor()->getProgram(),
            args.front().c_str());
        if(module) {
            std::vector<Function *> funcList;
            for(auto func : CIter::functions(module)) {
                funcList.push_back(func);
            }

            std::sort(funcList.begin(), funcList.end(),
                [](Function *a, Function *b) {
                    if(a->getAddress() < b->getAddress()) return true;
                    if(a->getAddress() == b->getAddress()) {
                        return a->getName() < b->getName();
                    }
                    return false;
                });

            for(auto func : funcList) {
                std::printf("0x%08lx 0x%08lx %s\n",
                    func->getAddress(), func->getSize(), func->getName().c_str());
            }
        }
    }, "shows a list of all functions in a module, with addresses and sizes");

    topLevel->add("regions", [&] (Arguments args) {
        args.shouldHave(1);
        auto module = CIter::findChild(setup->getConductor()->getProgram(),
            args.front().c_str());
        if(module) {
            ChunkDumper dumper;
            module->getDataRegionList()->accept(&dumper);
        }
    }, "shows a list of all data regions in a module");

    topLevel->add("markers", [&] (Arguments args) {
        args.shouldHave(1);
        auto module = CIter::findChild(setup->getConductor()->getProgram(),
            args.front().c_str());
        if(module) {
            ChunkDumper dumper;
            module->getMarkerList()->accept(&dumper);
        }
    }, "shows a list of all markers in a module");

    topLevel->add("detectnull", [&] (Arguments args) {
        if(!setup->getConductor()) {
            std::cout << "no ELF files loaded\n";
            return;
        }
        args.shouldHave(0);
        DetectNullPtrPass detectNull;
        setup->getConductor()->acceptInAllModules(&detectNull, true);
    }, "runs DetectNullPtrPass to instrument indirect calls");

    topLevel->add("dwarf", [&] (Arguments args) {
        args.shouldHave(0);
        auto elf = setup->getConductor()->getMainSpace()->getElfMap();
        DwarfParser parser(elf);
    }, "parses DWARF unwind info for the main ELF file");

    topLevel->add("extend", [&] (Arguments args) {
        if(!setup->getConductor()) {
            std::cout << "no ELF files loaded\n";
            return;
        }
        args.shouldHave(1);

        Function *func = nullptr;
        address_t addr;
        if(args.asHex(0, &addr)) {
            func = ChunkFind2(setup->getConductor())
                .findFunctionContaining(addr);
        }
        else {
            func = ChunkFind2(setup->getConductor())
                .findFunction(args.front().c_str());
        }

        if(func) {
            StackExtendPass extender(0x10);
            func->accept(&extender);
        }
        else {
            std::cout << "can't find function or address \"" << args.front() << "\"\n";
        }
    }, "extend stack of a single function");
}
