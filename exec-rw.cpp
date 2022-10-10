#include "elfio/elfio.hpp"

#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>

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

  std::cout << "section name : " << section->get_name() << '\n';
  std::cout << "section size : " << section->get_size() << '\n';
  std::cout << "offset in file : " << section->get_offset() << '\n';
  std::cout << "address alignment : " << section->get_addr_align() << '\n';

  if (!printContents)
    return;

  std::cout << "section contents :\n";

  std::cout << std::hex;
  for (int i = 0; i < section->get_size(); ++i) {
    std::cout << (unsigned)section->get_data()[i] << ' ';
  }
  std::cout << std::dec << '\n';
}

ELFIO::section *getFatbinSection(ELFIO::elfio &file) {
  for (int i = 0; i < file.sections.size(); ++i) {
    if (file.sections[i]->get_name() == ".hip_fatbin")
      return file.sections[i];
  }
  return nullptr;
}

ELFIO::section *getFatbinWrapperSection(ELFIO::elfio &file) {
  for (int i = 0; i < file.sections.size(); ++i) {
    if (file.sections[i]->get_name() == ".hipFatBinSegment")
      return file.sections[i];
  }
  return nullptr;
}

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

void cloneSections(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {
  auto ogSections = ogExec.sections;
  for (size_t i = 0; i < ogSections.size(); ++i) {
    ELFIO::section *ogSection = ogSections[i];
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
    newSection->set_name_string_offset(ogSection->get_name_string_offset());

    if (const char *contents = ogSection->get_data())
      newSection->set_data(contents);
  }
}


void cloneSectionSegmentMapping(ELFIO::segment *ogSegment, ELFIO::segment *newSegment) {
  std::cout << ogSegment->get_sections_num() << '\n';

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

    cloneSectionSegmentMapping(ogSegment, newSegment);
    newSegment->set_file_size(ogSegment->get_file_size());
    newSegment->set_memory_size(ogSegment->get_memory_size());
  }
}

void cloneSymbols(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {
}

void cloneRelocs(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {

}

void cloneExec(const ELFIO::elfio &ogExec, ELFIO::elfio &newExec) {
  cloneHeader(ogExec, newExec);
  cloneSections(ogExec, newExec);
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
