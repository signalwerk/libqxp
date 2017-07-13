/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef QXP4PARSER_H_INCLUDED
#define QXP4PARSER_H_INCLUDED

#include "libqxp_utils.h"
#include "QXPParser.h"
#include "QXP4Header.h"
#include <map>

namespace libqxp
{

class QXP4Parser : public QXPParser
{
public:
  QXP4Parser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter,
             const QXPHeader &commonHeader);

private:
  struct ColorBlockSpec
  {
    unsigned offset;
    unsigned padding;
    unsigned nextOffset;

    ColorBlockSpec()
      : offset(0), padding(0), nextOffset(0)
    { }

    unsigned length() const
    {
      return nextOffset - offset - padding;
    }
  };

  QXP4Header m_header;

  bool parseDocument(const std::shared_ptr<librevenge::RVNGInputStream> &docStream) override;

  void parseColors(const std::shared_ptr<librevenge::RVNGInputStream> &docStream);
  ColorBlockSpec parseColorBlockSpec(const std::shared_ptr<librevenge::RVNGInputStream> &stream);
  unsigned parseColorsHeader(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ColorBlockSpec &block);
  unsigned parseColor(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ColorBlockSpec &block,
                      const std::map<unsigned, ColorBlockSpec> &blocks);
};

}

#endif // QXP4PARSER_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
