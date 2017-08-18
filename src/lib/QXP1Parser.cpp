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

bool QXP1Parser::parseDocument(const std::shared_ptr<librevenge::RVNGInputStream> &docStream, QXPCollector &)
{
  parseCharFormats(docStream);
  return true;
}

bool QXP1Parser::parsePages(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXPCollector &collector)
{
  (void) stream;
  (void) collector;
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
  (void) stream;
  return ParagraphFormat();
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
