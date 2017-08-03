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
#include "QXP4Deobfuscator.h"
#include <vector>

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

    ColorBlockSpec()
      : offset(0), padding(0)
    { }
  };

  enum class ShapeType
  {
    UNKNOWN,
    LINE,
    ORTHOGONAL_LINE,
    BEZIER_LINE,
    RECTANGLE,
    ROUNDED_RECTANGLE,
    CONCAVE_RECTANGLE,
    BEVELED_RECTANGLE,
    OVAL,
    BEZIER_BOX
  };

  struct ObjectHeader
  {
    boost::optional<Color> fillColor;
    Color color;

    unsigned contentIndex;
    unsigned linkId;
    unsigned oleId;
    unsigned gradientId;

    double rotation;
    double skew;

    bool hflip;
    bool vflip;

    ContentType contentType;
    ShapeType shapeType;

    BoxType boxType;
    CornerType cornerType;

    ObjectHeader()
      : fillColor(), contentIndex(0), linkId(0), oleId(0), gradientId(0),
        rotation(0), skew(0), hflip(false), vflip(false),
        contentType(ContentType::UNKNOWN), shapeType(ShapeType::UNKNOWN),
        boxType(BoxType::UNKNOWN), cornerType(CornerType::DEFAULT)
    { }
  };

  QXP4Header m_header;

  std::vector<std::vector<TabStop>> m_paragraphTabStops;

  bool parseDocument(const std::shared_ptr<librevenge::RVNGInputStream> &docStream) override;
  bool parsePages(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXPCollector &collector) override;

  void parseColors(const std::shared_ptr<librevenge::RVNGInputStream> &docStream);
  ColorBlockSpec parseColorBlockSpec(const std::shared_ptr<librevenge::RVNGInputStream> &stream);
  void parseColor(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const std::vector<ColorBlockSpec> &blocks);
  void skipParagraphStylesheets(const std::shared_ptr<librevenge::RVNGInputStream> &stream);
  CharFormat parseCharFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream) override;
  void skipTemplates(const std::shared_ptr<librevenge::RVNGInputStream> &stream);
  void parseTabStops(const std::shared_ptr<librevenge::RVNGInputStream> &stream);
  ParagraphFormat parseParagraphFormat(const std::shared_ptr<librevenge::RVNGInputStream> &stream) override;

  Page parsePage(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXP4Deobfuscator &deobfuscator);

  void parseObject(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXP4Deobfuscator &deobfuscator, QXPCollector &collector, const Page &page);
  ObjectHeader parseObjectHeader(const std::shared_ptr<librevenge::RVNGInputStream> &stream, QXP4Deobfuscator &deobfuscator);
  void parseLine(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parseBezierLine(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parseBezierEmptyBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parseEmptyBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parseBezierPictureBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parsePictureBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parseLineText(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parseBezierText(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parseBezierTextBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parseTextBox(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector);
  void parseGroup(const std::shared_ptr<librevenge::RVNGInputStream> &stream, const ObjectHeader &header, QXPCollector &collector, const Page &page);

  Frame readFrame(const std::shared_ptr<librevenge::RVNGInputStream> &stream);
};

}

#endif // QXP4PARSER_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
