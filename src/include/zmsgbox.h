#include <zstorage.h>

class ZMsgBox : public ZStorage {
private:
  const char* chatName_;

public:
  ZMsgBox(ZStorage& s) { path_ = s.getPath(); };
};