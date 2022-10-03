#include "elfio/elfio.hpp"

#include <cstring>
#include <iomanip>
#include <iostream>

void showHelp(const char *toolName) {
  std::cout << "usage : \n";
  std::cout << "  ";
  std::cout << toolName
            << " <path-to-exe> <path-to-fatbin> <path-to-rewritten-exe> \n\n";
  std::cout << toolName
            << " will rewrite <path-to-exe> as <path-to-rewritten-exe> "
               "containing <path-to-fatbin> instead of original fatbin\n";
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

void dumpSection(const ELFIO::section *section, bool printContents = true) {
  std::cout << "section name : " << section->get_name() << '\n';
  std::cout << "section size : " << section->get_size() << '\n';
  std::cout << "offset in file : " << section->get_offset() << '\n';

  if (!printContents)
    return;

  std::cout << "section contents :\n";

  std::cout << std::hex;
  for (int i = 0; i < section->get_size(); ++i) {
    std::cout << (unsigned)section->get_data()[i] << ' ';
  }
  std::cout << std::dec << '\n';
}

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cout << "exactly 3 arguments to " << argv[0] << " expected\n";
    showHelp(argv[0]);
    exit(1);
  }

  const char *ogExecPath= argv[1];
  const char *newFatbinPath = argv[2];
  const char *rwExecPath = argv[3];

  ELFIO::elfio ogExec;
  std::ifstream newFatbin;

  if (!ogExec.load(ogExecPath)) {
    std::cout << "can't find or process ELF file " << ogExecPath << '\n';
    exit(1);
  }

    ELFIO::section *fatbinSection = getFatbinSection(ogExec);
  if (!fatbinSection) {
    std::cout << ".hip_fatbin section in " << ogExecPath << "\n";
    exit(1);
  }

  ELFIO::section *fatbinWrapperSection = getFatbinWrapperSection(ogExec);

  dumpSection(fatbinSection, false);
  std::cout << '\n';
  dumpSection(fatbinWrapperSection);
  std::cout << std::endl;

  newFatbin.open(newFatbinPath, std::ios::in);
  newFatbin.seekg(0, std::ifstream::end);
  int newFatbinSize = newFatbin.tellg();
  newFatbin.seekg(0, std::ifstream::beg);

  if (newFatbinSize > fatbinSection->get_size()) {
    std::cout << "new fatbin is bigger than the one present in the executable\n";
    exit(1);
  }

  char *newFatbinContent = new char[fatbinSection->get_size()];
  newFatbin.read(newFatbinContent, newFatbinSize);
  newFatbin.close();

  // char *newBigWrapperContents = new char[fatbinWrapperSection->get_size()];
  // std::memcpy(newBigWrapperContents, fatbinWrapperSection->get_data(), fatbinWrapperSection->get_size());

  // read bytes 8 through 15 in bigWrapper, get the offset
  // size_t offset = 0;
  // char *offsetPtr = (char *)&offset;
  // for (int i = 0; i < 8; ++i) {
  //   int idx = i + 8;
  //   offsetPtr[i] = (char)newBigWrapperContents[idx];
  // }
  //
  // std::cout << std::hex;
  // for (int i = 0; i < 8; ++i) {
  //   std::cout << (unsigned)offsetPtr[i] << ' ';
  // }

  // std::cout << '\n';
  // std::cout << std::dec << offset << '\n';
  //
  // offset += 20; // also change line 105 accordingly. works
  //
  // // write it to the buffer
  // offsetPtr = (char *)&offset;
  // for (int i = 0; i < 8; ++i) {
  //   int idx = i + 8;
  //   newBigWrapperContents[idx] = offsetPtr[i];
  // }
  //
  // update fatbinSection and fatbinWrapperSection with updated data

  fatbinSection->set_data(newFatbinContent, fatbinSection->get_size());
  // fatbinWrapperSection->set_data(newBigWrapperContents, fatbinWrapperSection->get_size());
  //
  // rewrite with modifications
  ogExec.save(rwExecPath);

}
