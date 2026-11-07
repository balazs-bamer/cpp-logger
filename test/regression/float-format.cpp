// REVIEW.md agreed redesign point 7: floating point numbers were converted with log10 and pow on
// long double, which needs most of the floating point library on an MCU. A last digit rounding up
// to 10 was clamped to 9 instead of carrying, so 0.99999 with 3 digits came out as 9.99e-1, and
// nan and inf had no sign. Now integer arithmetic only, correctly rounded. glibc's printf is the
// reference, it rounds exactly with ties to even like the converter.

#include "TestCheck.h"
#include "LogMessageCompact.h"
#include "LogConverterCustomText.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <string>

using Message = nowtech::log::MessageCompact<8u, true>;
using Converter = nowtech::log::ConverterCustomText<Message, true, 100u, true, false>;
using AlignedConverter = nowtech::log::ConverterCustomText<Message, true, 100u, true, true>;

constexpr uint8_t cgMaxDigits = 17u;

template<typename tConverter = Converter, typename tValue>
std::string convert(tValue const aValue, uint8_t const aDigits) {
  char buffer[64];
  tConverter converter(buffer, buffer + sizeof(buffer));
  converter.convert(aValue, 10u, aDigits);
  return std::string(buffer, converter.end() - 1);   // Without the separating space.
}

/// printf("%.*e") in the converter's format, without leading zeros in the exponent.
std::string reference(double const aValue, uint8_t const aDigits) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.*e", aDigits - 1, aValue);
  std::string result(buffer);
  auto const exponent = result.find('e');
  if(exponent != std::string::npos) {
    size_t const first = exponent + 2u;   // After the exponent's sign.
    while(first + 1u < result.size() && result[first] == '0') {
      result.erase(first, 1u);
    }
  }
  else { // nothing to do
  }
  return result;
}

uint32_t gMismatches = 0u;

void compareAllDigits(double const aValue) {
  for(uint8_t digits = 1u; digits <= cgMaxDigits; ++digits) {
    std::string const mine = convert(aValue, digits);
    std::string const expected = reference(aValue, digits);
    if(mine != expected) {
      if(gMismatches < 10u) {
        std::fprintf(stderr, "%a with %u digits: %s instead of %s\n", aValue, digits, mine.c_str(), expected.c_str());
      }
      else { // nothing to do
      }
      ++gMismatches;
    }
    else { // nothing to do
    }
  }
}

double fromBits(uint64_t const aBits) {
  double result;
  std::memcpy(&result, &aBits, sizeof(result));
  return result;
}

int main() {
  std::mt19937_64 random(20261107u);

  // Random bit patterns cover every exponent, subnormals included.
  for(uint32_t i = 0u; i < 20000u; ++i) {
    double const value = fromBits(random());
    if(std::isfinite(value) && value != 0.0) {
      compareAllDigits(value);
    }
    else { // nothing to do
    }
  }
  // Everyday values: integers, short decimals, and some at other scales.
  for(int32_t i = 0; i < 5000; ++i) {
    int32_t const integer = static_cast<int32_t>(random() % 2000001u) - 1000000;
    compareAllDigits(static_cast<double>(integer));
    compareAllDigits(integer / 1000.0);
    compareAllDigits(integer * 1e-30);
    compareAllDigits(integer * 1e30);
  }
  // Powers of ten and their neighbours, where the decimal exponent changes.
  for(int32_t power = -323; power <= 308; ++power) {
    char text[16];
    std::snprintf(text, sizeof(text), "1e%d", power);
    double const value = std::strtod(text, nullptr);
    compareAllDigits(value);
    compareAllDigits(std::nextafter(value, 0.0));
    compareAllDigits(std::nextafter(value, 2.0 * value));
  }
  for(double const value : {DBL_MAX, -DBL_MAX, DBL_MIN, std::numeric_limits<double>::denorm_min(), DBL_EPSILON, 1.0 - DBL_EPSILON / 2.0}) {
    compareAllDigits(value);
  }
  // Exact ties round to even, from both sides of the decimal point.
  for(double const value : {0.125, 0.375, 2.5, 3.5, 1250.0, 1350.0, 125000.0, 1.25e20, 1.35e22, 999999.5, -0.125}) {
    compareAllDigits(value);
  }
  CHECK(gMismatches == 0u);

  // float goes through double without loss, 8 digits by default.
  for(uint32_t i = 0u; i < 20000u; ++i) {
    uint32_t const bits = static_cast<uint32_t>(random());
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    if(std::isfinite(value)) {
      CHECK(convert(value, 6u) == convert(static_cast<double>(value), 6u));
    }
    else { // nothing to do
    }
  }
  CHECK(convert(1.23456789f, 0u) == "1.2345679e+0");
  CHECK(convert(-1.23456789, 0u) == "-1.234567890000000e+0");   // double, 16 digits by default.
  CHECK(convert(1.23456789L, 0u) == reference(static_cast<double>(1.23456789L), cgMaxDigits));   // long double, via double.

  // The old code clamped these to 9 instead of carrying.
  CHECK(convert(0.99999, 3u) == "1.00e+0");
  CHECK(convert(9.9999, 4u) == "1.000e+1");
  CHECK(convert(-99.96, 3u) == "-1.00e+2");
  CHECK(convert(999999.5, 6u) == "1.00000e+6");   // An exact tie rounding up to even.

  // Signs of the special values, and the sign column of aligned conversion.
  double const infinity = std::numeric_limits<double>::infinity();
  double const nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(convert(0.0, 0u) == "0");
  CHECK(convert(-0.0, 0u) == "-0");
  CHECK(convert(infinity, 0u) == "inf");
  CHECK(convert(-infinity, 0u) == "-inf");
  CHECK(convert(nan, 0u) == "nan");
  CHECK(convert(-nan, 0u) == "-nan");
  CHECK(convert<AlignedConverter>(1.5, 2u) == " 1.5e+0");
  CHECK(convert<AlignedConverter>(-1.5, 2u) == "-1.5e+0");
  CHECK(convert<AlignedConverter>(infinity, 0u) == " inf");

  // One digit has no dot, and more than 17 are no more precise.
  CHECK(convert(2.5, 1u) == "2e+0");
  CHECK(convert(0.1, 20u) == reference(0.1, cgMaxDigits));
  return finish();
}
