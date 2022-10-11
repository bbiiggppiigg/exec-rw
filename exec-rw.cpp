#include "elfio/elfio.hpp"

#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>

#include <unordered_map>

std::unordered_map<ELFIO::section *, ELFIO::section *> ogToNewSectionMap;
std::unordered_map<ELFIO::section *, ELFIO::section *> newToOgSectionMap;

// This tool creates a clone of the original executable. The goal is to
// eventually add a new section and a new segment while cloning. Right now the
// tool will ignore the fatbin parameter, till have the cloned executable
// running.
//
// usage:
// exec-rw <og-exec> <fatbin> <new-exec>
//
// As mentioned eariler, one can pass anything for <fatbin> as we aren't using
// it right now.
//
// FIXME:
// cloning any executable (not necessarily AMD ones) should work on x86-64 at
// least, but the cloned executable halts with a segfault

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
ELFIO::section *getStrtabSection(const ELFIO::elfio &file) {
  for (int i = 0; i < file.sections.size(); ++i) {
    ELFIO::section *sec = file.sections[i];
    if (sec->get_name() == ".strtab" && sec->get_type() == ELFIO::SHT_STRTAB)
      return file.sections[i];
  }
  return nullptr;
}

ELFIO::section *getSymtabSection(const ELFIO::elfio &file) {
  for (int i = 0; i < file.sections.size(); ++i) {
    ELFIO::section *sec = file.sections[i];
    if (sec->get_name() == ".symtab" && sec->get_type() == ELFIO::SHT_SYMTAB)
      return file.sections[i];
  }
  return nullptr;
}

ELFIO::section *getDynsymSection(const ELFIO::elfio &file) {
  for (int i = 0; i < file.sections.size(); ++i) {
    ELFIO::section *sec = file.sections[i];
    if (sec->get_name() == ".dynsym" && sec->get_type() == ELFIO::SHT_DYNSYM)
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

// static size_t getFileSizeAndReset(std::ifstream &openFile) {
//   openFile.seekg(0, std::ifstream::end);
//   size_t size = openFile.tellg();
//   openFile.seekg(0, std::ifstream::beg);
//   return size;
// }
//
// ELFIO::segment *getLastSegment(const ELFIO::elfio &execFile) {
//   const size_t numSegments = execFile.segments.size();
//   assert(numSegments != 0);
//
//   ELFIO::segment *theSegment = execFile.segments[0];
//   for (size_t i = 0; i < numSegments; ++i) {
//     ELFIO::segment *currSegment = execFile.segments[i];
//     if (currSegment->get_virtual_address() >
//         theSegment->get_virtual_address()) {
//       theSegment = currSegment;
//     }
//   }
//
//   return theSegment;
// }

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
  case ELFIO::SHT_NULL:
    return false;

  case ELFIO::SHT_STRTAB:
    // Don't clone section header string table, ELFIO will create a new one
    if (section->get_name() == ".shstrtab")
      return false;
    return true;

  default:
    return true;
  }
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

    // NOTE: This can be incorrect link, and will be corrected later, after all
    // sections are cloned.
    newSection->set_link(ogSection->get_link());

    newSection->set_addr_align(ogSection->get_addr_align());
    newSection->set_entry_size(ogSection->get_entry_size());
    newSection->set_address(ogSection->get_address());
    newSection->set_size(ogSection->get_size());

    if (const char *contents = ogSection->get_data())
      newSection->set_data(contents, ogSection->get_size());

    ogToNewSectionMap[ogSection] = newSection;
    newToOgSectionMap[newSection] = ogSection;
  }
}

struct SymbolInfo {
  std::string name;
  ELFIO::Elf64_Addr addr = 0;
  ELFIO::Elf_Xword size = 0;
  unsigned char bind = 0;
  unsigned char type = 0;
  ELFIO::Elf_Half sectionIdx = 0;
  unsigned char other = 0;
};

// bool doesSectionLinkToStrtab(const ELFIO::section *section) {
//   switch(section->get_type()) {
//     case ELFIO::SHT_DYNAMIC:
//     case ELFIO::SHT_SYMTAB:
//     case ELFIO::SHT_DYNSYM:
//
//     case ELFIO::SHT_GNU_verdef:
//     case ELFIO::SHT_GNU_verneed:
//
//       return true;
//     default:
//       return false;
//   }
// }
//
// bool doesSectionLinkToDynsym(const ELFIO::section *section) {
//   switch(section->get_type()) {
//     case ELFIO::SHT_GNU_versym:
//     case ELFIO::SHT_GNU_HASH:
//     case ELFIO::SHT_DYNAMIC:
//       return true;
//
//     default:
//       return false;
//   }
// }
//
// bool doesSectionLinkToSymtab(const ELFIO::section *section) {
//   switch(section->get_type()) {
//     case ELFIO::SHT_HASH:
//     case ELFIO::SHT_REL:
//     case ELFIO::SHT_RELA:
//     case ELFIO::SHT_GROUP:
//     case ELFIO::SHT_SYMTAB_SHNDX:
//       return true;
//
//     default:
//       return false;
//   }
// }

void correctSectionLinks(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {
  auto ogSections = ogExec.sections;
  auto newSections = newExec.sections;

  // If ogSection's sh_link holds index in ogExec's section header table, we
  // must update newSection's sh_link hold corresponding index in newExec's
  // section header table.
  for (size_t i = 0; i < newSections.size(); ++i) {
    auto *currentNewSection = newSections[i];

    auto iter1 = newToOgSectionMap.find(currentNewSection);
    if (iter1 == newToOgSectionMap.end())
      continue;

    auto *ogSection = iter1->second;
    auto ogLinkSectionIdx = ogSection->get_link();
    auto *ogLinkSection = ogSections[ogLinkSectionIdx];

    auto iter2 = ogToNewSectionMap.find(ogLinkSection);
    if (iter2 == ogToNewSectionMap.end())
      continue;

    auto *newLinkSection = iter2->second;
    auto newLinkSectionIdx = newLinkSection->get_index();
    currentNewSection->set_link(newLinkSectionIdx);
  }
}

void cloneSegments(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {
  auto ogSegments = ogExec.segments;
  for (size_t i = 0; i < ogSegments.size(); ++i) {
    ELFIO::segment *ogSegment = ogSegments[i];
    ELFIO::segment *newSegment = newExec.segments.add();
    newSegment->set_type(ogSegment->get_type());
    newSegment->set_flags(ogSegment->get_flags());
    newSegment->set_align(ogSegment->get_align());
    newSegment->set_virtual_address(ogSegment->get_virtual_address());
    newSegment->set_physical_address(ogSegment->get_physical_address());

    newSegment->set_file_size(ogSegment->get_file_size());
    newSegment->set_memory_size(ogSegment->get_memory_size());
  }

  auto newSegments = newExec.segments;
  auto newSections = newExec.sections;

  // Now map new sections into new segments
  for (size_t i = 0; i < newSections.size(); ++i) {
    auto currSection = newSections[i];
    auto currSectionBegin = currSection->get_address();
    auto currSectionSize = currSection->get_size();
    auto currSectionEnd = currSectionBegin + currSectionSize;
    auto currSectionAlign = currSection->get_addr_align();

    for (size_t j = 0; j < newSegments.size(); ++j) {
      auto newSegmentBegin = newSegments[j]->get_virtual_address();
      auto newSegmentSize = newSegments[j]->get_memory_size();
      auto newSegmentEnd = newSegmentBegin + newSegmentSize;
      bool c1 = currSectionBegin >= newSegmentBegin &&
                currSectionBegin < newSegmentEnd;
      bool c2 = currSectionEnd <= newSegmentEnd;
      if (c1 && c2) {
        newSegments[j]->add_section(currSection, 1);
      }
    }
  }
}

void cloneExec(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {
  cloneHeader(ogExec, newExec);
  cloneSections(ogExec, newExec);
  correctSectionLinks(ogExec, newExec);
  cloneSegments(ogExec, newExec);
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
