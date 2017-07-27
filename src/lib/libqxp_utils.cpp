/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "libqxp_utils.h"

#ifdef DEBUG
#include <cstdarg>
#include <cstdio>
#endif

using std::string;

namespace libqxp
{

namespace
{

void checkStream(librevenge::RVNGInputStream *const input)
{
  if (!input || input->isEnd())
    throw EndOfStreamException();
}

struct SeekFailedException {};

}

#ifdef DEBUG
void debugPrint(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  va_end(args);
}
#endif

uint8_t readU8(librevenge::RVNGInputStream *input, bool /* bigEndian */)
{
  checkStream(input);

  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint8_t), numBytesRead);

  if (p && numBytesRead == sizeof(uint8_t))
    return *(uint8_t const *)(p);
  throw EndOfStreamException();
}

uint16_t readU16(librevenge::RVNGInputStream *input, bool bigEndian)
{
  checkStream(input);

  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint16_t), numBytesRead);

  if (p && numBytesRead == sizeof(uint16_t))
  {
    if (bigEndian)
      return static_cast<uint16_t>((uint16_t)p[1]|((uint16_t)p[0]<<8));
    return static_cast<uint16_t>((uint16_t)p[0]|((uint16_t)p[1]<<8));
  }
  throw EndOfStreamException();
}

uint32_t readU32(librevenge::RVNGInputStream *input, bool bigEndian)
{
  checkStream(input);

  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint32_t), numBytesRead);

  if (p && numBytesRead == sizeof(uint32_t))
  {
    if (bigEndian)
      return (uint32_t)p[3]|((uint32_t)p[2]<<8)|((uint32_t)p[1]<<16)|((uint32_t)p[0]<<24);
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
  }
  throw EndOfStreamException();
}

uint64_t readU64(librevenge::RVNGInputStream *input, bool bigEndian)
{
  checkStream(input);

  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint64_t), numBytesRead);

  if (p && numBytesRead == sizeof(uint64_t))
  {
    if (bigEndian)
      return (uint64_t)p[7]|((uint64_t)p[6]<<8)|((uint64_t)p[5]<<16)|((uint64_t)p[4]<<24)|((uint64_t)p[3]<<32)|((uint64_t)p[2]<<40)|((uint64_t)p[1]<<48)|((uint64_t)p[0]<<56);
    return (uint64_t)p[0]|((uint64_t)p[1]<<8)|((uint64_t)p[2]<<16)|((uint64_t)p[3]<<24)|((uint64_t)p[4]<<32)|((uint64_t)p[5]<<40)|((uint64_t)p[6]<<48)|((uint64_t)p[7]<<56);
  }
  throw EndOfStreamException();
}

int16_t readS16(librevenge::RVNGInputStream *input, bool bigEndian)
{
  return int16_t(readU16(input, bigEndian));
}

int32_t readS32(librevenge::RVNGInputStream *input, bool bigEndian)
{
  return int32_t(readU32(input, bigEndian));
}

double readFloat16(librevenge::RVNGInputStream *input, bool bigEndian)
{
  return readU16(input, bigEndian) / double(0x10000);
}

double readFraction(librevenge::RVNGInputStream *input, bool bigEndian)
{
  int32_t num = readS32(input, bigEndian);
  return (num >> 16) + ((num & 0xffff) / double(0x10000));
}

const unsigned char *readNBytes(librevenge::RVNGInputStream *const input, const unsigned long numBytes)
{
  checkStream(input);

  unsigned long readBytes = 0;
  const unsigned char *const s = input->read(numBytes, readBytes);

  if (numBytes != readBytes)
    throw EndOfStreamException();

  return s;
}

string readCString(librevenge::RVNGInputStream *input)
{
  checkStream(input);

  string str;
  unsigned char c = readU8(input);
  while (0 != c)
  {
    str.push_back(c);
    c = readU8(input);
  }

  return str;
}

string readPascalString(librevenge::RVNGInputStream *input)
{
  checkStream(input);

  const unsigned length = readU8(input);

  return readString(input, length);
}

std::string readString(librevenge::RVNGInputStream *input, const unsigned length)
{
  checkStream(input);

  string str;
  for (unsigned i = 0; length != i; ++i)
    str.push_back(readU8(input));

  return str;
}

std::string readPlatformString(librevenge::RVNGInputStream *input, bool bigEndian)
{
  return bigEndian ? readPascalString(input) : readCString(input);
}

void skip(librevenge::RVNGInputStream *input, unsigned long numBytes)
{
  checkStream(input);

  seekRelative(input, static_cast<long>(numBytes));
}

void seek(librevenge::RVNGInputStream *const input, const unsigned long pos)
{
  if (!input)
    throw EndOfStreamException();

  if (0 != input->seek(static_cast<long>(pos), librevenge::RVNG_SEEK_SET))
    throw SeekFailedException();
}

void seekRelative(librevenge::RVNGInputStream *const input, const long pos)
{
  if (!input)
    throw EndOfStreamException();

  if (0 != input->seek(pos, librevenge::RVNG_SEEK_CUR))
    throw SeekFailedException();
}

unsigned long getLength(librevenge::RVNGInputStream *const input)
{
  if (!input)
    throw SeekFailedException();

  const unsigned long begin = input->tell();
  unsigned long end = begin;

  if (0 == input->seek(0, librevenge::RVNG_SEEK_END))
    end = input->tell();
  else
  {
    // librevenge::RVNG_SEEK_END does not work. Use the harder way.
    while (!input->isEnd())
    {
      readU8(input);
      ++end;
    }
  }

  seek(input, begin);

  return end - begin;
}

uint8_t readU8(const std::shared_ptr<librevenge::RVNGInputStream> input, bool)
{
  return readU8(input.get());
}

uint16_t readU16(const std::shared_ptr<librevenge::RVNGInputStream> input, const bool bigEndian)
{
  return readU16(input.get(), bigEndian);
}

uint32_t readU32(const std::shared_ptr<librevenge::RVNGInputStream> input, const bool bigEndian)
{
  return readU32(input.get(), bigEndian);
}

uint64_t readU64(const std::shared_ptr<librevenge::RVNGInputStream> input, const bool bigEndian)
{
  return readU64(input.get(), bigEndian);
}

int16_t readS16(std::shared_ptr<librevenge::RVNGInputStream> input, bool bigEndian)
{
  return readS16(input.get(), bigEndian);
}

int32_t readS32(std::shared_ptr<librevenge::RVNGInputStream> input, bool bigEndian)
{
  return readS32(input.get(), bigEndian);
}

double readFloat16(std::shared_ptr<librevenge::RVNGInputStream> input, bool bigEndian)
{
  return readFloat16(input.get(), bigEndian);
}

double readFraction(std::shared_ptr<librevenge::RVNGInputStream> input, bool bigEndian)
{
  return readFraction(input.get(), bigEndian);
}

const unsigned char *readNBytes(const std::shared_ptr<librevenge::RVNGInputStream> input, const unsigned long numBytes)
{
  return readNBytes(input.get(), numBytes);
}

std::string readCString(const std::shared_ptr<librevenge::RVNGInputStream> input)
{
  return readCString(input.get());
}

std::string readPascalString(const std::shared_ptr<librevenge::RVNGInputStream> input)
{
  return readPascalString(input.get());
}

std::string readString(std::shared_ptr<librevenge::RVNGInputStream> input, const unsigned length)
{
  return readString(input.get(), length);
}

std::string readPlatformString(std::shared_ptr<librevenge::RVNGInputStream> input, bool bigEndian)
{
  return readPlatformString(input.get(), bigEndian);
}

void skip(const std::shared_ptr<librevenge::RVNGInputStream> input, const unsigned long numBytes)
{
  return skip(input.get(), numBytes);
}

void seek(const std::shared_ptr<librevenge::RVNGInputStream> input, const unsigned long pos)
{
  seek(input.get(), pos);
}

void seekRelative(const std::shared_ptr<librevenge::RVNGInputStream> input, const long pos)
{
  seekRelative(input.get(), pos);
}

unsigned long getLength(const std::shared_ptr<librevenge::RVNGInputStream> input)
{
  return getLength(input.get());
}

EndOfStreamException::EndOfStreamException()
{
  QXP_DEBUG_MSG(("Throwing EndOfStreamException\n"));
}

double normalizeDegAngle(double degAngle)
{
  while (degAngle < 0.0)
    degAngle += 360.0;
  while (degAngle > 360.0)
    degAngle -= 360.0;
  return degAngle;
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
