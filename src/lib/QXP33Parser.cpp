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
using std::make_shared;
using std::vector;

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

  parseCharFormats(docStream);

  parseParagraphFormats(docStream);

  skipRecord(docStream);

  return true;
}

bool QXP33Parser::parsePages(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXPCollector &collector)
{
  QXP33Deobfuscator deobfuscator(m_header.seed(), m_header.increment());
  QXPDummyCollector dummyCollector;

  unsigned ind = 0;
  while (true)
  {
    if (readU32(stream, be) == 0x9e)
    {
      break;
    }
    seekRelative(stream, -4);

    // don't output master pages, everything is included in normal pages
    QXPCollector &coll = ind < m_header.masterPagesCount() ? dummyCollector : collector;

    auto page = parsePage(stream);
    coll.startPage(page);

    for (unsigned i = 0; i < page.objectsCount; ++i)
    {
      parseObject(stream, deobfuscator, coll, page);
      deobfuscator.next();
    }

    coll.endPage();

    ind++;
  }

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

CharFormat QXP33Parser::parseCharFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  skip(stream, 2);

  CharFormat result;
  parseCommonCharFormatProps(stream, result);

  skip(stream, 4);

  const unsigned colorId = readU8(stream);

  skip(stream, 1);

  const double shade = readFraction(stream, be);
  result.color = getColor(colorId).applyShade(shade);

  skip(stream, 26);

  return result;
}

ParagraphFormat QXP33Parser::parseParagraphFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  ParagraphFormat result;

  skip(stream, 5);
  result.alignment = readHorAlign(stream);

  skip(stream, 8);

  result.margin.left = readFraction(stream, be);
  result.firstLineIndent = readFraction(stream, be);
  result.margin.right = readFraction(stream, be);
  result.leading = readFraction(stream, be);
  result.margin.top = readFraction(stream, be);
  result.margin.bottom = readFraction(stream, be);

  skip(stream, 218);

  return result;
}

Page QXP33Parser::parsePage(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  Page page;
  page.pageSettings = parsePageSettings(stream);
  page.objectsCount = readU32(stream, be);
  return page;
}

void QXP33Parser::parseObject(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXP33Deobfuscator &deobfuscator, QXPCollector &collector, const Page &page)
{
  const auto &header = parseObjectHeader(stream, deobfuscator);

  switch (header.contentType)
  {
  case ContentType::NONE:
    switch (header.shapeType)
    {
    case ShapeType::LINE:
    case ShapeType::ORTHOGONAL_LINE:
      parseLine(stream, header, collector);
      break;
    case ShapeType::RECTANGLE:
    case ShapeType::CORNERED_RECTANGLE:
    case ShapeType::OVAL:
    case ShapeType::POLYGON:
      parseEmptyBox(stream, header, collector);
      break;
    default:
      QXP_DEBUG_MSG(("Unsupported shape\n"));
      throw GenericException();
    }
    break;
  case ContentType::PICTURE:
    parsePictureBox(stream, header, collector);
    break;
  case ContentType::TEXT:
    parseTextBox(stream, header, collector);
    break;
  case ContentType::OBJECTS:
    parseGroup(stream, header, collector, page);
    break;
  default:
    QXP_DEBUG_MSG(("Unsupported content\n"));
    throw GenericException();
  }
}

QXP33Parser::ObjectHeader QXP33Parser::parseObjectHeader(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXP33Deobfuscator &deobfuscator)
{
  ObjectHeader result;

  skip(stream, 1);

  const unsigned colorId = readU8(stream);
  const double shade = readFraction(stream, be);
  const auto color = getColor(colorId).applyShade(shade);

  result.contentIndex = deobfuscator(uint16_t(readU32(stream, be) & 0xffff));

  const uint8_t flags = readU8(stream);
  const bool noColor = flags & 0x1;
  if (!noColor)
  {
    result.fill = color;
  }

  skip(stream, 1);

  result.rotation = readFraction(stream, be);
  result.skew = readFraction(stream, be);

  result.linkId = readU32(stream, be);
  result.gradientId = readU32(stream, be);

  skip(stream, 6);

  const uint8_t contentType = readU8(stream);
  switch (contentType)
  {
  case 1:
    result.contentType = ContentType::OBJECTS;
    break;
  case 2:
  case 4:
    result.contentType = ContentType::NONE;
    break;
  case 3:
    result.contentType = ContentType::TEXT;
    break;
  case 5:
    result.contentType = ContentType::PICTURE;
    break;
  default:
    QXP_DEBUG_MSG(("Unknown content type %u\n", contentType));
    throw ParseError();
  }

  const uint8_t shapeType = readU8(stream);
  switch (shapeType)
  {
  case 0:
    result.shapeType = ShapeType::LINE;
    break;
  case 1:
    result.shapeType = ShapeType::ORTHOGONAL_LINE;
    break;
  case 2:
    result.shapeType = ShapeType::RECTANGLE;
    break;
  case 3:
    result.shapeType = ShapeType::CORNERED_RECTANGLE;
    break;
  case 4:
    result.shapeType = ShapeType::OVAL;
    break;
  case 5:
    result.shapeType = ShapeType::POLYGON;
    break;
  default:
    QXP_DEBUG_MSG(("Unknown shape type %u\n", shapeType));
    throw ParseError();
  }

  result.cornerRadius = readFraction(stream, be);

  if (result.gradientId != 0)
  {
    result.fill = readGradient(stream, color);
  }

  result.boundingBox = readObjectBBox(stream);

  return result;
}

void QXP33Parser::parseLine(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP33Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto line = make_shared<Line>();
  line->boundingBox = header.boundingBox;
  line->rotation = header.rotation;
  if (auto fill = header.fill.get_ptr())
  {
    if (auto color = boost::get<Color>(fill))
    {
      line->color = *color;
    }
    else
    {
      QXP_DEBUG_MSG(("Unsupported line fill type\n"));
    }
  }

  line->width = readFraction(stream, be);
  skip(stream, 2);

  collector.collectLine(line);
}

void QXP33Parser::parseTextBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP33Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto textbox = make_shared<TextBox>();
  textbox->boundingBox = header.boundingBox;
  textbox->fill = header.fill;
  textbox->cornerRadius = header.cornerRadius;
  textbox->settings.linkId = header.linkId;

  textbox->frame = readFrame(stream);
  skip(stream, 4);
  const unsigned runaroundId = readU32(stream, be);

  if (header.contentIndex == 0)
  {
    skip(stream, 82);
    collector.collectBox(textbox);
  }
  else
  {
    textbox->settings.offsetIntoText = readU32(stream, be);
    skip(stream, 4);
    textbox->settings.gutterWidth = readFraction(stream, be);
    textbox->settings.inset.top = readFraction(stream, be);
    textbox->settings.inset.left = readFraction(stream, be);
    textbox->settings.inset.right = readFraction(stream, be);
    textbox->settings.inset.bottom = readFraction(stream, be);
    textbox->settings.rotation = readFraction(stream, be);
    textbox->settings.skew = readFraction(stream, be);
    textbox->settings.columnsCount = readU8(stream);
    textbox->settings.verticalAlignment = readVertAlign(stream);
    skip(stream, 8);
    textbox->settings.nextLinkedIndex = readU16(stream, be);
    skip(stream, 10);

    if (textbox->settings.offsetIntoText > 0)
    {
      textbox->settings.linkedIndex = header.contentIndex;
    }
    else
    {
      textbox->text = parseText(header.contentIndex, header.linkId, collector);
      skip(stream, 12);
    }

    collector.collectTextBox(textbox);
  }

  if (runaroundId != 0)
  {
    const unsigned length = readU32(stream, be);
    skip(stream, length);
  }
}

void QXP33Parser::parsePictureBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP33Parser::ObjectHeader &header, QXPCollector &collector)
{

}

void QXP33Parser::parseEmptyBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP33Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto box = make_shared<Box>();
  box->boundingBox = header.boundingBox;
  box->fill = header.fill;
  box->cornerRadius = header.cornerRadius;

  box->frame = readFrame(stream);
  skip(stream, 4);
  const unsigned runaroundId = readU32(stream, be);

  skip(stream, 74);

  if (runaroundId != 0)
  {
    const unsigned length = readU32(stream, be);
    skip(stream, length);
  }

  collector.collectBox(box);
}

void QXP33Parser::parseGroup(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP33Parser::ObjectHeader &header, QXPCollector &collector, const Page &page)
{
  skip(stream, 10);
  const unsigned count = readU16(stream, be);
  if (count >= page.objectsCount - 1)
  {
    QXP_DEBUG_MSG(("Invalid group elements count %u\n", count));
    throw ParseError();
  }
  skip(stream, 6);

  auto group = make_shared<Group>();
  group->boundingBox = header.boundingBox;

  group->objectsIndexes.resize(count);
  for (unsigned &ind : group->objectsIndexes)
  {
    ind = readU32(stream, be);
    if (ind >= page.objectsCount)
    {
      QXP_DEBUG_MSG(("Invalid group element index %u\n", ind));
      throw ParseError();
    }
  }

  collector.collectGroup(group);
}

Frame QXP33Parser::readFrame(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  Frame frame;
  frame.width = readFraction(stream, be);
  const double shade = readFraction(stream, be);
  const unsigned colorId = readU8(stream);
  frame.color = getColor(colorId).applyShade(shade);
  skip(stream, 1);
  return frame;
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
