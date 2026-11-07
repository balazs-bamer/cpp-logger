// REVIEW.md agreed redesign point 7, end to end: floating point values travel through the messages
// in queued mode, or straight to the converter in direct mode, and come out correctly rounded with
// the digits their format asks for.

#include "TestLog.h"

#include <limits>
#include <string>

int main() {
  initLog();
  Log::registerCurrentTask("main");
  Log::n() << 1.5f << -2.25 << LC::D3 << 3.14159 << LC::D4 << 9.99999 << -std::numeric_limits<double>::infinity() << Log::end;
#ifdef LOG_TEST_DIRECT   // A long double may not fit the 8 byte payload, then it doesn't compile in queued mode.
  Log::n() << 0.1L << Log::end;   // Converted with double precision, 17 digits by default.
#endif
  Log::unregisterCurrentTask();
  Log::done();

  std::string const expected =
    "-=- Registered task: main 1 \n"
    "1.5000000e+0 -2.250000000000000e+0 3.14e+0 1.000e+1 -inf \n"
#ifdef LOG_TEST_DIRECT
    "1.0000000000000001e-1 \n"
#endif
    "-=- Unregistered task: 1 \n";
  CHECK(gOutput.str() == expected);
  if(gOutput.str() != expected) {
    std::fprintf(stderr, "output:\n%s---\nexpected:\n%s---\n", gOutput.str().c_str(), expected.c_str());
  }
  else { // nothing to do
  }
  return finish();
}
