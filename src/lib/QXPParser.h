/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef QXPPARSER_H_INCLUDED
#define QXPPARSER_H_INCLUDED

#include "libqxp_utils.h"
#include "QXPHeader.h"
#include "QXPCollector.h"

namespace libqxp
{

class QXPParser
{
  // disable copying
  QXPParser(const QXPParser &other) = delete;
  QXPParser &operator=(const QXPParser &other) = delete;

public:
  QXPParser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter,
            const QXPHeader &commonHeader);
  virtual ~QXPParser() = default;

  virtual bool parse() = 0;

protected:
  const std::shared_ptr<librevenge::RVNGInputStream> m_input;
  QXPCollector m_collector;
  const bool be; // big endian
};

}

#endif // QXPPARSER_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
