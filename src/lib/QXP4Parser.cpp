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
    skip(stream, 8);
    const unsigned dataEndOffset = readU32(stream, be);
    skip(stream, 8);
    std::map<unsigned, ColorBlockSpec> blocks;
    for (unsigned i = 1; i <= blocksCount; ++i)
    {
      blocks[i] = parseColorBlockSpec(stream);
    }
    for (unsigned i = 1; i <= blocksCount - 1; ++i)
    {
      blocks[i].nextOffset = blocks[i + 1].offset;
    }
    blocks[blocksCount].nextOffset = dataEndOffset;

    const unsigned firstBlockInd = parseColorsHeader(stream, blocks[1]);
    unsigned ind = firstBlockInd;
    while (ind != 0)
    {
      if (blocks.find(ind) == blocks.end())
      {
        QXP_DEBUG_MSG(("Block %u not found\n", ind));
        return;
      }
      ind = parseColor(stream, blocks[ind], blocks);
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

unsigned QXP4Parser::parseColorsHeader(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ColorBlockSpec &block)
{
  seek(stream, block.offset + 16);
  return readU16(stream, be);
}

unsigned QXP4Parser::parseColor(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP4Parser::ColorBlockSpec &block,
                                const std::map<unsigned, QXP4Parser::ColorBlockSpec> &blocks)
{
  seek(stream, block.offset);

  skip(stream, 6);
  unsigned nextBlockInd = readU16(stream, be);

  skip(stream, 26);
  unsigned id = readU16(stream, be);

  skip(stream, 70);
  unsigned rgbBlockInd = readU16(stream, be);

  if (rgbBlockInd != 0)
  {
    if (blocks.find(rgbBlockInd) == blocks.end())
    {
      QXP_DEBUG_MSG(("RGB block %u not found\n", rgbBlockInd));
    }
    else
    {
      seek(stream, blocks.at(rgbBlockInd).offset + 16);
      Color color;
      color.red = readColorComp(stream);
      color.green = readColorComp(stream);
      color.blue = readColorComp(stream);
      m_colors[id] = color;
    }
  }

  return nextBlockInd;
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
