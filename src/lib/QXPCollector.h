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
#include "QXPCollectorInterface.h"

namespace libqxp
{

class QXPCollector : public QXPCollectorInterface
{
  // disable copying
  QXPCollector(const QXPCollector &other) = delete;
  QXPCollector &operator=(const QXPCollector &other) = delete;

public:
  QXPCollector(librevenge::RVNGDrawingInterface *painter);
  ~QXPCollector();

  void startDocument() override;
  void endDocument() override;

  void startPage() override;
  void endPage() override;

private:
  librevenge::RVNGDrawingInterface *m_painter;

  bool m_isDocumentStarted;
  bool m_isPageStarted;
};

}

#endif // QXPCOLLECTOR_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
