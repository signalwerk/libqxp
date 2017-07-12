/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXPHeader.h"
#include <string>

namespace libqxp
{

namespace
{
QXPVersion parseVersion(unsigned val)
{
  switch (val)
  {
  case 0x3e:
    return QXPVersion::QXP_31;
  case 0x3f:
    return QXPVersion::QXP_33;
  case 0x41:
    return QXPVersion::QXP_4;
  case 0x42:
    return QXPVersion::QXP_5;
  case 0x43:
    return QXPVersion::QXP_6;
  case 0x44:
    return QXPVersion::QXP_7;
  case 0x45:
    return QXPVersion::QXP_8;
  default:
    QXP_DEBUG_MSG(("Unknown version 0x%x\n", val));
    return QXPVersion::UNKNOWN;
  }
}

}

QXPHeader::QXPHeader()
  : m_proc(0)
  , m_signature()
  , m_version()
{
}

bool QXPHeader::load(const std::shared_ptr<librevenge::RVNGInputStream> &input)
{
  seek(input, 2);

  m_proc = readU8(input);
  skip(input, 1);
  m_signature = readString(input, 3);

  skip(input, 1);

  m_version = parseVersion(readU16(input, isBigEndian()));

  return true;
}

bool QXPHeader::isSupported() const
{
  return checkSignature() && (version() == QXP_33 || version() == QXP_4);
}

bool QXPHeader::isLittleEndian() const
{
  return m_proc == 'I';
}

bool QXPHeader::isBigEndian() const
{
  return !isLittleEndian();
}

QXPVersion QXPHeader::version() const
{
  return m_version;
}

bool QXPHeader::checkSignature() const
{
  return m_signature == "XPR";
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
