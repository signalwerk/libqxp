/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef QXPHEADER_H_INCLUDED
#define QXP4HEADER_H_INCLUDED

#include "libqxp_utils.h"
#include <string>

namespace libqxp
{

enum QXPVersion
{
  QXP_31 = 0x3e,
  QXP_33 = 0x3f,
  QXP_4 = 0x41,
  QXP_5 = 0x42,
  QXP_6 = 0x43,
  QXP_7 = 0x44,
  QXP_8 = 0x45,
};

class QXPHeader
{
public:
  QXPHeader();

  bool load(const std::shared_ptr<librevenge::RVNGInputStream> &input);

  bool isSupported() const;

  bool isLittleEndian() const;
  QXPVersion version() const;

private:
  bool checkSignature() const;

  uint8_t m_proc;
  std::string m_signature;
  QXPVersion m_version;
};

}

#endif // QXPHEADER_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
