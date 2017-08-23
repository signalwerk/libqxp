/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXPTextParser.h"

#include "QXPHeader.h"
#include "QXPTypes.h"

namespace libqxp
{

using std::make_shared;

QXPTextParser::QXPTextParser(const std::shared_ptr<librevenge::RVNGInputStream> &input, const std::shared_ptr<QXPHeader> &header)
  : m_header(header)
  , be(header->isBigEndian())
  , m_encoding(header->encoding())
  , m_blockParser(input, header)
{
}

std::shared_ptr<Text> QXPTextParser::parseText(unsigned index,
                                               const std::vector<std::shared_ptr<CharFormat>> &charFormats,
                                               const std::vector<std::shared_ptr<ParagraphFormat>> &paragraphFormats)
{
  auto infoStream = m_blockParser.getChain(index);

  auto text = make_shared<Text>();
  text->encoding = m_encoding;

  skip(infoStream, 4);

  {
    const unsigned blocksSpecLength = readU32(infoStream, be);
    const long end = infoStream->tell() + blocksSpecLength;
    while (infoStream->tell() < end)
    {
      const unsigned blockIndex = readU32(infoStream, be);
      const unsigned length = m_header->version() >= QXP_4 ? readU32(infoStream, be) : readU16(infoStream, be);
      auto blockStream = m_blockParser.getBlock(blockIndex);
      text->text += readString(blockStream, length);
    }
  }

  {
    const unsigned charSpecLength = readU32(infoStream, be);
    const long end = infoStream->tell() + charSpecLength;
    while (infoStream->tell() < end)
    {
      const unsigned charFormatIndex = m_header->version() >= QXP_4 ? readU32(infoStream, be) : readU16(infoStream, be);
      const unsigned length = readU32(infoStream, be);
      const unsigned startIndex = text->charFormats.empty() ? 0 : text->charFormats.back().afterEndIndex();

      std::shared_ptr<CharFormat> charFormat;
      if (charFormatIndex >= charFormats.size())
      {
        QXP_DEBUG_MSG(("Char format %u not found\n", charFormatIndex));
        if (!charFormats.empty())
        {
          charFormat = charFormats[0];
        }
      }
      else
      {
        charFormat = charFormats[charFormatIndex];
      }
      text->charFormats.emplace_back(CharFormatSpec(charFormat, startIndex, length));
    }
  }

  {
    const unsigned paragraphSpecLength = readU32(infoStream, be);
    const long end = infoStream->tell() + paragraphSpecLength;
    while (infoStream->tell() < end)
    {
      const unsigned paragraphFormatIndex = m_header->version() >= QXP_4 ? readU32(infoStream, be) : readU16(infoStream, be);
      const unsigned length = readU32(infoStream, be);
      const unsigned startIndex = text->paragraphs.empty() ? 0 : text->paragraphs.back().afterEndIndex();

      std::shared_ptr<ParagraphFormat> paragraphFormat;
      if (paragraphFormatIndex >= paragraphFormats.size())
      {
        QXP_DEBUG_MSG(("Paragraph format %u not found\n", paragraphFormatIndex));
        if (!paragraphFormats.empty())
        {
          paragraphFormat = paragraphFormats[0];
        }
      }
      else
      {
        paragraphFormat = paragraphFormats[paragraphFormatIndex];
      }
      text->paragraphs.emplace_back(ParagraphSpec(paragraphFormat, startIndex, length));
    }
  }

  return text;
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
