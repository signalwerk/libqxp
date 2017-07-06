/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXP4Header.h"

namespace libqxp
{

QXP4Header::QXP4Header()
  : m_masterPagesCount(0)
  , m_seed(0)
  , m_increment()
{
}

bool QXP4Header::load(const std::shared_ptr<librevenge::RVNGInputStream> &input)
{
  QXPHeader::load(input);

  seek(input, 0x4d);
  m_masterPagesCount = readU8(input);

  seek(input, 0x52);
  m_increment = readU16(input, !isLittleEndian());

  seek(input, 0x80);
  m_seed = readU16(input, !isLittleEndian());

  seek(input, 512);

  return true;
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

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
