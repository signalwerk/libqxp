/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef QXPTYPES_H_INCLUDED
#define QXPTYPES_H_INCLUDED

#include "libqxp_utils.h"
#include <boost/optional.hpp>
#include <boost/variant.hpp>
#include <vector>

namespace libqxp
{

struct Point
{
  double x;
  double y;

  Point()
    : x(0.0), y(0.0)
  { }

  Point(double xVal, double yVal)
    : x(xVal), y(yVal)
  { }

  Point move(double dx, double dy) const;
  Point rotateDeg(double rotationDeg, const Point &center) const;

  double distance(const Point &p2) const;
};

bool operator==(const Point &lhs, const Point &rhs);
bool operator!=(const Point &lhs, const Point &rhs);

struct Rect
{
  double top;
  double right;
  double bottom;
  double left;

  Rect();
  Rect(double t, double r, double b, double l);

  double width() const;
  double height() const;

  Point center() const;
  Point topLeft() const;
  Point topRight() const;
  Point bottomRight() const;
  Point bottomLeft() const;

  Rect shrink(const double diff) const;
};

struct Color
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;

  Color()
    : red(0), green(0), blue(0)
  {  }

  Color(uint8_t r, uint8_t g, uint8_t b)
    : red(r), green(g), blue(b)
  {  }

  librevenge::RVNGString toString() const;

  Color applyShade(double shade) const;
};

enum class GradientType
{
  LINEAR,
  MIDLINEAR,
  RECTANGULAR,
  DIAMOND,
  CIRCULAR,
  FULLCIRCULAR
};

struct Gradient
{
  GradientType type;
  Color color1;
  Color color2;
  double angle;

  Gradient()
    : type(), color1(), color2(), angle(0.0)
  { }
};

typedef boost::variant<Color, Gradient> Fill;

struct CharFormat
{
  librevenge::RVNGString fontName;
  double fontSize;
  Color color;
  bool bold;
  bool italic;
  bool underline;
  bool outline;
  bool shadow;
  bool superscript;
  bool subscript;
  bool superior;
  bool strike;
  bool allCaps;
  bool smallCaps;
  bool wordUnderline;

  CharFormat()
    : fontName("Arial"), fontSize(12.0), color(0, 0, 0),
      bold(false), italic(false), underline(false), outline(false), shadow(false), superscript(false), subscript(false), superior(false),
      strike(false), allCaps(false), smallCaps(false), wordUnderline(false)
  {  }
};

enum class HorizontalAlignment
{
  LEFT,
  CENTER,
  RIGHT,
  JUSTIFIED,
  FORCED
};

enum class VerticalAlignment
{
  TOP,
  CENTER,
  BOTTOM,
  JUSTIFIED
};

enum class TabStopType
{
  LEFT,
  CENTER,
  RIGHT,
  ALIGN
};

struct TabStop
{
  TabStopType type;
  double position;
  char fillChar;
  char alignChar;

  TabStop()
    : type(TabStopType::LEFT), position(0.0), fillChar(' '), alignChar(',')
  {  }
};

struct ParagraphRule
{
  double width;
  Color color;
  double shade;
  double leftMargin;
  double rightMargin;
  double offset;

  ParagraphRule()
    : width(1.0), color(0, 0, 0), shade(1.0), leftMargin(0), rightMargin(0), offset(0)
  {  }
};

struct ParagraphFormat
{
  HorizontalAlignment alignment;
  Rect margin;
  double firstLineIndent;
  double leading;
  boost::optional<ParagraphRule> ruleAbove;
  boost::optional<ParagraphRule> ruleBelow;
  std::vector<TabStop> tabStops;

  ParagraphFormat()
    : alignment(HorizontalAlignment::LEFT), margin(), firstLineIndent(0), leading(0),
      ruleAbove(), ruleBelow(), tabStops()
  {  }
};

enum class ContentType
{
  UNKNOWN,
  OBJECTS, // group
  NONE,
  TEXT,
  PICTURE
};

struct TextSpec
{
  const unsigned startIndex;
  const unsigned length;

  unsigned endIndex() const
  {
    return startIndex + length - 1;
  }

  unsigned afterEndIndex() const
  {
    return startIndex + length;
  }

protected:
  TextSpec(unsigned start, unsigned len)
    : startIndex(start), length(len)
  { }
};

struct CharFormatSpec : public TextSpec
{
  std::shared_ptr<CharFormat> format;

  CharFormatSpec(const std::shared_ptr<CharFormat> &f, unsigned start, unsigned len)
    : TextSpec(start, len), format(f)
  { }
};

struct ParagraphSpec : public TextSpec
{
  std::shared_ptr<ParagraphFormat> format;

  ParagraphSpec(const std::shared_ptr<ParagraphFormat> &f, unsigned start, unsigned len)
    : TextSpec(start, len), format(f)
  { }
};

struct Text
{
  std::string text;
  std::vector<ParagraphSpec> paragraphs;
  std::vector<CharFormatSpec> charFormats;

  Text()
    : text(), paragraphs(), charFormats()
  { }
};

enum class LineCapType
{
  BUTT,
  ROUND,
  RECT,
  STRETCH
};

enum class LineJoinType
{
  MITER,
  ROUND,
  BEVEL
};

struct LineStyle
{
  std::vector<double> segmentLengths;
  bool isStripe;
  LineCapType endcapType;
  LineJoinType joinType;

  LineStyle()
    : segmentLengths(), isStripe(false), endcapType(LineCapType::BUTT), joinType(LineJoinType::MITER)
  { }
};

struct Arrow
{
  const std::string path;
  const std::string viewbox;
  const double scale;

  explicit Arrow(const std::string &d, const std::string &vbox = "0 0 10 10", double s = 3)
    : path(d), viewbox(vbox), scale(s)
  { }
};

struct Frame
{
  double width;
  boost::optional<Color> color;
  boost::optional<Color> gapColor;
  LineStyle lineStyle;
  Arrow *startArrow;
  Arrow *endArrow;

  Frame()
    : width(1.0), color(), gapColor(), lineStyle(), startArrow(nullptr), endArrow(nullptr)
  { }

  Frame(const Frame &other) = default;
  Frame &operator=(const Frame &other) = default;
};

struct LinkedTextSettings
{
  unsigned linkId;
  unsigned offsetIntoText;
  unsigned linkedIndex;
  unsigned nextLinkedIndex;
  boost::optional<unsigned> textLength;

  LinkedTextSettings()
    : linkId(0), offsetIntoText(0), linkedIndex(0), nextLinkedIndex(0), textLength()
  { }
};

struct TextObject
{
  LinkedTextSettings linkSettings;
  boost::optional<std::shared_ptr<Text>> text;

  TextObject()
    : linkSettings(), text()
  { }

  bool isLinked() const;
};

struct TextSettings
{
  unsigned columnsCount;
  double gutterWidth;
  VerticalAlignment verticalAlignment;
  Rect inset;
  double rotation;
  double skew;

  TextSettings()
    : columnsCount(1), gutterWidth(12.0), verticalAlignment(VerticalAlignment::TOP), inset(), rotation(0), skew(0)
  { }
};

enum class TextPathAlignment
{
  ASCENT,
  CENTER,
  BASELINE,
  DESCENT
};

enum class TextPathLineAlignment
{
  TOP,
  CENTER,
  BOTTOM
};

struct TextPathSettings
{
  bool rotate;
  bool skew;
  TextPathAlignment alignment;
  TextPathLineAlignment lineAlignment;

  TextPathSettings()
    : rotate(false), skew(false), alignment(TextPathAlignment::BASELINE), lineAlignment(TextPathLineAlignment::TOP)
  { }
};

struct CurveComponent
{
  Rect boundingBox;
  std::vector<Point> points;

  CurveComponent()
    : boundingBox(), points()
  { }
};

struct Object
{
  Rect boundingBox;
  unsigned zIndex;

protected:
  Object()
    : boundingBox(), zIndex(0)
  { }
};

struct Line : Object
{
  double rotation;
  Frame style;
  std::vector<CurveComponent> curveComponents;

  Line()
    : rotation(0), style(), curveComponents()
  { }
};

struct TextPath : Line, TextObject
{
  TextPathSettings settings;

  TextPath()
    : settings()
  { }
};

enum class CornerType
{
  DEFAULT,
  ROUNDED,
  BEVELED,
  CONCAVE
};

enum class BoxType
{
  UNKNOWN,
  RECTANGLE,
  OVAL,
  POLYGON,
  BEZIER
};

struct Box : Object
{
  boost::optional<Fill> fill;
  Frame frame;
  BoxType boxType;
  CornerType cornerType;
  double cornerRadius;
  double rotation;
  std::vector<Point> customPoints;
  std::vector<CurveComponent> curveComponents;

  Box()
    : fill(), frame(), boxType(BoxType::UNKNOWN), cornerType(CornerType::DEFAULT), cornerRadius(0.0), rotation(0),
      customPoints(), curveComponents()
  { }
};

struct TextBox : Box, TextObject
{
  TextSettings settings;

  TextBox()
    : settings()
  { }
};

struct PictureBox : Box
{
  double pictureRotation;
  double pictureSkew;
  double offsetLeft;
  double offsetTop;
  double scaleHor;
  double scaleVert;

  PictureBox()
    : pictureRotation(0.0), pictureSkew(0.0),
      offsetLeft(0.0), offsetTop(0.0), scaleHor(0.0), scaleVert(0.0)
  { }
};

struct Group : Object
{
  std::vector<unsigned> objectsIndexes;

  Group()
    : objectsIndexes()
  { }
};

struct PageSettings
{
  Rect offset;
  Rect margin;

  PageSettings()
    : offset(), margin()
  { }
};

struct Page
{
  std::vector<PageSettings> pageSettings;
  unsigned objectsCount;

  Page()
    : pageSettings(), objectsCount(0)
  { }

  bool isFacing() const
  {
    return pageSettings.size() == 2;
  }
};

}

#endif // QXPTYPES_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
