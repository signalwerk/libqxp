/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef QXP33PARSER_H_INCLUDED
#define QXP33PARSER_H_INCLUDED

#include "libqxp_utils.h"
#include "QXPParser.h"
#include "QXP33Header.h"

#include <string>

namespace libqxp
{

class QXP33Parser : public QXPParser
{
public:
  QXP33Parser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter,
              const QXPHeader &commonHeader);

private:
  QXP33Header m_header;

  bool parseDocument(const std::shared_ptr<librevenge::RVNGInputStream> &docStream) override;
  bool parsePages(const std::shared_ptr<librevenge::RVNGInputStream> &pagesStream) override;

  void parseColors(const std::shared_ptr<librevenge::RVNGInputStream> &stream);
  CharFormat parseCharFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream) override;
  ParagraphFormat parseParagraphFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream) override;

  std::string readName(const std::shared_ptr<librevenge::RVNGInputStream> &stream);
};

}

#endif // QXP33PARSER_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
