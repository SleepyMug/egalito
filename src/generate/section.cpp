#include <cstring>  // for memset
#include "section.h"
#include "elf/symbol.h"
#include "elf/elfxx.h"
#include "chunk/concrete.h"  // for Function
#include "log/log.h"

void SectionHeader::writeTo(std::ostream &stream) {
    auto shdr = new ElfXX_Shdr();
    std::memset(shdr, 0, sizeof(*shdr));
    shdr->sh_name       = 0;
    shdr->sh_type       = shdrType;
    shdr->sh_flags      = shdrFlags;
    shdr->sh_addr       = address;
    shdr->sh_offset     = offset;
    shdr->sh_size       = outer->getContent()->getSize();
    shdr->sh_link       = sectionLink->getIndex();
    shdr->sh_info       = 0;  // updated later for strtabs
    shdr->sh_addralign  = 1;
    shdr->sh_entsize    = 0;

    if(writeFunc) writeFunc(stream, shdr);

    stream.write(reinterpret_cast<const char *>(shdr), sizeof(*shdr));
}

SectionHeader::SectionHeader(Section2 *outer, ElfXX_Word type,
    ElfXX_Xword flags) : outer(outer), address(0), offset(0),
    shdrType(type), shdrFlags(flags), sectionLink(nullptr) {

}

Section2::Section2(const std::string &name, ElfXX_Word type,
    ElfXX_Xword flags) : name(name), content(nullptr) {

    header = new SectionHeader(this, type, flags);
    init();
}

Section2::Section2(const std::string &name, DeferredValue *content)
    : name(name), header(nullptr), content(content) {

    init();
}

std::ostream &operator << (std::ostream &stream, Section2 &rhs) {
    if(rhs.hasContent()) {
        rhs.getContent()->writeTo(stream);
    }
    return stream;
}

#if 0
size_t Section::add(const void *data, size_t size) {
    size_t oldSize = this->data.size();
    add(static_cast<const char *>(data), size);
    return oldSize;
}

size_t Section::add(const char *data, size_t size) {
    size_t oldSize = this->data.size();
    this->data.append(data, size);
    LOG(1, "    really added " << size << " bytes");
    return oldSize;
}

size_t Section::add(const std::string &string, bool withNull) {
    size_t oldSize = this->data.size();
    this->data.append(string.c_str(), string.size() + (withNull ? 1 : 0));
    return oldSize;
}

void Section::addNullBytes(size_t size) {
    this->data.append(size, '\0');
}

ElfXX_Shdr *Section::makeShdr(size_t index, size_t nameStrIndex) {
    this->shdrIndex = index;
    ElfXX_Shdr *entry = new ElfXX_Shdr();
    entry->sh_name = nameStrIndex;
    entry->sh_type = shdrType;
    entry->sh_flags = shdrFlags;
    entry->sh_offset = offset;
    entry->sh_size = getSize();
    entry->sh_addr = address;
    // These are sometimes changed later
    entry->sh_entsize = 0;
    entry->sh_info = 0;  // updated later for strtabs
    entry->sh_addralign = 1;
    // don't forget to set sh_link!
    return entry;
}
#endif
