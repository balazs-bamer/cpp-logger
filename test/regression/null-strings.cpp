// Logging a null char const* or char* dereferenced it: in the converter for plain strings, and on
// the logging task while copying an LC::St stored string in background mode. Each must print (null).

#include "TestLog.h"

#include <string>

int main() {
  initLog();
  Log::registerCurrentTask("main");
  char const * nullConst = nullptr;
  char * nullMutable = nullptr;
  Log::n() << "null:" << nullConst << nullMutable << LC::St << nullConst << "end" << Log::end;
  Log::unregisterCurrentTask();
  Log::done();

  bool found = false;
  for(auto const &line : outputLines()) {
    found = found || line == "null: (null) (null) (null) end ";
  }
  CHECK(found);
  return finish();
}
