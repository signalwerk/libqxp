/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef OPENLAYOUTCOLLECTOR_H_INCLUDED
#define OPENLAYOUTCOLLECTOR_H_INCLUDED

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "QXPCollector.h"
#include "QXPTypes.h"

#include "JsonValue.h"

namespace libqxp
{

enum class OpenLayoutUnit
{
  POINT,
  MILLIMETER
};

struct OpenLayoutOptions
{
  /// Unit of geometry (page sizes, positions, margins, indents). Type sizes and strokes stay in pt.
  OpenLayoutUnit unit;
  /// Reference masters from pages and drop page items that are unchanged copies of master items.
  bool linkMasters;
  std::string title;
  std::string generator;
  unsigned version;
  bool bigEndian;

  OpenLayoutOptions()
    : unit(OpenLayoutUnit::POINT), linkMasters(true), title(), generator(), version(0), bigEndian(false)
  { }
};

/** Collects the content of a QuarkXPress document and converts it to an OpenLayout document.
  *
  * Unlike QXPContentCollector, this collector keeps the structure of the document:
  * master pages, stylesheets, color list and text chains.
  */
class OpenLayoutCollector : public QXPCollector
{
public:
  enum class ItemKind
  {
    LINE,
    BOX,
    TEXT_BOX,
    TEXT_PATH,
    GROUP
  };

  struct Item
  {
    ItemKind kind;
    std::shared_ptr<Object> object;

    Item()
      : kind(ItemKind::BOX), object()
    { }

    Item(ItemKind k, const std::shared_ptr<Object> &obj)
      : kind(k), object(obj)
    { }
  };

  struct Spread
  {
    Page page;
    std::map<unsigned, Item> items;

    explicit Spread(const Page &p)
      : page(p), items()
    { }
  };

  explicit OpenLayoutCollector(const OpenLayoutOptions &options);

  bool collectsMasterPages() const override;

  void startPage(const Page &page) override;

  void collectDocumentProperties(const QXPDocumentProperties &props) override;
  void collectColor(const ColorSpec &color) override;
  void collectParagraphStyle(const ParagraphStyle &style) override;
  void collectCharacterStyle(const CharacterStyle &style) override;

  void collectLine(const std::shared_ptr<Line> &line) override;
  void collectBox(const std::shared_ptr<Box> &box) override;
  void collectTextBox(const std::shared_ptr<TextBox> &box) override;
  void collectTextPath(const std::shared_ptr<TextPath> &textPath) override;
  void collectGroup(const std::shared_ptr<Group> &group) override;

  void collectText(const std::shared_ptr<Text> &text, unsigned linkId) override;

  /// Builds the OpenLayout document from everything collected so far.
  openlayout::JsonValue createDocument() const;

private:
  void addItem(ItemKind kind, const std::shared_ptr<Object> &object);

  const OpenLayoutOptions m_options;
  QXPDocumentProperties m_docProps;
  std::vector<ColorSpec> m_colors;
  std::vector<ParagraphStyle> m_paragraphStyles;
  std::vector<CharacterStyle> m_characterStyles;
  std::vector<Spread> m_spreads;
  std::unordered_map<unsigned, std::shared_ptr<Text>> m_texts;
  unsigned m_currentObjectIndex;
};

}

#endif // OPENLAYOUTCOLLECTOR_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
