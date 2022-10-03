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
  std::cout << "offset in file : " << section->get_address() << '\n';

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
  if (argc != 3) {
    std::cout << "exactly 3 arguments to " << argv[0] << " expected\n";
    showHelp(argv[0]);
    exit(1);
  }

  const char *bigPath = argv[1];
  const char *smallPath = argv[2];

  ELFIO::elfio bigFile;
  ELFIO::elfio smallFile;

  if (!bigFile.load(bigPath)) {
    std::cout << "can't find or process ELF file " << bigPath << '\n';
    exit(1);
  }

  if (!smallFile.load(smallPath)) {
    std::cout << "can't find or process ELF file " << smallPath << '\n';
    exit(1);
  }

  ELFIO::section *bigFatbinSection = getFatbinSection(bigFile);
  if (!bigFatbinSection) {
    std::cout << ".hip_fatbin section in " << smallPath << "\n";
    exit(1);
  }

  ELFIO::section *smallFatbinSection = getFatbinSection(smallFile);
  if (!smallFatbinSection) {
    std::cout << ".hip_fatbin section in " << smallPath << "\n";
    exit(1);
  }

  ELFIO::section *bigFatbinWrapperSection = getFatbinWrapperSection(bigFile);
  ELFIO::section *smallFatbinWrapperSection =
      getFatbinWrapperSection(smallFile);

  std::cout << "dumps for " << bigPath << '\n';
  dumpSection(bigFatbinSection, false);
  dumpSection(bigFatbinWrapperSection);
  std::cout << std::endl;
  std::cout << "dumps for " << smallPath << '\n';
  dumpSection(smallFatbinSection, false);
  dumpSection(smallFatbinWrapperSection);

  // Step 1. take contents of smaller fatbin.
  // Step 2. rewrite bigger elf section with smaller fatbin starting at offset + 20
  // Step 3. modify third field of fatbinWrappersection i.e 8th byte onwards to hold the new fatbin-offset
  // Step 4. rewrite bigger file with the new contents.

  const char *smallFatbinContent = smallFatbinSection->get_data();
  char *newBigFatbinContent = new char[bigFatbinSection->get_size()];
  std::memcpy(newBigFatbinContent + 20, smallFatbinContent,
              smallFatbinSection->get_size());

  char *newBigWrapperContents = new char[bigFatbinWrapperSection->get_size()];
  std::memcpy(newBigWrapperContents, bigFatbinWrapperSection->get_data(), bigFatbinWrapperSection->get_size());

  // read bytes 8 through 15 in bigWrapper, get the offset
  size_t offset = 0;
  char *offsetPtr = (char *)&offset;
  for (int i = 0; i < 8; ++i) {
    int idx = i + 8;
    offsetPtr[i] = (char)newBigWrapperContents[idx];
  }

  std::cout << std::hex;
  for (int i = 0; i < 8; ++i) {
    std::cout << (unsigned)offsetPtr[i] << ' ';
  }
  std::cout << '\n';
  std::cout << std::dec << offset << '\n';

  offset += 20; // also change line 105 accordingly. works

  // write it to the buffer
  offsetPtr = (char *)&offset;
  for (int i = 0; i < 8; ++i) {
    int idx = i + 8;
    newBigWrapperContents[idx] = offsetPtr[i];
  }

  // update bigFatbinSection and bigFatbinWrappersection with updated data
  bigFatbinSection->set_data(newBigFatbinContent, bigFatbinSection->get_size());
  bigFatbinWrapperSection->set_data(newBigWrapperContents, bigFatbinWrapperSection->get_size());

  // rewrite with modifications
  bigFile.save(std::string(bigPath) + "-rewritten");
}
