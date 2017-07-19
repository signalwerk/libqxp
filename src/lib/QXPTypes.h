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
#include <vector>

namespace libqxp
{

struct Rect
{
  double top;
  double right;
  double bottom;
  double left;

  Rect()
    : top(0.0), right(0.0), bottom(0.0), left(0.0)
  { }

  Rect(double t, double r, double b, double l)
    : top(t), right(r), bottom(b), left(l)
  { }

  double width() const
  {
    return right - left;
  }

  double height() const
  {
    return bottom - top;
  }
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
};

struct CharFormat
{
  librevenge::RVNGString fontName;
  double fontSize;
  Color color;
  double shade;
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
    : fontName("Arial"), fontSize(12.0), color(0, 0, 0), shade(1.0),
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
