#include "elfio/elfio.hpp"

#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>



// This tool creates a clone of the original executable. The goal is to eventually add a new section and a new segment while cloning.


static void showHelp(const char *toolName) {
  std::cout << "usage : \n";
  std::cout << "  ";
  std::cout << toolName
            << " <path-to-exe> <path-to-fatbin> <path-to-rewritten-exe> \n\n";
  std::cout << toolName
            << " will rewrite <path-to-exe> as <path-to-rewritten-exe> "
               "containing <path-to-fatbin> instead of original fatbin\n";
}

static void dumpSection(const ELFIO::section *section,
                        bool printContents = true) {
  assert(section && "section must be non-null");

  std::cout << "section : " << section->get_name() << ", ";
  std::cout << "size : " << section->get_size() << ", ";
  std::cout << "offset : " << section->get_offset() << ", ";
  std::cout << "addr-align : " << section->get_addr_align() << ", ";
  std::cout << "entry-size : " << section->get_entry_size() << '\n';

  if (!printContents)
    return;

  std::cout << "section contents :\n";

  std::cout << std::hex;
  for (int i = 0; i < section->get_size(); ++i) {
    std::cout << (unsigned)section->get_data()[i] << ' ';
  }
  std::cout << std::dec << '\n';
}

// === SECTION-GETTING HELPERS BEGIN ===
//
ELFIO::section *getSymtabSection(const ELFIO::elfio &file) {
  for (int i = 0; i < file.sections.size(); ++i) {
    ELFIO::section *sec = file.sections[i];
    if (sec->get_name() == ".symtab" && sec->get_type() == ELFIO::SHT_SYMTAB)
      return file.sections[i];
  }
  return nullptr;
}

ELFIO::section *getStrtabSection(const ELFIO::elfio &file) {
  for (int i = 0; i < file.sections.size(); ++i) {
    ELFIO::section *sec = file.sections[i];
    if (sec->get_name() == ".strtab" && sec->get_type() == ELFIO::SHT_STRTAB)
      return file.sections[i];
  }
  return nullptr;
}

ELFIO::section *getSectionStrtabSection(const ELFIO::elfio &file) {
  for (int i = 0; i < file.sections.size(); ++i) {
    ELFIO::section *sec = file.sections[i];
    if (sec->get_name() == ".shstrtab" && sec->get_type() == ELFIO::SHT_STRTAB)
      return file.sections[i];
  }
  return nullptr;
}
ELFIO::section *getFatbinSection(const ELFIO::elfio &file) {
  for (int i = 0; i < file.sections.size(); ++i) {
    if (file.sections[i]->get_name() == ".hip_fatbin")
      return file.sections[i];
  }
  return nullptr;
}

ELFIO::section *getFatbinWrapperSection(const ELFIO::elfio &file) {
  for (int i = 0; i < file.sections.size(); ++i) {
    if (file.sections[i]->get_name() == ".hipFatBinSegment")
      return file.sections[i];
  }
  return nullptr;
}
//
// === SECTION-GETTING HELPERS END ===

static size_t getFileSizeAndReset(std::ifstream &openFile) {
  openFile.seekg(0, std::ifstream::end);
  size_t size = openFile.tellg();
  openFile.seekg(0, std::ifstream::beg);
  return size;
}

ELFIO::segment *getLastSegment(const ELFIO::elfio &execFile) {
  const size_t numSegments = execFile.segments.size();
  assert(numSegments != 0);

  ELFIO::segment *theSegment = execFile.segments[0];
  for (size_t i = 0; i < numSegments; ++i) {
    ELFIO::segment *currSegment = execFile.segments[i];
    if (currSegment->get_virtual_address() >
        theSegment->get_virtual_address()) {
      theSegment = currSegment;
    }
  }

  return theSegment;
}

void cloneHeader(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {
  newExec.create(ogExec.get_class(), ogExec.get_encoding());
  // newExec.set_version(ogExec.get_version());
  newExec.set_os_abi(ogExec.get_os_abi());
  newExec.set_abi_version(ogExec.get_abi_version());
  newExec.set_type(ogExec.get_type());
  newExec.set_machine(ogExec.get_machine());
  newExec.set_entry(ogExec.get_entry());
}

bool shouldClone(const ELFIO::section *section) {
  auto secType = section->get_type();
  switch (secType) {
  case ELFIO::SHT_PROGBITS:
  case ELFIO::SHT_NOBITS:
    return true;
  default:
    return false;
  }
  // if(section->get_type() == ELFIO::SHT_NULL)
  //   return false;
  //
  // if (section->get_name() == ".shstrtab" && section->get_type() ==
  // ELFIO::SHT_STRTAB)
  //   return false;
  //
  // if (section->get_type() == ELFIO::SHT_REL || section->get_type() ==
  // ELFIO::SHT_RELA)
  //   return false;
  //
  // return true;
}

bool isNullSection(const ELFIO::section *section) {
  return section->get_type() == ELFIO::SHT_NULL;
}

void cloneSections(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {
  auto ogSections = ogExec.sections;
  for (size_t i = 0; i < ogSections.size(); ++i) {
    ELFIO::section *ogSection = ogSections[i];

    if (!shouldClone(ogSection))
      continue;
    std::cout << "cloning\n";
    dumpSection(ogSection, false);
    std::cout << '\n';

    const std::string &name = ogSection->get_name();
    ELFIO::section *newSection = newExec.sections.add(name);
    newSection->set_type(ogSection->get_type());
    newSection->set_flags(ogSection->get_flags());
    newSection->set_info(ogSection->get_info());
    newSection->set_link(ogSection->get_link());
    newSection->set_addr_align(ogSection->get_addr_align());
    newSection->set_entry_size(ogSection->get_entry_size());
    newSection->set_address(ogSection->get_address());
    newSection->set_size(ogSection->get_size());

    if (const char *contents = ogSection->get_data())
      newSection->set_data(contents, ogSection->get_size());
  }
}

// void cloneSegments(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {
//   auto ogSegments = ogExec.segments;
//   for (size_t i = 0; i < ogSegments.size(); ++i) {
//     ELFIO::segment *ogSegment = ogSegments[i];
//     ELFIO::segment *newSegment = newExec.segments.add();
//     newSegment->set_type(ogSegment->get_type());
//     newSegment->set_flags(ogSegment->get_flags());
//     newSegment->set_align(ogSegment->get_align());
//     newSegment->set_virtual_address(ogSegment->get_virtual_address());
//     newSegment->set_physical_address(ogSegment->get_physical_address());
//
//     newSegment->set_file_size(ogSegment->get_file_size());
//     newSegment->set_memory_size(ogSegment->get_memory_size());
//   }
// }

struct SymbolInfo {
  std::string name;
  ELFIO::Elf64_Addr addr = 0;
  ELFIO::Elf_Xword size = 0;
  unsigned char bind = 0;
  unsigned char type = 0;
  ELFIO::Elf_Half sectionIdx = 0;
  unsigned char other = 0;
};

void dumpSymbolDetails(const SymbolInfo &symInfo) {
  std::cout << "name : " << symInfo.name << ", "
            << "addr : " << symInfo.addr << ", "
            << "size : " << symInfo.size << ", "
            << "bind : " << symInfo.bind << ", "
            << "type : " << symInfo.type << ", "
            << "sectionIdx : " << symInfo.sectionIdx << ", "
            << "other : " << symInfo.other << '\n';
}

ELFIO::section *createStringSection(ELFIO::elfio &execFile) {
  auto *strtab = execFile.sections.add(".strtab");
  strtab->set_type(ELFIO::SHT_STRTAB);
  strtab->set_addr_align(1);
  return strtab;
}

void cloneSymbols(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {
  // create new strtab first
  auto *ogStrtab = getStrtabSection(ogExec);
  auto *newStrtab = newExec.sections.add(".strtab");
  newStrtab->set_type(ogStrtab->get_type());
  newStrtab->set_flags(ogStrtab->get_flags());
  // newStrtab->set_info(ogStrtab->get_info ());
  // newStrtab->set_link(ogStrtab->get_index());
  // newStrtab->set_entry_size(ogStrtab->get_entry_size());
  newStrtab->set_addr_align(ogStrtab->get_addr_align());

  ELFIO::section *ogSymtab = getSymtabSection(ogExec);

  // new symtab for cloning new symbols
  auto *newSymtab = newExec.sections.add(".symtab");
  newSymtab->set_type(ogSymtab->get_type());
  newSymtab->set_flags(ogSymtab->get_flags());
  newSymtab->set_info(ogSymtab->get_info());

  newSymtab->set_link(newStrtab->get_index());

  newSymtab->set_entry_size(ogSymtab->get_entry_size());
  newSymtab->set_addr_align(ogSymtab->get_addr_align());
  ELFIO::symbol_section_accessor newSymtabAccessor(newExec, newSymtab);

  ELFIO::string_section_accessor newStrtabAccessor(newStrtab);

  // og symbol information for reference
  ELFIO::symbol_section_accessor symtabAccessor(ogExec, ogSymtab);
  unsigned numSymbols = symtabAccessor.get_symbols_num();
  std::cout << numSymbols << " found\n";

  SymbolInfo si;
  for (unsigned i = 0; i < numSymbols; ++i) {
    bool success =
        symtabAccessor.get_symbol(i, si.name, si.addr, si.size, si.bind,
                                  si.type, si.sectionIdx, si.other);
    if (success) {
      dumpSymbolDetails(si);
      // std::cout << "in " << ogExec.sections[si.sectionIdx]->get_name() <<
      // '\n';

      // if (shouldClone(ogExec.sections[si.sectionIdx]) ||
      // isNullSection(ogExec.sections[si.sectionIdx]))
      newSymtabAccessor.add_symbol(newStrtabAccessor, si.name.c_str(), si.addr,
                                   si.size, si.bind, si.type, si.other,
                                   si.sectionIdx);
    }
  }
}

void cloneRelocs(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {

}

void cloneExec(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {
  cloneHeader(ogExec, newExec);
  cloneSections(ogExec, newExec);
  cloneSymbols(ogExec, newExec);
  // cloneRelocationSections(ogExec, newExec);
  // updateHeader(newExec);
  // cloneSegments(ogExec, newExec);
}




int main(int argc, char **argv) {
  if (argc != 4) {
    std::cout << "exactly 3 arguments to " << argv[0] << " expected\n";
    showHelp(argv[0]);
    exit(1);
  }

  const char *execFilePath = argv[1];
  const char *newFatbinPath = argv[2];
  const char *rwExecPath = argv[3];

  ELFIO::elfio execFile;
  ELFIO::elfio newExecFile;
  std::ifstream newFatbin;

  if (!execFile.load(execFilePath)) {
    std::cout << "can't find or process ELF file " << execFilePath << '\n';
    exit(1);
  }

  cloneExec(execFile, newExecFile);
  std::cout << newExecFile.validate() << '\n';
  newExecFile.save(rwExecPath);


  // ELFIO::section *fatbinSection = getFatbinSection(execFile);
  // if (!fatbinSection) {
  //   std::cout << ".hip_fatbin section in " << execFilePath << "\n";
  //   exit(1);
  // }
  //
  // ELFIO::section *fatbinWrapperSection = getFatbinWrapperSection(execFile);
  //
  // dumpSection(fatbinSection, false);
  // std::cout << '\n';
  // dumpSection(fatbinWrapperSection);
  // std::cout << std::endl;
  //
  // std::cout << execFile.validate() << '\n';
  //
  // newFatbin.open(newFatbinPath, std::ios::in);
  // size_t newFatbinSize = getFileSizeAndReset(newFatbin);
  // if (newFatbinSize > fatbinSection->get_size()) {
  //   std::cout
  //       << "new fatbin is bigger than the one present in the executable\n";
  //   std::cout << "appending new fatbin and updating binary...\n";
  // }
}
