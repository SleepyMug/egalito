#ifndef EGALITO_CONDUCTOR_CONDUCTOR_H
#define EGALITO_CONDUCTOR_CONDUCTOR_H

#include "types.h"
#include "chunk/program.h"
#include "chunk/module.h"
#include "chunk/library.h"

class ElfMap;
class Module;
class ChunkVisitor;
class IFuncList;
struct EgalitoTLS;

class Conductor {
private:
    Program *program;
    address_t mainThreadPointer;
    size_t TLSOffsetFromTCB;
    IFuncList *ifuncList;
public:
    Conductor();
    ~Conductor();

    Module *parseExecutable(ElfMap *elf);
    Module *parseEgalito(ElfMap *elf);
    void parseLibraries();
    Module *parseAddOnLibrary(ElfMap *elf);
    void parseEgalitoArchive(const char *archive);

    void resolvePLTLinks();
    void resolveTLSLinks();
    void resolveWeak();
    void resolveVTables();
    void setupIFuncLazySelector();
    void fixDataSections();
    void fixPointersInData();
    EgalitoTLS *getEgalitoTLS() const;

    void writeDebugElf(const char *filename, const char *suffix = "$new");
    void acceptInAllModules(ChunkVisitor *visitor, bool inEgalito = true);

    Program *getProgram() const { return program; }
    LibraryList *getLibraryList() const { return program->getLibraryList(); }

    // deprecated, please use getProgram()->getMain()
    ElfSpace *getMainSpace() const;

    address_t getMainThreadPointer() const { return mainThreadPointer; }
    IFuncList *getIFuncList() const { return ifuncList; }

    void loadTLSDataFor(address_t tcb);

    void check();
private:
    Module *parse(ElfMap *elf, Library *library);
    void allocateTLSArea(address_t base);
    void loadTLSData();
    void backupTLSData();
};

#endif
