/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXP33Parser.h"

namespace libqxp
{

QXP33Parser::QXP33Parser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter,
                         const QXPHeader &commonHeader)
  : QXPParser(input, painter, commonHeader)
  , m_header()
{
}

bool QXP33Parser::parse()
{
  if (!m_header.load(m_input) || !m_header.isSupported())
    return false;

  return true;
}


}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
