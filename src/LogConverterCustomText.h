#ifndef NOWTECH_LOG_CONVERTER_CUSTOM_CHARACTER
#define NOWTECH_LOG_CONVERTER_CUSTOM_CHARACTER

#include "LogNumericSystem.h"
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace nowtech::log {

/// Independent of STL
template<typename tMessage, bool tArchitecture64, uint8_t tAppendStackBufferSize, bool tAppendBasePrefix, bool tAlignSigned>
class ConverterCustomText final {
public:
  using tMessage_          = tMessage;
  using ConversionResult   = char;
  using Iterator           = char*;
  static constexpr Iterator csNullIterator = nullptr; // Used in SenderVoid to return void begin-end pair.
  static constexpr bool     csSupportFloatingPoint = tMessage::csSupportFloatingPoint;

private:
  using IntegerConversionUnsigned = std::conditional_t<tArchitecture64, uint64_t, uint32_t>;
  using IntegerConversionSigned   = std::conditional_t<tArchitecture64, int64_t, int32_t>;

  static constexpr uint8_t csMaxDigitCountFloat      =  8u;
  static constexpr uint8_t csMaxDigitCountDouble     = 16u;
  static constexpr uint8_t csMaxDigitCountLongDouble = 17u;   // Converted with double precision.
  static constexpr uint8_t csMaxDigitCount           = 17u;   // All a double has, more would be noise.

  static constexpr char csNumericError            = '#';
  static constexpr char csEndOfLine               = '\n';
  static constexpr char csNumericFill             = '0';
  static constexpr char csNumericMarkBinary       = 'b';
  static constexpr char csNumericMarkHexadecimal  = 'x';
  static constexpr char csNumericMarkOther        = '!';
  static constexpr char csMinus                   = '-';
  static constexpr char csSpace                   = ' ';
  static constexpr char csFractionDot             = '.';
  static constexpr char csPlus                    = '+';
  static constexpr char csScientificE             = 'e';

  inline static constexpr char csNan[]            = "nan";
  inline static constexpr char csInf[]            = "inf";
  inline static constexpr char csTrue[]           = "true";
  inline static constexpr char csFalse[]          = "false";

  Iterator       mBegin;
  Iterator const mEnd;

public:
  ConverterCustomText(Iterator aBegin, Iterator const aEnd) noexcept
  : mBegin(aBegin)
  , mEnd(aEnd) {
  }

  ConverterCustomText(ConverterCustomText const &) = delete;
  ConverterCustomText(ConverterCustomText &&) = delete;
  ConverterCustomText& operator=(ConverterCustomText const &) = delete;
  ConverterCustomText& operator=(ConverterCustomText &&) = delete;

  Iterator end() const noexcept {
    return mBegin;
  }

  void convert(ShutdownMessageContent const, uint8_t const, uint8_t const) noexcept { // nothing to do
  }

  template <typename tDummy = void>
  auto convert(float const aValue, uint8_t const, uint8_t const aFill) noexcept -> std::enable_if_t<csSupportFloatingPoint, tDummy> {
    append(static_cast<double>(aValue), aFill == 0u ? csMaxDigitCountFloat : aFill);
    appendSpace();
  }

  template <typename tDummy = void>
  auto convert(double const aValue, uint8_t const, uint8_t const aFill) noexcept -> std::enable_if_t<csSupportFloatingPoint, tDummy> {
    append(aValue, aFill == 0u ? csMaxDigitCountDouble : aFill);
    appendSpace();
  }

  template <typename tDummy = void>
  auto convert(long double const aValue, uint8_t const, uint8_t const aFill) noexcept -> std::enable_if_t<csSupportFloatingPoint, tDummy> {
    append(static_cast<double>(aValue), aFill == 0u ? csMaxDigitCountLongDouble : aFill);
    appendSpace();
  }

  void convert(uint8_t const aValue, uint8_t const aBase, uint8_t const aFill) noexcept {
    append(static_cast<IntegerConversionUnsigned>(aValue), static_cast<IntegerConversionUnsigned>(aBase), aFill);
    appendSpace();
  }
  
  void convert(uint16_t const aValue, uint8_t const aBase, uint8_t const aFill) noexcept {
    append(static_cast<IntegerConversionUnsigned>(aValue), static_cast<IntegerConversionUnsigned>(aBase), aFill);
    appendSpace();
  }
  
  void convert(uint32_t const aValue, uint8_t const aBase, uint8_t const aFill) noexcept {
    append(static_cast<IntegerConversionUnsigned>(aValue), static_cast<IntegerConversionUnsigned>(aBase), aFill);
    appendSpace();
  }
  
  void convert(uint64_t const aValue, uint8_t const aBase, uint8_t const aFill) noexcept {
    append(aValue, static_cast<uint64_t>(aBase), aFill);
    appendSpace();
  }

  void convert(int8_t const aValue, uint8_t const aBase, uint8_t const aFill) noexcept {
    append(static_cast<IntegerConversionSigned>(aValue), static_cast<IntegerConversionSigned>(aBase), aFill);
    appendSpace();
  }
  
  void convert(int16_t const aValue, uint8_t const aBase, uint8_t const aFill) noexcept {
    append(static_cast<IntegerConversionSigned>(aValue), static_cast<IntegerConversionSigned>(aBase), aFill);
    appendSpace();
  }
  
  void convert(int32_t const aValue, uint8_t const aBase, uint8_t const aFill) noexcept {
    append(static_cast<IntegerConversionSigned>(aValue), static_cast<IntegerConversionSigned>(aBase), aFill);
    appendSpace();
  }
  
  void convert(int64_t const aValue, uint8_t const aBase, uint8_t const aFill) noexcept {
    append(aValue, static_cast<int64_t>(aBase), aFill);
    appendSpace();
  }

  void convert(char const aValue, uint8_t const, uint8_t const) noexcept {
    append(aValue);
    appendSpace();
  }

  void convert(char const * const aValue, uint8_t const, uint8_t const) noexcept {
    append(aValue);
    appendSpace();
  }

  void convert(bool const aValue, uint8_t const, uint8_t const) noexcept {
    append(aValue ? csTrue : csFalse);
    appendSpace();
  }

  void convert(char const * const aValue, uint8_t const aFill) noexcept {   // extra function for MessageCompact
    append(aValue);
    if(aFill < LogFormat::csFillValueStoreString) {   // Antipattern to use the fill for other purposes, but we go for space saving.
      appendSpace();
    }
    else { // nothing to do
    }
  }

  void convert(std::array<char, tMessage::csPayloadSize> const &aValue, uint8_t const, uint8_t const aFill) noexcept {
    convert(aValue.data(), aFill);
  }

  void terminateSequence() noexcept {
    append(csEndOfLine);
  }

private:
  void appendSpace() noexcept {
    append(csSpace);
  }

  void append(char const aValue) noexcept {
    if(mBegin < mEnd) {
      *mBegin = aValue;
      ++mBegin;
    }
    else { // nothing to do
    }
  }

  void append(char const * const aValue) noexcept {
    char const * where = aValue;
    while(*where && mBegin < mEnd) { // Don't use append to be able to escape when buffer is full.
      *mBegin = *where;
      mBegin++;
      where++;
    }
  }

  inline static constexpr char csDigit2char[NumericSystem::csBaseMax] = {
    '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
  };

  template<typename tValue>
  void append(tValue const aValue, tValue const aBase, uint8_t const aFill) noexcept {
    tValue tmpValue = aValue;
    uint8_t tmpFill = aFill;
    if((aBase <= NumericSystem::csInvalid) || (aBase > NumericSystem::csBaseMax)) {
      append(csNumericError);
      return;
    }
    else { // nothing to do
    }
    if constexpr(tAppendBasePrefix) {
      if (aBase == 2u) {
        append(csNumericFill);
        append(csNumericMarkBinary);
      }
      else if(aBase == 16u) {
        append(csNumericFill);
        append(csNumericMarkHexadecimal);
      }
      else if(aBase != 10u) {
        append(csNumericFill);
        append(csNumericMarkOther);
      }
      else { // nothing to do
      }
    }
    char tmpBuffer[tAppendStackBufferSize];
    uint8_t where = 0u;
    bool negative = aValue < 0;
    do {
      tValue mod = tmpValue % aBase;
      if(mod < 0) {
        mod = -mod;
      }
      else { // nothing to do
      }
      tmpBuffer[where] = csDigit2char[mod];
      ++where;
      tmpValue /= aBase;
    } while((tmpValue != 0) && (where < tAppendStackBufferSize));
    if(where >= tAppendStackBufferSize) {
      append(csNumericError);
      return;
    }
    else { // nothing to do
    }
    if(negative) {
      append(csMinus);
    }
    else if(tAlignSigned && (aFill > 0u)) {
      append(csSpace);
    }
    else { // nothing to do
    }
    if(tmpFill > where) {
      tmpFill -= where;
      while(tmpFill > 0u) {
        append(csNumericFill);
        --tmpFill;
      }
    }
    for(--where; where > 0u; --where) {
      append(tmpBuffer[where]);
    }
    append(tmpBuffer[0]);
  }

  static constexpr int32_t  csPowerStep          = 16;    // Decades between table entries, the rest is multiplied exactly.
  static constexpr int32_t  csFirstPowerIndex    = -20;   // j of the first entry, the table covers every double.
  static constexpr uint64_t csHalf               = 1ull << 63u;
  static constexpr uint64_t csTieWindow          = 1ull << 60u;   // Table error is below 2^56 in the lower fraction word.
  static constexpr uint32_t csMantissaBits       = 52u;
  static constexpr uint32_t csExponentMask       = 0x7ffu;
  static constexpr int32_t  csExponentBias       = 1075;  // Bias and mantissa width.

  // 10^(16 j) is about (high 2^64 + low) 2^exponent, the mantissa in [2^127, 2^128), rounded to nearest.
  // Generated with exact rational arithmetic. Two arrays, as one of structs would be padded.
  inline static constexpr uint64_t csPowerMantissas[][2] = {
    {0xfd00b897478238d0u, 0x8920b098955522b5u},   // 1e-320
    {0x8c71dcd9ba0b4925u, 0x9ff0c08b7f1d0b15u},   // 1e-304
    {0x9becce62836ac577u, 0x4ee367f9430aec33u},   // 1e-288
    {0xad1c8eab5ee43b66u, 0xda3243650005eecfu},   // 1e-272
    {0xc0314325637a1939u, 0xfa911155fefb5309u},   // 1e-256
    {0xd5605fcdcf32e1d6u, 0xfb1e4a9a90880a65u},   // 1e-240
    {0xece53cec4a314ebdu, 0xa4f8bf5635246428u},   // 1e-224
    {0x8380dea93da4bc60u, 0x4247cb9e59f71e6du},   // 1e-208
    {0x91ff83775423cc06u, 0x7b6306a34627ddcfu},   // 1e-192
    {0xa21727db38cb002fu, 0xb8ada00e5a506a7du},   // 1e-176
    {0xb3f4e093db73a093u, 0x59ed216765690f57u},   // 1e-160
    {0xc7caba6e7c5382c8u, 0xfe64a52ee96b8fc1u},   // 1e-144
    {0xddd0467c64bce4a0u, 0xac7cb3f6d05ddbdfu},   // 1e-128
    {0xf64335bcf065d37du, 0x4d4617b5ff4a16d6u},   // 1e-112
    {0x88b402f7fd75539bu, 0x11dbcb0218ebb414u},   // 1e-96
    {0x97c560ba6b0919a5u, 0xdccd879fc967d41au},   // 1e-80
    {0xa87fea27a539e9a5u, 0x3f2398d747b36224u},   // 1e-64
    {0xbb127c53b17ec159u, 0x5560c018580d5d52u},   // 1e-48
    {0xcfb11ead453994bau, 0x67de18eda5814af2u},   // 1e-32
    {0xe69594bec44de15bu, 0x4c2ebe687989a9b4u},   // 1e-16
    {0x8000000000000000u, 0x0000000000000000u},   // 1e0
    {0x8e1bc9bf04000000u, 0x0000000000000000u},   // 1e16
    {0x9dc5ada82b70b59du, 0xf020000000000000u},   // 1e32
    {0xaf298d050e4395d6u, 0x9670b12b7f410000u},   // 1e48
    {0xc2781f49ffcfa6d5u, 0x3cbf6b71c76b25fbu},   // 1e64
    {0xd7e77a8f87daf7fbu, 0xdc33745ec97be906u},   // 1e80
    {0xefb3ab16c59b14a2u, 0xc5cfe94ef3ea101eu},   // 1e96
    {0x850fadc09923329eu, 0x03e2cf6bc604ddb0u},   // 1e112
    {0x93ba47c980e98cdfu, 0xc66f336c36b10137u},   // 1e128
    {0xa402b9c5a8d3a6e7u, 0x5f16206c9c6209a6u},   // 1e144
    {0xb616a12b7fe617aau, 0x577b986b314d6009u},   // 1e160
    {0xca28a291859bbf93u, 0x7d7b8f7503cfdcffu},   // 1e176
    {0xe070f78d3927556au, 0x85bbe253f47b1417u},   // 1e192
    {0xf92e0c3537826145u, 0xa7709a56ccdf8a83u},   // 1e208
    {0x8a5296ffe33cc92fu, 0x82bd6b70d99aaa70u},   // 1e224
    {0x9991a6f3d6bf1765u, 0xacca6da1e0a8ef29u},   // 1e240
    {0xaa7eebfb9df9de8du, 0xddbb901b98feeab8u},   // 1e256
    {0xbd49d14aa79dbc82u, 0x4b2d8644d8a74e19u},   // 1e272
    {0xd226fc195c6a2f8cu, 0x73832eec6fff3112u},   // 1e288
    {0xe950df20247c83fdu, 0x47c6b82ef32a2069u},   // 1e304
    {0x81842f29f2cce375u, 0xe6a1158300d46640u},   // 1e320
    {0x8fcac257558ee4e6u, 0x213a4f0aa5e8a7b2u},   // 1e336
  };

  inline static constexpr int16_t csPowerExponents[] = {
    -1191, -1137, -1084, -1031, -978, -925, -872, -818, -765, -712, -659, -606,
    -553, -500, -446, -393, -340, -287, -234, -181, -127, -74, -21, 32,
    85, 138, 191, 245, 298, 351, 404, 457, 510, 563, 617, 670,
    723, 776, 829, 882, 936, 989
  };

  inline static constexpr uint64_t csSmallPowersOfTen[csMaxDigitCount + 1u] = {
    1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u, 10000000u, 100000000u, 1000000000u, 10000000000u,
    100000000000u, 1000000000000u, 10000000000000u, 100000000000000u, 1000000000000000u, 10000000000000000u,
    100000000000000000u
  };

  /// 64 x 64 bit to 128 bit from 32 bit halves, as 32 bit MCUs have nothing wider.
  static void multiply(uint64_t const aLeft, uint64_t const aRight, uint64_t &aHigh, uint64_t &aLow) noexcept {
    uint64_t const leftLow   = aLeft & 0xffffffffu;
    uint64_t const leftHigh  = aLeft >> 32u;
    uint64_t const rightLow  = aRight & 0xffffffffu;
    uint64_t const rightHigh = aRight >> 32u;
    uint64_t const lowLow    = leftLow * rightLow;
    uint64_t const lowHigh   = leftLow * rightHigh;
    uint64_t const highLow   = leftHigh * rightLow;
    uint64_t const middle    = (lowLow >> 32u) + (lowHigh & 0xffffffffu) + (highLow & 0xffffffffu);
    aLow  = (middle << 32u) | (lowLow & 0xffffffffu);
    aHigh = leftHigh * rightHigh + (lowHigh >> 32u) + (highLow >> 32u) + (middle >> 32u);
  }

  /// Adds aLeft x aRight to aSum, starting at 64 bit word aWord.
  static void multiplyAdd(uint64_t (&aSum)[4], uint32_t const aWord, uint64_t const aLeft, uint64_t const aRight) noexcept {
    uint64_t high;
    uint64_t low;
    multiply(aLeft, aRight, high, low);
    uint64_t carry = 0u;
    for(uint32_t i = aWord; i < 4u; ++i) {
      uint64_t const addend = (i == aWord) ? low : ((i == aWord + 1u) ? high : 0u);
      uint64_t const sum = aSum[i] + addend;
      uint64_t const sumWithCarry = sum + carry;
      carry = ((sum < addend) ? 1u : 0u) + ((sumWithCarry < sum) ? 1u : 0u);
      aSum[i] = sumWithCarry;
    }
  }

  /// The 64 bits of aValue starting at bit aLowestBit, bits outside it read as 0.
  static uint64_t bitsFrom(uint64_t const (&aValue)[4], int32_t const aLowestBit) noexcept {
    uint64_t result = 0u;
    if(aLowestBit >= 0) {
      uint32_t const word = static_cast<uint32_t>(aLowestBit) / 64u;
      uint32_t const shift = static_cast<uint32_t>(aLowestBit) % 64u;
      if(word < 4u) {
        result = aValue[word] >> shift;
        if(shift > 0u && word < 3u) {
          result |= aValue[word + 1u] << (64u - shift);
        }
        else { // nothing to do
        }
      }
      else { // nothing to do
      }
    }
    else if(aLowestBit > -64) {
      result = aValue[0] << static_cast<uint32_t>(-aLowestBit);
    }
    else { // nothing to do
    }
    return result;
  }

  static constexpr int32_t floorDivide(int32_t const aValue, int32_t const aDivisor) noexcept {
    return (aValue >= 0) ? (aValue / aDivisor) : -((-aValue + aDivisor - 1) / aDivisor);
  }

  /// floor(aExponent log10(2)), maybe one less, the caller corrects that.
  static constexpr int32_t floorLog10Pow2(int32_t const aExponent) noexcept {
    return (aExponent >= 0) ? ((aExponent * 78913) >> 18) : -((-aExponent * 78913 + (1 << 18) - 1) >> 18);
  }

  /// round(aMantissa 2^aExponent 10^aPower), ties to even. 10^aPower is a table entry times an exact 10^r,
  /// so the error comes from the 128 bit table entries only. Exact ties come out within csTieWindow of
  /// one half, other values practically never come that close.
  static uint64_t scale(uint64_t const aMantissa, int32_t const aExponent, int32_t const aPower) noexcept {
    int32_t const index = floorDivide(aPower, csPowerStep);
    int32_t const rest = aPower - index * csPowerStep;
    uint64_t exactHigh;
    uint64_t exactLow;
    multiply(aMantissa, csSmallPowersOfTen[rest], exactHigh, exactLow);   // Below 2^103, exact.
    uint64_t const (&power)[2] = csPowerMantissas[index - csFirstPowerIndex];
    uint64_t product[4] = {0u, 0u, 0u, 0u};
    multiplyAdd(product, 0u, exactLow, power[1]);
    multiplyAdd(product, 1u, exactLow, power[0]);
    multiplyAdd(product, 1u, exactHigh, power[1]);
    multiplyAdd(product, 2u, exactHigh, power[0]);
    int32_t const shift = -(aExponent + csPowerExponents[index - csFirstPowerIndex]);
    uint64_t result = bitsFrom(product, shift);
    uint64_t const fractionHigh = bitsFrom(product, shift - 64);
    uint64_t const fractionLow = bitsFrom(product, shift - 128);
    bool const tie = (fractionHigh == csHalf && fractionLow <= csTieWindow) || (fractionHigh == csHalf - 1u && fractionLow >= ~csTieWindow);
    if(tie) {
      result += result & 1u;
    }
    else if(fractionHigh >= csHalf) {
      ++result;
    }
    else { // nothing to do
    }
    return result;
  }

  /// Scientific notation with aDigitsNeeded significant digits, correctly rounded, using integer
  /// arithmetic only. float and long double come here converted to double.
  void append(double const aValue, uint8_t const aDigitsNeeded) noexcept {
    static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == sizeof(uint64_t));
#if defined(__FLOAT_WORD_ORDER__) && defined(__BYTE_ORDER__)   // The bits are read as an integer.
    static_assert(__FLOAT_WORD_ORDER__ == __BYTE_ORDER__, "double must use the integer byte order");
#endif
    uint64_t bits;
    std::memcpy(&bits, &aValue, sizeof(bits));
    uint32_t const biasedExponent = static_cast<uint32_t>(bits >> csMantissaBits) & csExponentMask;
    uint64_t mantissa = bits & ((1ull << csMantissaBits) - 1u);
    if((bits >> 63u) != 0u) {
      append(csMinus);
    }
    else if(tAlignSigned) {
      append(csSpace);
    }
    else { // nothing to do
    }
    if(biasedExponent == csExponentMask) {
      append((mantissa == 0u) ? csInf : csNan);
    }
    else if(biasedExponent == 0u && mantissa == 0u) {
      append(csNumericFill);
    }
    else {
      int32_t exponent;
      if(biasedExponent == 0u) {   // Subnormal, normalize it.
        exponent = 1 - csExponentBias;
        while(mantissa < (1ull << csMantissaBits)) {
          mantissa <<= 1u;
          --exponent;
        }
      }
      else {
        mantissa |= 1ull << csMantissaBits;
        exponent = static_cast<int32_t>(biasedExponent) - csExponentBias;
      }
      uint8_t const digits = (aDigitsNeeded == 0u) ? 1u : ((aDigitsNeeded > csMaxDigitCount) ? csMaxDigitCount : aDigitsNeeded);
      int32_t decimalExponent = floorLog10Pow2(exponent + static_cast<int32_t>(csMantissaBits));
      uint64_t scaled = 0u;
      for(uint8_t attempt = 0u; attempt < 3u; ++attempt) {   // The estimate may be one off, and rounding may carry.
        scaled = scale(mantissa, exponent, static_cast<int32_t>(digits) - 1 - decimalExponent);
        if(scaled >= csSmallPowersOfTen[digits]) {
          ++decimalExponent;
        }
        else if(scaled < csSmallPowersOfTen[digits - 1u]) {
          --decimalExponent;
        }
        else {
          break;
        }
      }
      char digitBuffer[csMaxDigitCount];
      for(uint8_t i = digits; i > 0u; --i) {
        digitBuffer[i - 1u] = csDigit2char[scaled % 10u];
        scaled /= 10u;
      }
      append(digitBuffer[0]);
      if(digits > 1u) {
        append(csFractionDot);
        for(uint8_t i = 1u; i < digits; ++i) {
          append(digitBuffer[i]);
        }
      }
      else { // nothing to do
      }
      append(csScientificE);
      if(decimalExponent >= 0) {
        append(csPlus);
      }
      else { // nothing to do
      }
      append(decimalExponent, static_cast<int32_t>(10), 0u);
    }
  }
};

}

#endif
