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

struct PageSettings
{
  Rect offset;
  Rect margin;

  PageSettings()
    : offset(), margin()
  { }

  PageSettings(Rect off, Rect marg)
    : offset(off), margin(marg)
  { }
};

}

#endif // QXPTYPES_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
