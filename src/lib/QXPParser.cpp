/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXPParser.h"
#include "QXPContentCollector.h"
#include <cmath>
#include <memory>

namespace libqxp
{

using librevenge::RVNGStringStream;
using std::make_shared;

QXPParser::QXPParser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter,
                     const QXPHeader &commonHeader, QXPHeader &header)
  : m_input(input)
  , m_painter(painter)
  , be(commonHeader.isBigEndian())
  , m_blockParser(input, commonHeader)
  , m_textParser(input, commonHeader)
  , m_colors()
  , m_fonts()
  , m_charFormats()
  , m_paragraphFormats()
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

  QXPContentCollector collector(m_painter);

  collector.startDocument();

  auto docStream = m_blockParser.getChain(3);
  if (!parseDocument(docStream))
    return false;

  if (!parsePages(docStream, collector))
    return false;

  collector.endDocument();

  return true;
}

Color QXPParser::getColor(unsigned id, Color defaultColor) const
{
  auto it = m_colors.find(id);
  if (it == m_colors.end())
  {
    QXP_DEBUG_MSG(("Color %u not found\n", id));
    return defaultColor;
  }
  return (*it).second;
}

std::string QXPParser::getFont(unsigned id, std::string defaultFont) const
{
  auto it = m_fonts.find(id);
  if (it == m_fonts.end())
  {
    QXP_DEBUG_MSG(("Font %u not found\n", id));
    return defaultFont;
  }
  return (*it).second;
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
  const unsigned end = readRecordEndOffset(stream);

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

void QXPParser::parseHJs(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  skipRecord(stream);
}

void QXPParser::parseCharFormats(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  m_charFormats.clear();
  parseCollection(stream, [=]()
  {
    m_charFormats.emplace_back(make_shared<CharFormat>(parseCharFormat(stream)));
  });
}

void QXPParser::parseCommonCharFormatProps(const std::shared_ptr<librevenge::RVNGInputStream> &stream, CharFormat &result)
{
  const int fontIndex = readS16(stream, be);
  result.fontName = getFont(fontIndex).c_str();

  const unsigned flags = readU16(stream, be);
  result.bold = flags & 0x1;
  result.italic = flags & 0x2;
  result.underline = flags & 0x4;
  result.outline = flags & 0x8;
  result.shadow = flags & 0x10;
  result.superscript = flags & 0x20;
  result.subscript = flags & 0x40;
  result.superior = flags & 0x100;
  result.strike = flags & 0x200;
  result.allCaps = flags & 0x400;
  result.smallCaps = flags & 0x800;
  result.wordUnderline = flags & 0x1000;

  result.fontSize = readFraction(stream, be);
}

TabStop QXPParser::parseTabStop(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  TabStop tabStop;

  const uint8_t type = readU8(stream);
  switch (type)
  {
  default:
    QXP_DEBUG_MSG(("Unknown tab stop type %u\n", type));
  // fall-through intended: pick a default
  case 0:
    tabStop.type = TabStopType::LEFT;
    break;
  case 1:
    tabStop.type = TabStopType::CENTER;
    break;
  case 2:
    tabStop.type = TabStopType::RIGHT;
    break;
  case 3:
    tabStop.type = TabStopType::ALIGN;
    break;
  }

  const uint8_t alignChar = readU8(stream);
  switch (alignChar)
  {
  case 1:
    tabStop.alignChar = '.';
    break;
  case 2:
    tabStop.alignChar = ',';
    break;
  default:
    tabStop.alignChar = char(alignChar);
    break;
  }

  tabStop.fillChar = char(readU16(stream, be));

  tabStop.position = readFraction(stream, be);

  return tabStop;
}

void QXPParser::parseParagraphFormats(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  m_paragraphFormats.clear();
  parseCollection(stream, [=]()
  {
    m_paragraphFormats.emplace_back(make_shared<ParagraphFormat>(parseParagraphFormat(stream)));
  });
}

void QXPParser::parseCollection(const std::shared_ptr<librevenge::RVNGInputStream> stream, std::function<void()> itemHandler)
{
  const unsigned end = readRecordEndOffset(stream);

  try
  {
    while (stream->tell() < end)
    {
      itemHandler();
    }
  }
  catch (...)
  {
    QXP_DEBUG_MSG(("Failed to parse collection, offset %ld\n", stream->tell()));
  }

  seek(stream, end);
}

std::vector<PageSettings> QXPParser::parsePageSettings(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  skip(stream, 6);
  const unsigned count = readU16(stream, be);
  if (count == 0 || count > 2)
  {
    QXP_DEBUG_MSG(("Invalid page settings blocks count %u\n", count));
    throw ParseError();
  }
  skip(stream, 2);

  std::vector<PageSettings> pages;
  pages.resize(count);
  for (auto &page : pages)
  {
    page.offset.top = readFraction(stream, be);
    page.offset.left = readFraction(stream, be);
    page.offset.bottom = readFraction(stream, be);
    page.offset.right = readFraction(stream, be);
    skip(stream, 20);
    page.margin.top = readFraction(stream, be);
    page.margin.bottom = readFraction(stream, be);
    page.margin.left = readFraction(stream, be);
    page.margin.right = readFraction(stream, be);
    skip(stream, m_header.version() >= QXP_4 ? 12 : 8);
  }
  skip(stream, count * 12 + 12);

  if (!be)
  {
    skip(stream, 4);
  }
  const unsigned nameLength = readU32(stream, be);
  skip(stream, nameLength);

  return pages;
}

std::shared_ptr<Text> QXPParser::parseText(unsigned index, unsigned linkId, QXPCollector &collector)
{
  try
  {
    auto text = m_textParser.parseText(index, m_charFormats, m_paragraphFormats);
    collector.collectText(text, linkId);
    return text;
  }
  catch (...)
  {
    QXP_DEBUG_MSG(("Failed to parse text %u\n", index));
    return make_shared<Text>();
  }
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

Rect QXPParser::readObjectBBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  Rect bbox;
  bbox.top = readFraction(stream, be);
  bbox.left = readFraction(stream, be);
  bbox.bottom = readFraction(stream, be);
  bbox.right = readFraction(stream, be);
  return bbox;
}

Gradient QXPParser::readGradient(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const Color &color1)
{
  Gradient gradient;
  gradient.color1 = color1;

  skip(stream, m_header.version() >= QXP_4 ? 20 : 14);

  const uint8_t type = readU16(stream, be) & 0xff;
  switch (type)
  {
  default:
    QXP_DEBUG_MSG(("Unknown gradient type %u\n", type));
  // fall-through intended: pick a default
  case 0x10:
    gradient.type = GradientType::LINEAR;
    break;
  case 0x18:
    gradient.type = GradientType::MIDLINEAR;
    break;
  case 0x19:
    gradient.type = GradientType::RECTANGULAR;
    break;
  case 0x1a:
    gradient.type = GradientType::DIAMOND;
    break;
  case 0x1b:
    gradient.type = GradientType::CIRCULAR;
    break;
  case 0x1c:
    gradient.type = GradientType::FULLCIRCULAR;
    break;
  }
  skip(stream, 4);

  unsigned colorId;
  if (m_header.version() >= QXP_4)
  {
    colorId = readU16(stream, be);
  }
  else
  {
    colorId = readU8(stream);
    skip(stream, 1);
  }
  const double shade = readFraction(stream, be);
  gradient.color2 = getColor(colorId).applyShade(shade);

  gradient.angle = readFraction(stream, be);
  skip(stream, 4);

  return gradient;
}

HorizontalAlignment QXPParser::readHorAlign(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  const uint8_t align = readU8(stream);
  switch (align)
  {
  default:
    QXP_DEBUG_MSG(("Unknown hor. align %u\n", align));
  // fall-through intended: pick a default
  case 0:
    return HorizontalAlignment::LEFT;
  case 1:
    return HorizontalAlignment::CENTER;
  case 2:
    return HorizontalAlignment::RIGHT;
  case 3:
    return HorizontalAlignment::JUSTIFIED;
  case 4:
    return HorizontalAlignment::FORCED;
  }
}

VerticalAlignment QXPParser::readVertAlign(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  const uint8_t align = readU8(stream);
  switch (align)
  {
  default:
    QXP_DEBUG_MSG(("Unknown vert. align %u\n", align));
  // fall-through intended: pick a default
  case 0:
    return VerticalAlignment::TOP;
  case 1:
    return VerticalAlignment::CENTER;
  case 2:
    return VerticalAlignment::BOTTOM;
  case 3:
    return VerticalAlignment::JUSTIFIED;
  }
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
