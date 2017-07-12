/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXPParser.h"
#include <cmath>

namespace libqxp
{

QXPParser::QXPParser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter,
                     const QXPHeader &commonHeader, QXPHeader &header)
  : m_input(input)
  , be(commonHeader.isBigEndian())
  , m_blockParser(input, commonHeader)
  , m_collector(painter)
  , m_colors()
  , m_fonts()
  , m_header(header)
{
  // default colors, in case parsing fails
  m_colors[0] = Color(255, 255, 255); // white
  m_colors[1] = Color(0, 0, 0); // black
  m_colors[2] = Color(255, 0, 0); // red
  m_colors[3] = Color(0, 255, 0); // green
  m_colors[4] = Color(0, 0, 255); // blue
  m_colors[5] = Color(1, 160, 198); // cyan
  m_colors[6] = Color(239, 4, 127); // magenta
  m_colors[7] = Color(255, 255, 0); // yellow
  m_colors[8] = Color(0, 0, 0); // registration
}

bool QXPParser::parse()
{
  if (!m_header.load(m_input) || !m_header.isSupported())
    return false;

  m_collector.startDocument();

  auto docStream = m_blockParser.getChain(3);
  if (!parseDocument(docStream))
    return false;

  m_collector.endDocument();

  return true;
}

void QXPParser::skipRecord(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  unsigned length = readU32(stream, be);
  if (length > 0)
  {
    skip(stream, length);
  }
}

void QXPParser::parseFonts(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  auto end = readRecordEndOffset(stream);

  try
  {
    unsigned count = readU16(stream, be);
    for (unsigned i = 0; i < count; ++i)
    {
      int index = readS16(stream, be);
      if (m_header.version() >= QXP_4)
      {
        skip(stream, 2);
      }
      auto name = readPlatformString(stream, be);
      readPlatformString(stream, be); // skip

      m_fonts[index] = name;
    }
  }
  catch (...)
  {
    QXP_DEBUG_MSG(("Failed to parse fonts, offset %ld\n", stream->tell()));
  }

  seek(stream, end);
}

uint32_t QXPParser::readRecordEndOffset(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  unsigned length = readU32(stream, be);
  return stream->tell() + length;
}

uint8_t QXPParser::readColorComp(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  return uint8_t(std::round(255 * readFloat16(stream, be)));
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
