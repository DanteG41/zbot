#include <getopt.h>
#include <zconfig.h>

int main(int argc, char *argv[]) {
  int opt;
  ZConfig config;

  while ((opt = getopt(argc, argv, "hf:")) != -1) {
    switch (opt) {
    case 'f': {
      config.configFile = optarg; // test
      break;                      // test2
    }
    default:
      fprintf(stderr, "Usage: %s [-f configfile] -[c|C] chat -m message\n\
            -f\t\tconfiguration file path\n\
            -c\t\tchat recipient name\n\
            -C\t\tchat recipient ID\n\
            -m\t\tmessage (max length 256 characters)\n",
              argv[0]);
      exit(EXIT_FAILURE);
    }
  }
  config.load();
}