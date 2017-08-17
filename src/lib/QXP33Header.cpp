/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXP33Header.h"

#include "QXP33Parser.h"

namespace libqxp
{

QXP33Header::QXP33Header(const boost::optional<QXPDocument::Type> &fileType)
  : QXP3HeaderBase(fileType)
  , m_masterPagesCount(0)
  , m_seed(0)
  , m_increment()
{
}

bool QXP33Header::load(const std::shared_ptr<librevenge::RVNGInputStream> &input)
{
  QXP3HeaderBase::load(input);

  seek(input, 0x75);
  m_masterPagesCount = readU8(input);

  seek(input, 0x110);
  m_seed = readU16(input, !isLittleEndian());
  m_increment = readU16(input, !isLittleEndian());

  seek(input, 512);

  return true;
}

QXPDocument::Type QXP33Header::getType() const
{
  return m_fileType ? get(m_fileType) : QXPDocument::TYPE_DOCUMENT;
}

std::unique_ptr<QXPParser> QXP33Header::createParser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter)
{
  return std::unique_ptr<QXPParser>(new QXP33Parser(input, painter, shared_from_this()));
}

uint16_t QXP33Header::masterPagesCount() const
{
  return m_masterPagesCount;
}

uint16_t QXP33Header::seed() const
{
  return m_seed;
}

uint16_t QXP33Header::increment() const
{
  return m_increment;
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
