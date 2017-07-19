/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXPContentCollector.h"

namespace libqxp
{

QXPContentCollector::QXPContentCollector(librevenge::RVNGDrawingInterface *painter)
  : m_painter(painter)
  , m_isDocumentStarted(false)
  , m_isPageStarted(false)
{
}

QXPContentCollector::~QXPContentCollector()
{
  if (m_isDocumentStarted)
  {
    endDocument();
  }
}

void QXPContentCollector::startDocument()
{
  if (m_isDocumentStarted)
    return;

  librevenge::RVNGPropertyList propList;

  m_painter->startDocument(propList);

  m_isDocumentStarted = true;
}

void QXPContentCollector::endDocument()
{
  if (!m_isDocumentStarted)
    return;

  if (m_isPageStarted)
    endPage();

  m_painter->endDocument();

  m_isDocumentStarted = false;
}

void QXPContentCollector::startPage(const Page &page)
{
  if (m_isPageStarted)
    return;

  librevenge::RVNGPropertyList propList;

  m_painter->startPage(propList);

  m_isPageStarted = true;
}

void QXPContentCollector::endPage()
{
  if (!m_isPageStarted)
    return;

  m_painter->endPage();

  m_isPageStarted = false;
}



}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
