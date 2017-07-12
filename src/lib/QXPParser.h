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
#include "QXPBlockParser.h"
#include "QXPHeader.h"
#include "QXPCollector.h"
#include "QXPTypes.h"
#include <map>

namespace libqxp
{

class QXPParser
{
  // disable copying
  QXPParser(const QXPParser &other) = delete;
  QXPParser &operator=(const QXPParser &other) = delete;

public:
  QXPParser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter,
            const QXPHeader &commonHeader, QXPHeader &header);
  virtual ~QXPParser() = default;

  bool parse();

protected:
  const std::shared_ptr<librevenge::RVNGInputStream> m_input;
  const bool be; // big endian

  QXPBlockParser m_blockParser;

  QXPCollector m_collector;

  std::map<uint32_t, Color> m_colors;
  std::map<int32_t, std::string> m_fonts;

  virtual bool parseDocument(const std::shared_ptr<librevenge::RVNGInputStream> &docStream) = 0;

  void skipRecord(const std::shared_ptr<librevenge::RVNGInputStream> &stream);
  void parseFonts(const std::shared_ptr<librevenge::RVNGInputStream> &stream);

  uint32_t readRecordEndOffset(const std::shared_ptr<librevenge::RVNGInputStream> &stream);

private:
  QXPHeader &m_header;
};

}

#endif // QXPPARSER_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
