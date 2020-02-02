#include <unistd.h>
#include <zinit.h>
#include <zmsgbox.h>

void zbot::init() {
  if (geteuid() == 0) {
    fprintf(stderr,
            "%s: cannot be run as root\n"
            "Please log in (using, e.g., \"su\") as the "
            "(unprivileged) user.\n",
            program_invocation_name);
  }
  srand(time(NULL));
}