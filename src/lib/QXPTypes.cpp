/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXPTypes.h"

namespace libqxp
{

librevenge::RVNGString Color::toString() const
{
  librevenge::RVNGString colorStr;
  colorStr.sprintf("#%.2x%.2x%.2x", red, green, blue);
  return colorStr;
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
