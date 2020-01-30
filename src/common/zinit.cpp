#include <zinit.h>
#include <zstorage.h>
#include <unistd.h>

void zbot::init(ZConfig& config) {
  if (geteuid() == 0) {
    fprintf(stderr,
            "%s: cannot be run as root\n"
            "Please log in (using, e.g., \"su\") as the "
            "(unprivileged) user.\n",
            program_invocation_name);
  }
  std::string path;
  config.getParam("storage", path);
  try {
    ZStorage zbotStorage(path);
    ZStorage processingStorage(path + "/processing");
    ZStorage sendingStorage(path + "/sending");
    ZStorage deferredStorage(path + "/deferred");
  } catch (ZStorageException& e) {
    fprintf(stderr, "%s\n", e.getError());
  }
}
