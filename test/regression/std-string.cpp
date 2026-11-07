// REVIEW.md bug 4: only the background mode chain helper had an operator<< for std::string, so
// logging one did not compile in direct mode. Logs a temporary, lvalues and a string longer than
// one message payload. Background mode copies them in payload sized chunks, because the
// temporary is gone before the transmitter converts it. The shutdown build has to compile and
// log nothing.

#include "TestLog.h"

#include <string>

int main() {
  initLog();
  Log::registerCurrentTask("main");
  std::string lvalue = "lvalue";
  std::string const constLvalue = "const";
  Log::n() << "string:" << std::string("temporary") << lvalue << constLvalue << std::string("longer-than-a-payload") << "end" << Log::end;
  Log::unregisterCurrentTask();
  Log::done();

  auto const lines = outputLines();
#ifdef LOG_TEST_SHUTDOWN
  CHECK(lines.empty());
#else
  bool found = false;
  for(auto const &line : lines) {
    found = found || line == "string: temporary lvalue const longer-than-a-payload end ";
  }
  CHECK(found);
#endif
  return finish();
}
