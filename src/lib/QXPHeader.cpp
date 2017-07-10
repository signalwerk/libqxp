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
  m_signature = std::string(reinterpret_cast<const char *>(readNBytes(input, 3)), 3);

  skip(input, 1);

  m_version = static_cast<QXPVersion>(readU8(input));

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
