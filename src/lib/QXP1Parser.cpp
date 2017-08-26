/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXP1Parser.h"

#include "QXP1Header.h"
#include "QXPCollector.h"

namespace libqxp
{

using librevenge::RVNGInputStream;
using std::make_shared;
using std::shared_ptr;

namespace
{

double getShade(const unsigned shadeId)
{
  if (shadeId < 3)
    return 0.1 * shadeId;
  else if (shadeId < 6)
    return 0.2 * (shadeId - 1);
  else
    return 1.0;
}

}

QXP1Parser::QXP1Parser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter, const std::shared_ptr<QXP1Header> &header)
  : QXPParser(input, painter, header)
  , m_header(header)
{
}

void QXP1Parser::adjust(double &pos, unsigned adjustment)
{
  pos += double(adjustment - 0x8000) / 0x10000;
}

bool QXP1Parser::parseDocument(const std::shared_ptr<librevenge::RVNGInputStream> &docStream, QXPCollector &)
{
  parseCharFormats(docStream);
  parseParagraphFormats(docStream);
  return true;
}

bool QXP1Parser::parsePages(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXPCollector &collector)
{
  Page page;
  page.pageSettings.resize(1);
  page.pageSettings[0].offset.bottom = m_header->pageHeight();
  page.pageSettings[0].offset.right = m_header->pageWidth();

  for (unsigned i = 0; i != m_header->pages(); ++i)
  {
    collector.startPage(page);
    collector.endPage();
  }

  return false;
}

CharFormat QXP1Parser::parseCharFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  CharFormat result;

  skip(stream, 2);

  const int fontIndex = readS16(stream, true);
  (void) fontIndex;
  result.fontName = "Helvetica"; // TODO: How to determine font name? Is the list of fonts fixed?

  result.fontSize = readU16(stream, true) / 4.0;

  const unsigned flags = readU16(stream, true);
  convertCharFormatFlags(flags, result);

  skip(stream, 2);

  const unsigned colorId = readU8(stream);
  const unsigned shadeId = readU8(stream);
  result.color = getColor(colorId).applyShade(getShade(shadeId));

  return result;
}

ParagraphFormat QXP1Parser::parseParagraphFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  ParagraphFormat result;

  skip(stream, 3);
  result.alignment = readHorAlign(stream);
  skip(stream, 2);
  result.margin.left = readFraction(stream, true);
  result.firstLineIndent = readFraction(stream, true);
  result.margin.right = readFraction(stream, true);
  result.leading = readFraction(stream, true);
  result.margin.top = readFraction(stream, true);
  result.margin.bottom = readFraction(stream, true);

  for (unsigned i = 0; i < 20; ++i)
  {
    TabStop tabStop;

    const uint8_t type = readU8(stream);
    tabStop.type = convertTabStopType(type);
    tabStop.fillChar.append(char(readU8(stream)));
    tabStop.position = readFraction(stream, true);

    if (tabStop.isDefined())
      result.tabStops.push_back(tabStop);
  }

  return result;
}

std::shared_ptr<HJ> QXP1Parser::parseHJ(const std::shared_ptr<librevenge::RVNGInputStream> &)
{
  return std::shared_ptr<HJ>();
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
