/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXP4Parser.h"
#include <librevenge-stream/librevenge-stream.h>
#include <memory>

namespace libqxp
{

using librevenge::RVNGStringStream;
using std::make_shared;

QXP4Parser::QXP4Parser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter,
                       const QXPHeader &commonHeader)
  : QXPParser(input, painter, commonHeader, m_header)
  , m_header()
  , m_paragraphTabStops()
{
}

bool QXP4Parser::parseDocument(const std::shared_ptr<librevenge::RVNGInputStream> &docStream)
{
  for (int i = 0; i < 5; ++i)
  {
    skipRecord(docStream);
  }

  parseFonts(docStream);

  skipRecord(docStream);

  parseColors(docStream);

  // don't need stylesheets, everything is included in the current style
  skipParagraphStylesheets(docStream);
  skipRecord(docStream);

  parseHJs(docStream);

  skipRecord(docStream);

  skipRecord(docStream);

  skipTemplates(docStream);

  parseCharFormats(docStream);

  parseTabStops(docStream);

  return true;
}

void QXP4Parser::parseColors(const std::shared_ptr<librevenge::RVNGInputStream> &docStream)
{
  unsigned length = readU32(docStream, be);
  auto stream = make_shared<RVNGStringStream>(readNBytes(docStream, length), length);

  try
  {
    skip(stream, 14);
    const unsigned blocksCount = readU16(stream, be);
    if (blocksCount == 0 || blocksCount * 4 > length)
    {
      QXP_DEBUG_MSG(("Invalid number of blocks %u\n", blocksCount));
      return;
    }
    skip(stream, 20);
    std::vector<ColorBlockSpec> blocks;
    blocks.resize(blocksCount + 1); // first dummy block
    for (unsigned i = 1; i <= blocksCount; ++i)
    {
      blocks[i] = parseColorBlockSpec(stream);
    }

    for (unsigned i = 2; i <= blocksCount; ++i)
    {
      seek(stream, blocks[i].offset);
      if (readU16(stream, be) + readU16(stream, be) == 6) // 04 02
      {
        parseColor(stream, blocks);
      }
    }
  }
  catch (...)
  {
    QXP_DEBUG_MSG(("Failed to parse colors, offset %ld\n", stream->tell()));
  }
}

QXP4Parser::ColorBlockSpec QXP4Parser::parseColorBlockSpec(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  const uint32_t info = readU32(stream, be);
  ColorBlockSpec spec;
  spec.offset = info & 0xFFFFFFF;
  spec.padding = (info >> 28) & 0x7;
  return spec;
}

void QXP4Parser::parseColor(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const std::vector<ColorBlockSpec> &blocks)
{
  skip(stream, 30);
  unsigned id = readU16(stream, be);

  skip(stream, 70);
  unsigned rgbBlockInd = readU16(stream, be);

  if (rgbBlockInd != 0)
  {
    if (rgbBlockInd >= blocks.size())
    {
      QXP_DEBUG_MSG(("RGB block %u not found\n", rgbBlockInd));
      return;
    }
    seek(stream, blocks[rgbBlockInd].offset + 16);
    Color color;
    color.red = readColorComp(stream);
    color.green = readColorComp(stream);
    color.blue = readColorComp(stream);
    m_colors[id] = color;
  }
}

void QXP4Parser::skipParagraphStylesheets(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  const unsigned end = readRecordEndOffset(stream);

  unsigned tabRecordsCount = 0;
  while (stream->tell() < end)
  {
    skip(stream, 90);
    const unsigned tabsCount = readU16(stream, be);
    if (tabsCount > 0)
    {
      tabRecordsCount++;
    }
    skip(stream, 152);
  }

  seek(stream, end);

  for (unsigned i = 0; i < tabRecordsCount; ++i)
  {
    skipRecord(stream);
  }
}

CharFormat QXP4Parser::parseCharFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  skip(stream, 8);

  CharFormat result;
  parseCommonCharFormatProps(stream, result);

  skip(stream, 4);

  unsigned colorId = readU16(stream, be);
  if (m_colors.find(colorId) == m_colors.end())
  {
    QXP_DEBUG_MSG(("Color %u not found\n", colorId));
  }
  else
  {
    result.color = m_colors[colorId];
  }

  skip(stream, 2);

  result.shade = readFraction(stream, be);

  skip(stream, 36);

  return result;
}

void QXP4Parser::skipTemplates(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  const unsigned indexEnd = readRecordEndOffset(stream);

  const unsigned count = readU32(stream, be);

  seek(stream, indexEnd);

  for (unsigned i = 0; i < count; ++i)
  {
    skipRecord(stream);
  }
}

void QXP4Parser::parseTabStops(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  const unsigned specLength = readU32(stream, be);
  if (specLength > getLength(stream) - stream->tell())
  {
    QXP_DEBUG_MSG(("Invalid tab stop spec length %u\n", specLength));
    throw ParseError();
  }

  std::vector<unsigned> tabStopsCounts;
  tabStopsCounts.resize(specLength / 8);
  for (auto countIt = tabStopsCounts.rbegin(); countIt != tabStopsCounts.rend(); ++countIt)
  {
    skip(stream, 2);
    const unsigned count = readU16(stream, be);
    if (count > (getLength(stream) - stream->tell()) / 8)
    {
      QXP_DEBUG_MSG(("Invalid tab stop count %u\n", count));
      throw ParseError();
    }
    *countIt = count;
    skip(stream, 4);
  }

  m_paragraphTabStops.resize(tabStopsCounts.size());
  unsigned i = 0;
  for (auto it = m_paragraphTabStops.rbegin(); it != m_paragraphTabStops.rend(); ++it)
  {
    skip(stream, 4);
    const unsigned tabStopsCount = tabStopsCounts[i++];
    it->resize(tabStopsCount);
    for (auto &tabStop : *it)
    {
      tabStop = parseTabStop(stream);
    }
  }
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
