/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXP4Header.h"

#include "QXP4Parser.h"

namespace libqxp
{

QXP4Header::QXP4Header(const boost::optional<QXPDocument::Type> &fileType)
  : QXP3HeaderBase(fileType)
  , m_type()
  , m_masterPagesCount(0)
  , m_seed(0)
  , m_increment(0)
  , m_documentProperties()
{
}

bool QXP4Header::load(const std::shared_ptr<librevenge::RVNGInputStream> &input)
{
  QXP3HeaderBase::load(input);

  seek(input, 12);
  m_type = readString(input, 2);

  seek(input, 0x4d);
  m_masterPagesCount = readU8(input);

  seek(input, 0x52);
  m_increment = readU16(input, !isLittleEndian());

  seek(input, 0x80);
  m_seed = readU16(input, !isLittleEndian());

  seek(input, 512);

  return true;
}

QXPDocument::Type QXP4Header::getType() const
{
  if (m_fileType)
    return get(m_fileType);

  if (m_type == "BK")
    return QXPDocument::TYPE_BOOK;
  else if (m_type == "DC")
    return QXPDocument::TYPE_DOCUMENT;
  else if (m_type == "LB")
    return QXPDocument::TYPE_LIBRARY;
  else if (m_type == "TP")
    return QXPDocument::TYPE_TEMPLATE;
  else
    return QXPDocument::TYPE_UNKNOWN;
}

std::unique_ptr<QXPParser> QXP4Header::createParser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter)
{
  return std::unique_ptr<QXPParser>(new QXP4Parser(input, painter, shared_from_this()));
}

uint16_t QXP4Header::masterPagesCount() const
{
  return m_masterPagesCount;
}

uint16_t QXP4Header::seed() const
{
  return m_seed;
}

uint16_t QXP4Header::increment() const
{
  return m_increment;
}

const QXPDocumentProperties &QXP4Header::documentProperties() const
{
  return m_documentProperties;
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
