/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef QXPCONTENTCOLLECTOR_H_INCLUDED
#define QXPCONTENTCOLLECTOR_H_INCLUDED

#include "libqxp_utils.h"
#include "QXPCollector.h"
#include <vector>
#include <unordered_map>
#include <memory>

namespace libqxp
{

class QXPContentCollector : public QXPCollector
{
  // disable copying
  QXPContentCollector(const QXPContentCollector &other) = delete;
  QXPContentCollector &operator=(const QXPContentCollector &other) = delete;

public:
  QXPContentCollector(librevenge::RVNGDrawingInterface *painter);
  ~QXPContentCollector();

  void startDocument() override;
  void endDocument() override;

  void startPage(const Page &page) override;
  void endPage() override;

  void collectLine(const Line &line) override;
  void collectBox(const Box &box) override;
  void collectTextBox(const TextBox &box) override;
  void collectGroup(const Group &group) override;

  void collectText(const std::shared_ptr<Text> &text, const unsigned linkId) override;

private:
  struct CollectedPage
  {
    const PageSettings settings;
    std::vector<Group> groups;

    CollectedPage(const PageSettings &pageSettings)
      : settings(pageSettings), groups()
    { }
  };

  librevenge::RVNGDrawingInterface *m_painter;

  bool m_isDocumentStarted;
  bool m_isCollectingFacingPage;

  std::vector<CollectedPage> m_unprocessedPages;

  std::unordered_map<unsigned, std::shared_ptr<Text>> m_linkTextMap;
  std::unordered_map<unsigned, std::vector<TextBox>> m_linkTextBoxMap;

  CollectedPage &getInsertionPage(const Object &obj);

  void draw();
};

}

#endif // QXPCONTENTCOLLECTOR_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
