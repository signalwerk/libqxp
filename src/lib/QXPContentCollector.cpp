/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXPContentCollector.h"
#include <unordered_set>

namespace libqxp
{

using librevenge::RVNG_POINT;
using librevenge::RVNGPropertyList;

QXPContentCollector::QXPContentCollector(librevenge::RVNGDrawingInterface *painter)
  : m_painter(painter)
  , m_isDocumentStarted(false)
  , m_isCollectingFacingPage(false)
  , m_unprocessedPages()
  , m_linkTextMap()
  , m_linkTextBoxMap()
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

  m_painter->startDocument(RVNGPropertyList());

  m_isDocumentStarted = true;
}

void QXPContentCollector::endDocument()
{
  if (!m_isDocumentStarted)
    return;

  if (!m_unprocessedPages.empty())
    endPage();

  m_painter->endDocument();

  m_isDocumentStarted = false;
}

void QXPContentCollector::startPage(const Page &page)
{
  m_unprocessedPages.push_back(CollectedPage(page.pageSettings[0]));
  if (page.isFacing())
  {
    m_unprocessedPages.push_back(CollectedPage(page.pageSettings[1]));
  }
  m_isCollectingFacingPage = page.isFacing();
}

void QXPContentCollector::endPage()
{
  if (m_unprocessedPages.empty())
    return;

  draw();
}

void QXPContentCollector::collectLine(const std::shared_ptr<Line> &line)
{

}

void QXPContentCollector::collectBox(const std::shared_ptr<Box> &box)
{

}

void QXPContentCollector::collectTextBox(const std::shared_ptr<TextBox> &box)
{
  if (0 == box->settings.linkId)
  {
    QXP_DEBUG_MSG(("Collected textbox with link ID 0"));
  }

  m_linkTextBoxMap[box->settings.linkId].push_back(box);
}

void QXPContentCollector::collectGroup(const std::shared_ptr<Group> &group)
{
  getInsertionPage(group).groups.push_back(group);
}

void QXPContentCollector::collectText(const std::shared_ptr<Text> &text, const unsigned linkId)
{
  m_linkTextMap[linkId] = text;
}

QXPContentCollector::CollectedPage &QXPContentCollector::getInsertionPage(const std::shared_ptr<Object> &obj)
{
  if (m_isCollectingFacingPage && obj->boundingBox.center().x > m_unprocessedPages.back().settings.offset.left)
  {
    return m_unprocessedPages[m_unprocessedPages.size() - 1];
  }
  return m_unprocessedPages.back();
}

void QXPContentCollector::draw()
{
  // TODO: check that all linked textboxes have text?

  for (auto &page : m_unprocessedPages)
  {
    RVNGPropertyList propList;
    propList.insert("svg:width", page.settings.offset.width(), RVNG_POINT);
    propList.insert("svg:height", page.settings.offset.height(), RVNG_POINT);
    m_painter->startPage(propList);

    std::unordered_set<unsigned> processedObjects;
    for (const auto &group : page.groups)
    {
      m_painter->openGroup(RVNGPropertyList());

      for (const unsigned &ind : group->objectsIndexes)
      {
        processedObjects.insert(ind);


      }

      m_painter->closeGroup();
    }


    m_painter->endPage();
  }

  m_unprocessedPages.clear();
}


}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
