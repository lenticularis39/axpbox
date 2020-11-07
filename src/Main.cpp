//
// Created by tglozar on 21.09.20.
//

#include <cstring>
#include <iostream>

int main_sim(int argc, char *argv[]);
int main_cfg(int argc, char *argv[]);

int main(int argc, char **argv) {
  if (argc <= 1 || (strcmp(argv[1], "run") && strcmp(argv[1], "configure"))) {
    std::cerr << "AXPBox Alpha Emulator" << std::endl;
#ifdef GITINFO
    std::cerr << "Git version info: " << std::string(GITINFO) << std::endl;
#endif
    std::cerr << "Usage: " << argv[0] << " run|configure <options>" << std::endl;
    return 1;
  }

  if (strcmp(argv[1], "run") == 0) {
    return main_sim(argc - 1, ++argv);
  }

  if (strcmp(argv[1], "configure") == 0) {
    return main_cfg(argc - 1, ++argv);
  }
}
