/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXP33Parser.h"
#include "QXPTypes.h"

namespace libqxp
{

using librevenge::RVNGInputStream;

QXP33Parser::QXP33Parser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter,
                         const QXPHeader &commonHeader)
  : QXPParser(input, painter, commonHeader, m_header)
  , m_header()
{
}

bool QXP33Parser::parseDocument(const std::shared_ptr<librevenge::RVNGInputStream> &docStream)
{
  for (int i = 0; i < 4; ++i)
  {
    skipRecord(docStream);
  }

  parseFonts(docStream);

  skipRecord(docStream);

  parseColors(docStream);

  skipRecord(docStream);
  skipRecord(docStream); // don't need stylesheets, everything is included in the current style

  parseHJs(docStream);

  skipRecord(docStream);

  m_charFormats.clear();
  parseCollection(docStream, [=]()
  {
    m_charFormats.emplace_back(parseCharFormat(docStream));
  });

  return true;
}

void QXP33Parser::parseColors(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  const unsigned end = readRecordEndOffset(stream);

  try
  {
    skip(stream, 1);
    unsigned count = readU8(stream);
    skip(stream, 32);
    for (unsigned i = 0; i < count; ++i)
    {
      unsigned id = readU8(stream);
      skip(stream, 1);
      Color color;
      color.red = readColorComp(stream);
      color.green = readColorComp(stream);
      color.blue = readColorComp(stream);
      m_colors[id] = color;
      skip(stream, 42);
      readName(stream); // skip
    }
  }
  catch (...)
  {
    QXP_DEBUG_MSG(("Failed to parse colors, offset %ld\n", stream->tell()));
  }

  seek(stream, end);
}

void QXP33Parser::parseHJs(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  const unsigned end = readRecordEndOffset(stream);

  try
  {

  }
  catch (...)
  {
    QXP_DEBUG_MSG(("Failed to parse H&Js, offset %ld\n", stream->tell()));
  }

  seek(stream, end);
}

CharFormat QXP33Parser::parseCharFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  skip(stream, 2);

  CharFormat result;
  parseCommonCharFormatProps(stream, result);

  skip(stream, 4);

  unsigned colorId = readU8(stream);
  if (m_colors.find(colorId) == m_colors.end())
  {
    QXP_DEBUG_MSG(("Color %u not found\n", colorId));
  }
  else
  {
    result.color = m_colors[colorId];
  }

  skip(stream, 1);

  result.shade = readFraction(stream, be);

  skip(stream, 26);

  return result;
}

std::string QXP33Parser::readName(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  unsigned start = stream->tell();
  auto name = readPlatformString(stream, be);
  if ((stream->tell() - start) % 2 == 1)
  {
    skip(stream, 1);
  }
  return name;
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
