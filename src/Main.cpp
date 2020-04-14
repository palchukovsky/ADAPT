
#include "Parser.hpp"

#include <string.h>

#include <fstream>
#include <iostream>

using namespace adapt;

namespace {

void PrintError(const Exception &ex, const bool debug) {
  std::cout << ex.what();
  if (debug) {
    std::cout << R"(: ")" << ex.GetDetails() << R"(".)";
  }
  std::cout << std::endl;
}

bool ReadArgs(int argc, char *argv[], const char *&file, bool &debug) {
  if (argc >= 2 && argv[1][0]) {
    file = &argv[1][0];
    debug = false;
    for (int i = 2; i < argc; ++i) {
      if (strcmp(&argv[i][0], "--debug") == 0) {
        debug = true;
        break;
      }
    }
    return true;
  }
  if (argc == 0) {
    std::cout << "Wrong arguments." << std::endl;
  } else {
    std::cout << "Usage:" << std::endl
              << "\t" << argv[0] << R"( "fileName">" [ --debug ], where:)"
              << std::endl
              << std::endl
              << "\t\t <fileName>: path to input file, required;" << std::endl
              << "\t\t --debug: enable additional debuging inforamtion if set, "
                 "optional;"
              << std::endl;
  }
  return false;
}
}  // namespace

int main(int argc, char *argv[]) {
  bool debug = false;
  const char *sourceFilePath;

  try {
    if (!ReadArgs(argc, argv, sourceFilePath, debug)) {
      return 1;
    }

    std::fstream source(sourceFilePath);
    if (!source) {
      std::cout << "Filed to open source file \"" << sourceFilePath << "\"."
                << std::endl;
      return 1;
    }

    const auto &keywords =
        Parse(source, [&debug](const Exception &ex) { PrintError(ex, debug); });
    if (keywords.empty()) {
      return 1;
    }

    Environment env(std::cout);
    for (const auto &keyword : keywords) {
      try {
        keyword->Execute(env);
      } catch (const BadLanguageException &ex) {
        PrintError(ex, debug);
      }
    }

  } catch (const Exception &ex) {
    PrintError(ex, debug);
    return 1;
  } catch (const std::exception &ex) {
    std::cout << R"(Fatal error: ")" << ex.what() << R"(".)" << std::endl;
    return 1;
  } catch (...) {
    std::cout << "Fatal unknown error." << std::endl;
    return 1;
  }
}