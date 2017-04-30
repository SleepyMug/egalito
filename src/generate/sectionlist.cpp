#include "sectionlist.h"

ObjGen::Sections::~Sections() {
    for(auto section : sections) {
        delete section;
    }
}

void SectionList::addSection(Section *section) {
    sectionMap[section->getName()] = section;
    sectionIndexMap[section] = sections.size();
    sections.push_back(section);
}

Section *SectionList::operator [] (std::string name) {
    auto it = sectionMap.find(name);
    return (it != sectionMap.end() ? (*it).second : nullptr);
}

int SectionList::indexOf(Section *section) {
    return sectionIndexMap[section];
}

Section *SectionRef::get() const {
    return list[sectionName];
}

int SectionRef::getIndex() const {
    auto section = get();
    return (section ? list->indexOf(section) : -1);
}
