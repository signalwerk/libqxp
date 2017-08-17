/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXP1Header.h"

#include "QXP1Parser.h"

namespace libqxp
{

QXP1Header::QXP1Header()
{
}

bool QXP1Header::load(const std::shared_ptr<librevenge::RVNGInputStream> &input)
{
  m_proc = 'M';
  m_version = readU16(input, true);
  return true;
}

QXPDocument::Type QXP1Header::getType() const
{
  return QXPDocument::TYPE_DOCUMENT;
}

std::unique_ptr<QXPParser> QXP1Header::createParser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter)
{
  return std::unique_ptr<QXPParser>(new QXP1Parser(input, painter, shared_from_this()));
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
