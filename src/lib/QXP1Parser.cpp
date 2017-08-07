/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXP1Parser.h"

namespace libqxp
{

using librevenge::RVNGInputStream;
using std::make_shared;
using std::shared_ptr;

QXP1Parser::QXP1Parser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter,
                       const QXPHeader &commonHeader)
  : QXPParser(input, painter, commonHeader, m_header)
  , m_header()
{
}

bool QXP1Parser::parseDocument(const std::shared_ptr<librevenge::RVNGInputStream> &docStream)
{
  (void) docStream;
  return false;
}

bool QXP1Parser::parsePages(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXPCollector &collector)
{
  (void) stream;
  (void) collector;
  return false;
}

CharFormat QXP1Parser::parseCharFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  (void) stream;
  return CharFormat();
}

ParagraphFormat QXP1Parser::parseParagraphFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream)
{
  (void) stream;
  return ParagraphFormat();
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
