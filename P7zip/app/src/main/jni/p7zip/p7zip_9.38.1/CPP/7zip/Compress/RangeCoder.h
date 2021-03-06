// Compress/RangeCoder.h
// 2013-01-10 : Igor Pavlov : Public domain

#ifndef __COMPRESS_RANGE_CODER_H
#define __COMPRESS_RANGE_CODER_H

#include "../Common/InBuffer.h"
#include "../Common/OutBuffer.h"

namespace NCompress {
namespace NRangeCoder {

const unsigned kNumTopBits = 24;
const UInt32 kTopValue = (1 << kNumTopBits);

class CEncoder
{
  UInt32 _cacheSize;
  Byte _cache;
public:
  UInt64 Low;
  UInt32 Range;
  COutBuffer Stream;
  bool Create(UInt32 bufSize) { return Stream.Create(bufSize); }

  void SetStream(ISequentialOutStream *stream) { Stream.SetStream(stream); }
  void Init()
  {
    Stream.Init();
    Low = 0;
    Range = 0xFFFFFFFF;
    _cacheSize = 1;
    _cache = 0;
  }

  void FlushData()
  {
    // Low += 1;
    for (int i = 0; i < 5; i++)
      ShiftLow();
  }

  HRESULT FlushStream() { return Stream.Flush();  }

  void Encode(UInt32 start, UInt32 size, UInt32 total)
  {
    Low += start * (Range /= total);
    Range *= size;
    while (Range < kTopValue)
    {
      Range <<= 8;
      ShiftLow();
    }
  }

  void ShiftLow()
  {
    if ((UInt32)Low < (UInt32)0xFF000000 || (unsigned)(Low >> 32) != 0)
    {
      Byte temp = _cache;
      do
      {
        Stream.WriteByte((Byte)(temp + (Byte)(Low >> 32)));
        temp = 0xFF;
      }
      while (--_cacheSize != 0);
      _cache = (Byte)((UInt32)Low >> 24);
    }
    _cacheSize++;
    Low = (UInt32)Low << 8;
  }
  
  void EncodeDirectBits(UInt32 value, int numBits)
  {
    for (numBits--; numBits >= 0; numBits--)
    {
      Range >>= 1;
      Low += Range & (0 - ((value >> numBits) & 1));
      if (Range < kTopValue)
      {
        Range <<= 8;
        ShiftLow();
      }
    }
  }

  void EncodeBit(UInt32 size0, UInt32 numTotalBits, UInt32 symbol)
  {
    UInt32 newBound = (Range >> numTotalBits) * size0;
    if (symbol == 0)
      Range = newBound;
    else
    {
      Low += newBound;
      Range -= newBound;
    }
    while (Range < kTopValue)
    {
      Range <<= 8;
      ShiftLow();
    }
  }

  UInt64 GetProcessedSize() { return Stream.GetProcessedSize() + _cacheSize + 4; }
};

class CDecoder
{
public:
  CInBuffer Stream;
  UInt32 Range;
  UInt32 Code;
  bool Create(UInt32 bufSize) { return Stream.Create(bufSize); }

  void Normalize()
  {
    while (Range < kTopValue)
    {
      Code = (Code << 8) | Stream.ReadByte();
      Range <<= 8;
    }
  }
  
  void SetStream(ISequentialInStream *stream) { Stream.SetStream(stream); }
  void Init()
  {
    Stream.Init();
    Code = 0;
    Range = 0xFFFFFFFF;
    for (int i = 0; i < 5; i++)
      Code = (Code << 8) | Stream.ReadByte();
  }

  UInt32 GetThreshold(UInt32 total)
  {
    return (Code) / (Range /= total);
  }

  void Decode(UInt32 start, UInt32 size)
  {
    Code -= start * Range;
    Range *= size;
    Normalize();
  }

  UInt32 DecodeDirectBits(int numTotalBits)
  {
    UInt32 range = Range;
    UInt32 code = Code;
    UInt32 result = 0;
    for (int i = numTotalBits; i != 0; i--)
    {
      range >>= 1;
      /*
      result <<= 1;
      if (code >= range)
      {
        code -= range;
        result |= 1;
      }
      */
      UInt32 t = (code - range) >> 31;
      code -= range & (t - 1);
      result = (result << 1) | (1 - t);

      if (range < kTopValue)
      {
        code = (code << 8) | Stream.ReadByte();
        range <<= 8;
      }
    }
    Range = range;
    Code = code;
    return result;
  }

  UInt32 DecodeBit(UInt32 size0, UInt32 numTotalBits)
  {
    UInt32 newBound = (Range >> numTotalBits) * size0;
    UInt32 symbol;
    if (Code < newBound)
    {
      symbol = 0;
      Range = newBound;
    }
    else
    {
      symbol = 1;
      Code -= newBound;
      Range -= newBound;
    }
    Normalize();
    return symbol;
  }

  UInt64 GetProcessedSize() { return Stream.GetProcessedSize(); }
};

}}

#endif
