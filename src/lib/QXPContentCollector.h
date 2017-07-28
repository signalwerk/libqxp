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
#include <functional>
#include <type_traits>

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

  void collectLine(const std::shared_ptr<Line> &line) override;
  void collectBox(const std::shared_ptr<Box> &box) override;
  void collectTextBox(const std::shared_ptr<TextBox> &box) override;
  void collectGroup(const std::shared_ptr<Group> &group) override;

  void collectText(const std::shared_ptr<Text> &text, const unsigned linkId) override;

private:
  struct CollectedPage;

  template<typename T>
  using ObjectHandler = std::function<void(const std::shared_ptr<T> &obj, const CollectedPage &)>;

  template<typename T, typename TThis>
  using ObjectHandlerMember = std::function<void(TThis *, const std::shared_ptr<T> &obj, const CollectedPage &)>;

  class CollectedObjectInterface
  {
  public:
    virtual ~CollectedObjectInterface() = default;

    virtual void draw(const CollectedPage &page) const = 0;
  };

  template<typename T>
  class CollectedObject : public CollectedObjectInterface
  {
    static_assert(std::is_base_of<Object, T>::value, "T is not Object");
  public:
    const std::shared_ptr<T> object;

    CollectedObject(const std::shared_ptr<T> &obj, const ObjectHandler<T> &handler)
      : object(obj), m_handler(handler)
    { }

    void draw(const CollectedPage &page) const override
    {
      m_handler(object, page);
    }
  private:
    const ObjectHandler<T> m_handler;
  };

  struct CollectedPage
  {
    const PageSettings settings;
    std::vector<std::shared_ptr<Group>> groups;
    std::vector<std::unique_ptr<CollectedObjectInterface>> objects;

    CollectedPage(const PageSettings &pageSettings)
      : settings(pageSettings), groups(), objects()
    { }

    double getX(const double x) const;
    double getX(const std::shared_ptr<Object> &obj) const;
    double getY(const double y) const;
    double getY(const std::shared_ptr<Object> &obj) const;
    Point getPoint(const Point &p) const;
  };

  librevenge::RVNGDrawingInterface *m_painter;

  bool m_isDocumentStarted;
  bool m_isCollectingFacingPage;

  std::vector<CollectedPage> m_unprocessedPages;

  std::unordered_map<unsigned, std::shared_ptr<Text>> m_linkTextMap;
  std::unordered_map<unsigned, std::vector<std::shared_ptr<TextBox>>> m_linkTextBoxMap;

  CollectedPage &getInsertionPage(const std::shared_ptr<Object> &obj);

  template<typename T>
  void addObject(const std::shared_ptr<T> &obj, const ObjectHandlerMember<T, QXPContentCollector> &handler)
  {
    getInsertionPage(obj).objects.emplace_back(new CollectedObject<T>(obj, std::bind(handler, this, std::placeholders::_1, std::placeholders::_2)));
  }

  void draw();

  void drawLine(const std::shared_ptr<Line> &line, const CollectedPage &page);
  void drawBox(const std::shared_ptr<Box> &box, const CollectedPage &page);

  void writeFill(librevenge::RVNGPropertyList &propList, const boost::optional<Fill> &fill);
  void writeFrame(librevenge::RVNGPropertyList &propList, const Frame &frame);
};

}

#endif // QXPCONTENTCOLLECTOR_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
