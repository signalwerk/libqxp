/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef QXPCOLLECTOR_H_INCLUDED
#define QXPCOLLECTOR_H_INCLUDED

#include "libqxp_utils.h"
#include "QXPTypes.h"

namespace libqxp
{

class QXPCollector
{
  // disable copying
  QXPCollector(const QXPCollector &other) = delete;
  QXPCollector &operator=(const QXPCollector &other) = delete;

public:
  QXPCollector() = default;
  virtual ~QXPCollector() = default;

  virtual void startDocument() { }
  virtual void endDocument() { }

  virtual void startPage(const Page &) { }
  virtual void endPage() { }
};

class QXPDummyCollector : public QXPCollector
{
public:
  QXPDummyCollector() = default;
};

}

#endif // QXPCOLLECTOR_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
