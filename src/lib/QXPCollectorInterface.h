/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef QXPCOLLECTORINTERFACE_H_INCLUDED
#define QXPCOLLECTORINTERFACE_H_INCLUDED

#include "libqxp_utils.h"

namespace libqxp
{

class QXPCollectorInterface
{
  // disable copying
  QXPCollectorInterface(const QXPCollectorInterface &other) = delete;
  QXPCollectorInterface &operator=(const QXPCollectorInterface &other) = delete;

public:
  QXPCollectorInterface() = default;
  virtual ~QXPCollectorInterface() = default;

  virtual void startDocument() { }
  virtual void endDocument() { }

  virtual void startPage() { }
  virtual void endPage() { }
};

}

#endif // QXPCOLLECTORINTERFACE_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
