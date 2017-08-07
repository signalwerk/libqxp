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
using std::shared_ptr;

namespace
{

template<typename T>
shared_ptr<T> createBox(const QXP4Parser::ObjectHeader &header)
{
  auto box = make_shared<T>();
  box->cornerType = header.cornerType;
  box->boxType = header.boxType;
  box->rotation = header.rotation;
  box->fill = header.fillColor;
  return box;
}

template<typename T>
shared_ptr<T> createLine(const QXP4Parser::ObjectHeader &header)
{
  auto line = make_shared<T>();
  line->rotation = header.rotation;
  return line;
}

}

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

  parseParagraphFormats(docStream);

  skipRecord(docStream);

  return true;
}

bool QXP4Parser::parsePages(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXPCollector &collector)
{
  QXP4Deobfuscator deobfuscator(m_header.seed(), m_header.increment());
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

    auto page = parsePage(stream, deobfuscator);
    coll.startPage(page);
    deobfuscator.nextRev();

    for (unsigned i = 0; i < page.objectsCount; ++i)
    {
      parseObject(stream, deobfuscator, coll, page);
    }

    coll.endPage();

    ind++;
  }

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

  const unsigned colorId = readU16(stream, be);

  skip(stream, 2);

  const double shade = readFraction(stream, be);
  result.color = getColor(colorId).applyShade(shade);

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
  if (specLength > getLength(stream))
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
    if (count > getLength(stream) / 8)
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

ParagraphFormat QXP4Parser::parseParagraphFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  ParagraphFormat result;

  skip(stream, 11);
  result.alignment = readHorAlign(stream);

  skip(stream, 8);

  result.margin.left = readFraction(stream, be);
  result.firstLineIndent = readFraction(stream, be);
  result.margin.right = readFraction(stream, be);
  result.leading = readFraction(stream, be);
  result.margin.top = readFraction(stream, be);
  result.margin.bottom = readFraction(stream, be);

  skip(stream, 52);

  const unsigned tabStopsIndex = readU16(stream, be);
  if (tabStopsIndex != 0xffff)
  {
    if (tabStopsIndex >= m_paragraphTabStops.size())
    {
      QXP_DEBUG_MSG(("Tab stop %u not found\n", tabStopsIndex));
    }
    else
    {
      result.tabStops = m_paragraphTabStops[tabStopsIndex];
    }
  }

  skip(stream, 2);

  return result;
}

Page QXP4Parser::parsePage(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXP4Deobfuscator &deobfuscator)
{
  Page page;
  page.pageSettings = parsePageSettings(stream);
  page.objectsCount = deobfuscator(uint16_t(readU32(stream, be) & 0xffff));
  return page;
}

void QXP4Parser::parseObject(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXP4Deobfuscator &deobfuscator, QXPCollector &collector, const Page &page)
{
  const auto header = parseObjectHeader(stream, deobfuscator);

  switch (header.contentType)
  {
  case ContentType::NONE:
    switch (header.shapeType)
    {
    case ShapeType::LINE:
    case ShapeType::ORTHOGONAL_LINE:
      parseLine(stream, header, collector);
      break;
    case ShapeType::BEZIER_LINE:
      parseBezierLine(stream, header, collector);
      break;
    case ShapeType::BEZIER_BOX:
      parseBezierEmptyBox(stream, header, collector);
      break;
    case ShapeType::RECTANGLE:
    case ShapeType::ROUNDED_RECTANGLE:
    case ShapeType::CONCAVE_RECTANGLE:
    case ShapeType::BEVELED_RECTANGLE:
    case ShapeType::OVAL:
      parseEmptyBox(stream, header, collector);
      break;
    default:
      QXP_DEBUG_MSG(("Unsupported shape\n"));
      throw GenericException();
    }
    break;
  case ContentType::PICTURE:
    switch (header.shapeType)
    {
    case ShapeType::BEZIER_BOX:
      parseBezierPictureBox(stream, header, collector);
      break;
    case ShapeType::RECTANGLE:
    case ShapeType::ROUNDED_RECTANGLE:
    case ShapeType::CONCAVE_RECTANGLE:
    case ShapeType::BEVELED_RECTANGLE:
    case ShapeType::OVAL:
      parsePictureBox(stream, header, collector);
      break;
    default:
      QXP_DEBUG_MSG(("Unsupported shape\n"));
      throw GenericException();
    }
    break;
  case ContentType::TEXT:
    switch (header.shapeType)
    {
    case ShapeType::LINE:
    case ShapeType::ORTHOGONAL_LINE:
      parseLineText(stream, header, collector);
      break;
    case ShapeType::BEZIER_LINE:
      parseBezierText(stream, header, collector);
      break;
    case ShapeType::BEZIER_BOX:
      parseBezierTextBox(stream, header, collector);
      break;
    case ShapeType::RECTANGLE:
    case ShapeType::ROUNDED_RECTANGLE:
    case ShapeType::CONCAVE_RECTANGLE:
    case ShapeType::BEVELED_RECTANGLE:
    case ShapeType::OVAL:
      parseTextBox(stream, header, collector);
      break;
    default:
      QXP_DEBUG_MSG(("Unsupported shape\n"));
      throw GenericException();
    }
    break;
  case ContentType::OBJECTS:
    parseGroup(stream, header, collector, page);
    break;
  default:
    QXP_DEBUG_MSG(("Unsupported content\n"));
    throw GenericException();
  }

  deobfuscator.next(uint16_t(header.contentIndex));
}

QXP4Parser::ObjectHeader QXP4Parser::parseObjectHeader(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXP4Deobfuscator &deobfuscator)
{
  ObjectHeader result;

  skip(stream, 1);

  bool noColor;
  readObjectFlags(stream, noColor);

  const unsigned colorId = readU16(stream, be);
  const double shade = readFraction(stream, be);
  result.color = getColor(colorId).applyShade(shade);
  if (!noColor)
  {
    result.fillColor = result.color;
  }

  skip(stream, 4);

  const uint16_t contentIndexObf = uint16_t(readU32(stream, be) & 0xffff);

  result.rotation = readFraction(stream, be);
  result.skew = readFraction(stream, be);

  result.linkId = readU32(stream, be);
  result.oleId = readU32(stream, be);
  result.gradientId = readU32(stream, be);

  skip(stream, 4);

  const bool boxFlag1 = readU8(stream);
  const bool boxFlag2 = readU8(stream);
  if (be)
  {
    result.hflip = boxFlag1 & 0x80;
    result.vflip = boxFlag2 & 0x80;
  }
  else
  {
    result.hflip = boxFlag1 & 0x1;
    result.vflip = boxFlag2 & 0x1;
  }

  const uint8_t contentType = deobfuscator(readU8(stream));
  deobfuscator.nextShift(contentType);

  result.contentIndex = deobfuscator(contentIndexObf);

  const uint8_t shapeType = deobfuscator(readU8(stream));

  switch (contentType)
  {
  case 0:
    result.contentType = ContentType::NONE;
    break;
  case 2:
    result.contentType = ContentType::OBJECTS;
    break;
  case 3:
    result.contentType = ContentType::TEXT;
    break;
  case 4:
    result.contentType = ContentType::PICTURE;
    break;
  default:
    QXP_DEBUG_MSG(("Unknown content type %u\n", contentType));
    throw ParseError();
  }

  switch (shapeType)
  {
  case 1:
    result.shapeType = ShapeType::LINE;
    break;
  case 2:
    result.shapeType = ShapeType::ORTHOGONAL_LINE;
    break;
  case 4:
    result.shapeType = ShapeType::BEZIER_LINE;
    break;
  case 5:
    result.shapeType = ShapeType::RECTANGLE;
    result.boxType = BoxType::RECTANGLE;
    break;
  case 6:
    result.shapeType = ShapeType::ROUNDED_RECTANGLE;
    result.boxType = BoxType::RECTANGLE;
    result.cornerType = CornerType::ROUNDED;
    break;
  case 7:
    result.shapeType = ShapeType::CONCAVE_RECTANGLE;
    result.boxType = BoxType::RECTANGLE;
    result.cornerType = CornerType::CONCAVE;
    break;
  case 8:
    result.shapeType = ShapeType::BEVELED_RECTANGLE;
    result.boxType = BoxType::RECTANGLE;
    result.cornerType = CornerType::BEVELED;
    break;
  case 9:
    result.shapeType = ShapeType::OVAL;
    result.boxType = BoxType::OVAL;
    break;
  case 11:
    result.shapeType = ShapeType::BEZIER_BOX;
    result.boxType = BoxType::BEZIER;
    break;
  default:
    QXP_DEBUG_MSG(("Unknown shape type %u\n", shapeType));
    throw ParseError();
  }

  return result;
}

void QXP4Parser::parseLine(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP4Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto line = createLine<Line>(header);

  line->style = readFrame(stream);

  skip(stream, 48);

  line->boundingBox = readObjectBBox(stream);

  skip(stream, 24);

  collector.collectLine(line);
}

void QXP4Parser::parseBezierLine(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP4Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto line = createLine<Line>(header);

  line->style = readFrame(stream);

  skip(stream, 88);

  readBezierData(stream, line->curveComponents, line->boundingBox);

  collector.collectLine(line);
}

void QXP4Parser::parseBezierEmptyBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP4Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto box = createBox<Box>(header);

  box->frame = readFrame(stream);

  skip(stream, 88);

  if (header.gradientId != 0)
  {
    box->fill = readGradient(stream, header.color);
  }

  readBezierData(stream, box->curveComponents, box->boundingBox);

  collector.collectBox(box);
}

void QXP4Parser::parseEmptyBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP4Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto box = createBox<Box>(header);

  box->frame = readFrame(stream);

  skip(stream, 48);

  box->boundingBox = readObjectBBox(stream);

  box->cornerRadius = readFraction(stream, be);

  skip(stream, 20);

  if (header.gradientId != 0)
  {
    box->fill = readGradient(stream, header.color);
  }

  collector.collectBox(box);
}

void QXP4Parser::parseBezierPictureBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP4Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto box = createBox<PictureBox>(header);

  box->frame = readFrame(stream);

  skip(stream, 84);

  readOleObject(stream);

  if (header.gradientId != 0)
  {
    box->fill = readGradient(stream, header.color);
  }

  readPictureSettings(stream, box);

  skip(stream, 76);

  if (header.contentIndex != 0 && header.oleId == 0)
  {
    readImageData(stream);
  }

  readBezierData(stream, box->curveComponents, box->boundingBox);

  collector.collectBox(box);
}

void QXP4Parser::parsePictureBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP4Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto picturebox = createBox<PictureBox>(header);

  picturebox->frame = readFrame(stream);

  skip(stream, 48);

  picturebox->boundingBox = readObjectBBox(stream);

  picturebox->cornerRadius = readFraction(stream, be);

  skip(stream, 16);

  readOleObject(stream);

  if (header.gradientId != 0)
  {
    picturebox->fill = readGradient(stream, header.color);
  }

  readPictureSettings(stream, picturebox);

  skip(stream, 76);

  if (header.contentIndex != 0 && header.oleId == 0)
  {
    readImageData(stream);
  }

  collector.collectBox(picturebox);
}

void QXP4Parser::parseLineText(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP4Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto textpath = createLine<TextPath>(header);
  textpath->linkSettings.linkId = header.linkId;

  textpath->style = readFrame(stream);

  skip(stream, 48);

  textpath->boundingBox = readObjectBBox(stream);

  skip(stream, 24);

  textpath->linkSettings.offsetIntoText = readU32(stream, be);
  skip(stream, 44);
  readLinkedTextSettings(stream, textpath->linkSettings);
  skip(stream, 4);
  readTextPathSettings(stream, textpath->settings);
  skip(stream, 4);

  if (header.contentIndex == 0)
  {
    skip(stream, 28);
    collector.collectLine(textpath);
  }
  else
  {
    if (textpath->linkSettings.offsetIntoText > 0)
    {
      textpath->linkSettings.linkedIndex = header.contentIndex;
    }
    else
    {
      textpath->text = parseText(header.contentIndex, header.linkId, collector);
      skip(stream, 12);
    }

    collector.collectTextPath(textpath);
  }
}

void QXP4Parser::parseBezierText(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP4Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto textpath = createLine<TextPath>(header);
  textpath->linkSettings.linkId = header.linkId;

  textpath->style = readFrame(stream);

  skip(stream, 88);

  textpath->linkSettings.offsetIntoText = readU32(stream, be);
  skip(stream, 44);
  readLinkedTextSettings(stream, textpath->linkSettings);
  skip(stream, 4);
  readTextPathSettings(stream, textpath->settings);
  skip(stream, 4);

  readBezierData(stream, textpath->curveComponents, textpath->boundingBox);

  if (header.contentIndex == 0)
  {
    skip(stream, 28);
    collector.collectLine(textpath);
  }
  else
  {
    if (textpath->linkSettings.offsetIntoText > 0)
    {
      textpath->linkSettings.linkedIndex = header.contentIndex;
    }
    else
    {
      textpath->text = parseText(header.contentIndex, header.linkId, collector);
      skip(stream, 12);
    }

    collector.collectTextPath(textpath);
  }
}

void QXP4Parser::parseBezierTextBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP4Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto textbox = createBox<TextBox>(header);
  textbox->linkSettings.linkId = header.linkId;

  textbox->frame = readFrame(stream);

  skip(stream, 88);

  if (header.gradientId != 0)
  {
    textbox->fill = readGradient(stream, header.color);
  }

  textbox->linkSettings.offsetIntoText = readU32(stream, be);
  skip(stream, 2);
  readTextSettings(stream, textbox->settings);
  readLinkedTextSettings(stream, textbox->linkSettings);
  skip(stream, 12);

  readBezierData(stream, textbox->curveComponents, textbox->boundingBox);

  if (header.contentIndex == 0)
  {
    skip(stream, 28);
    collector.collectBox(textbox);
  }
  else
  {
    if (textbox->linkSettings.offsetIntoText > 0)
    {
      textbox->linkSettings.linkedIndex = header.contentIndex;
    }
    else
    {
      textbox->text = parseText(header.contentIndex, header.linkId, collector);
      skip(stream, 12);
    }

    collector.collectTextBox(textbox);
  }
}

void QXP4Parser::parseTextBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP4Parser::ObjectHeader &header, QXPCollector &collector)
{
  auto textbox = createBox<TextBox>(header);
  textbox->linkSettings.linkId = header.linkId;

  textbox->frame = readFrame(stream);

  skip(stream, 48);

  textbox->boundingBox = readObjectBBox(stream);

  textbox->cornerRadius = readFraction(stream, be);

  skip(stream, 20);

  if (header.gradientId != 0)
  {
    textbox->fill = readGradient(stream, header.color);
  }

  textbox->linkSettings.offsetIntoText = readU32(stream, be);
  skip(stream, 2);
  readTextSettings(stream, textbox->settings);
  readLinkedTextSettings(stream, textbox->linkSettings);
  skip(stream, 12);

  if (header.contentIndex == 0)
  {
    skip(stream, 28);
    collector.collectBox(textbox);
  }
  else
  {
    if (textbox->linkSettings.offsetIntoText > 0)
    {
      textbox->linkSettings.linkedIndex = header.contentIndex;
    }
    else
    {
      textbox->text = parseText(header.contentIndex, header.linkId, collector);
      skip(stream, 12);
    }

    collector.collectTextBox(textbox);
  }
}

void QXP4Parser::parseGroup(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const QXP4Parser::ObjectHeader &, QXPCollector &collector, const Page &page)
{
  auto group = make_shared<Group>();

  skip(stream, 68);

  group->boundingBox = readObjectBBox(stream);

  skip(stream, 24);

  const unsigned count = readU16(stream, be);
  if (count > page.objectsCount - 1)
  {
    QXP_DEBUG_MSG(("Invalid group elements count %u\n", count));
    throw ParseError();
  }
  skip(stream, 10);

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

Frame QXP4Parser::readFrame(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  Frame frame;
  frame.width = readFraction(stream, be);
  const double shade = readFraction(stream, be);
  const unsigned colorId = readU16(stream, be);
  frame.color = getColor(colorId).applyShade(shade);
  const unsigned gapColorId = readU16(stream, be);
  const double gapShade = readFraction(stream, be);
  frame.gapColor = getColor(gapColorId).applyShade(gapShade);
  skip(stream, 4);
  return frame;
}

void QXP4Parser::readLinkedTextSettings(const std::shared_ptr<librevenge::RVNGInputStream> &stream, LinkedTextSettings &settings)
{
  settings.nextLinkedIndex = readU32(stream, be);
  skip(stream, 4);
}

void QXP4Parser::readTextSettings(const std::shared_ptr<librevenge::RVNGInputStream> &stream, TextSettings &settings)
{
  skip(stream, 2);
  settings.gutterWidth = readFraction(stream, be);
  settings.inset.top = readFraction(stream, be);
  settings.inset.left = readFraction(stream, be);
  settings.inset.right = readFraction(stream, be);
  settings.inset.bottom = readFraction(stream, be);
  settings.rotation = readFraction(stream, be);
  settings.skew = readFraction(stream, be);
  settings.columnsCount = readU8(stream);
  settings.verticalAlignment = readVertAlign(stream);
  skip(stream, 10);
}

void QXP4Parser::readTextPathSettings(const std::shared_ptr<librevenge::RVNGInputStream> &stream, TextPathSettings &settings)
{
  settings.skew = readU8(stream) == 1;
  settings.rotate = readU8(stream) == 1;
  const uint8_t align = readU8(stream);
  switch (align)
  {
  default:
    QXP_DEBUG_MSG(("Unknown text path align %u\n", align));
  // fall-through intended: pick a default
  case 2:
    settings.alignment = TextPathAlignment::BASELINE;
    break;
  case 0:
    settings.alignment = TextPathAlignment::ASCENT;
    break;
  case 1:
    settings.alignment = TextPathAlignment::CENTER;
    break;
  case 3:
    settings.alignment = TextPathAlignment::DESCENT;
    break;
  }
  const uint8_t lineAlign = readU8(stream);
  switch (lineAlign)
  {
  default:
    QXP_DEBUG_MSG(("Unknown text path line align %u\n", lineAlign));
  // fall-through intended: pick a default
  case 0:
    settings.lineAlignment = TextPathLineAlignment::TOP;
    break;
  case 1:
    settings.lineAlignment = TextPathLineAlignment::CENTER;
    break;
  case 2:
    settings.lineAlignment = TextPathLineAlignment::BOTTOM;
    break;
  }
}

void QXP4Parser::readOleObject(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  const unsigned length = readU32(stream, be);
  skip(stream, length);
}

void QXP4Parser::readPictureSettings(const std::shared_ptr<librevenge::RVNGInputStream> &stream, std::shared_ptr<PictureBox> &picturebox)
{
  skip(stream, 24);
  picturebox->pictureRotation = readFraction(stream, be);
  picturebox->pictureSkew = readFraction(stream, be);
  picturebox->offsetLeft = readFraction(stream, be);
  picturebox->offsetTop = readFraction(stream, be);
  picturebox->scaleHor = readFraction(stream, be);
  picturebox->scaleVert = readFraction(stream, be);
}

void QXP4Parser::readImageData(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  const unsigned length = readU32(stream, be);
  skip(stream, length);
}

void QXP4Parser::readBezierData(const std::shared_ptr<librevenge::RVNGInputStream> &stream, std::vector<CurveComponent> &curveComponents, Rect &bbox)
{
  const unsigned length = readU32(stream, be);
  if (length > getLength(stream))
  {
    QXP_DEBUG_MSG(("Invalid bezier data length %ul\n", length));
    throw ParseError();
  }
  const unsigned long start = stream->tell();
  const unsigned long end = start + length;

  try
  {
    skip(stream, 2);
    const unsigned componentsCount = readU16(stream, be);
    if (componentsCount > length / 24)
    {
      QXP_DEBUG_MSG(("Invalid bezier components count %u\n", componentsCount));
      throw ParseError();
    }

    bbox = readObjectBBox(stream);

    std::vector<unsigned long> componentsOffsets;
    componentsOffsets.resize(componentsCount);
    for (auto &off : componentsOffsets)
    {
      off = start + readU32(stream, be);
    }

    curveComponents.resize(componentsCount);
    unsigned i = 0;
    for (auto &comp : curveComponents)
    {
      seek(stream, componentsOffsets[i++]);

      skip(stream, 2);
      const unsigned pointsCount = readU16(stream, be);
      if (pointsCount > length / 8)
      {
        QXP_DEBUG_MSG(("Invalid bezier points count %u\n", componentsCount));
        throw ParseError();
      }

      comp.boundingBox = readObjectBBox(stream);

      comp.points.resize(pointsCount);
      for (auto &p : comp.points)
      {
        p = readYX(stream);
      }
    }
  }
  catch (...)
  {
    QXP_DEBUG_MSG(("Failed to parse bezier data, offset %ld\n", stream->tell()));
  }

  seek(stream, end);
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
