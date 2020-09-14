//
// Created by tglozar on 21.09.20.
//

#include <cstring>
#include <iostream>

using namespace std;

int main_sim(int argc, char *argv[]);
int main_cfg(int argc, char *argv[]);

int main(int argc, char **argv) {
  if (argc <= 1 || (strcmp(argv[1], "run") && strcmp(argv[1], "configure"))) {
    cerr << "AXPBox Alpha Emulator (version 0.1)" << endl
         << "Usage: " << argv[0] << " run|configure <options>" << endl;
    return 1;
  }

  if (strcmp(argv[1], "run") == 0) {
    return main_sim(argc - 1, ++argv);
  }

  if (strcmp(argv[1], "configure") == 0) {
    return main_cfg(argc - 1, ++argv);
  }
}
