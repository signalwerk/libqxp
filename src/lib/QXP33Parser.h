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
#include "QXPTypes.h"
#include "QXP33Deobfuscator.h"

#include <string>

namespace libqxp
{

class QXP33Parser : public QXPParser
{
public:
  QXP33Parser(const std::shared_ptr<librevenge::RVNGInputStream> &input, librevenge::RVNGDrawingInterface *painter,
              const QXPHeader &commonHeader);

private:
  enum class ShapeType
  {
    UNKNOWN,
    LINE,
    ORTHOGONAL_LINE,
    RECTANGLE,
    CORNERED_RECTANGLE,
    OVAL,
    POLYGON
  };

  struct ObjectHeader
  {
    boost::optional<Fill> fill;

    unsigned contentIndex;
    unsigned linkId;
    unsigned gradientId;

    double rotation;
    double skew;

    bool hflip;
    bool vflip;
    bool beveled;
    bool concave;
    double cornerRadius;

    ContentType contentType;
    ShapeType shapeType;

    Rect boundingBox;

    BoxType boxType;

    ObjectHeader()
      : fill(), contentIndex(0), linkId(0), gradientId(0),
        rotation(0), skew(0), hflip(false), vflip(false), beveled(false), concave(false),
        cornerRadius(0), contentType(ContentType::UNKNOWN), shapeType(ShapeType::UNKNOWN), boundingBox(),
        boxType(BoxType::UNKNOWN)
    { }
  };

  QXP33Header m_header;

  bool parseDocument(const std::shared_ptr<librevenge::RVNGInputStream> &docStream) override;
  bool parsePages(const std::shared_ptr<librevenge::RVNGInputStream> &pagesStream, QXPCollector &collector) override;

  void parseColors(const std::shared_ptr<librevenge::RVNGInputStream> &stream);
  CharFormat parseCharFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream) override;
  ParagraphFormat parseParagraphFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream) override;

  Page parsePage(const std::shared_ptr<librevenge::RVNGInputStream> &stream);

  void parseObject(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXP33Deobfuscator &deobfuscator, QXPCollector &collector, const Page &page);
  ObjectHeader parseObjectHeader(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXP33Deobfuscator &deobfuscator);
  void parseLine(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parseTextBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parsePictureBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parseEmptyBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parseGroup(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector, const Page &page);

  Frame readFrame(const std::shared_ptr<librevenge::RVNGInputStream> &stream);
  std::vector<Point> readPolygonData(const std::shared_ptr<librevenge::RVNGInputStream> &stream);

  std::string readName(const std::shared_ptr<librevenge::RVNGInputStream> &stream);
};

}

#endif // QXP33PARSER_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
