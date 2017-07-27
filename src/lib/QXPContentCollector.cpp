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
#include <boost/variant.hpp>

namespace libqxp
{

using librevenge::RVNG_POINT;
using librevenge::RVNGPropertyList;
using librevenge::RVNGPropertyListVector;
using std::vector;

namespace
{

RVNGPropertyListVector createPath(const vector<Point> &points, bool closed)
{
  RVNGPropertyListVector path;

  for (size_t i = 0; i < points.size(); i++)
  {
    librevenge::RVNGPropertyList pathPart;
    pathPart.insert("librevenge:path-action", i == 0 ? "M" : "L");
    pathPart.insert("svg:x", points[i].x, RVNG_POINT);
    pathPart.insert("svg:y", points[i].y, RVNG_POINT);

    path.append(pathPart);
  }

  if (closed)
  {
    librevenge::RVNGPropertyList pathPart;
    pathPart.insert("librevenge:path-action", "Z");

    path.append(pathPart);
  }

  return path;
}

class FillWriter : public boost::static_visitor<void>
{
public:
  explicit FillWriter(RVNGPropertyList &propList)
    : m_propList(propList)
  { }

  void operator()(const Color &color)
  {
    m_propList.insert("draw:fill", "solid");
    m_propList.insert("draw:fill-color", color.toString());
  }

  void operator()(const Gradient &gradient)
  {
    m_propList.insert("draw:fill", "gradient");
    m_propList.insert("draw:start-color", gradient.color1.toString());
    m_propList.insert("draw:end-color", gradient.color2.toString());

    switch (gradient.type)
    {
    default:
    case GradientType::LINEAR:
      m_propList.insert("draw:style", "linear");
      m_propList.insert("draw:angle", int(normalizeDegAngle(gradient.angle + 90)));
      break;
    case GradientType::CIRCULAR:
    case GradientType::FULLCIRCULAR:
      m_propList.insert("draw:style", "radial");
      m_propList.insert("draw:cx", 0.5, librevenge::RVNG_PERCENT);
      m_propList.insert("draw:cy", 0.5, librevenge::RVNG_PERCENT);
      m_propList.insert("draw:border", GradientType::CIRCULAR == gradient.type ? 0.35 : 0.10, librevenge::RVNG_PERCENT);
      m_propList.insert("draw:angle", int(normalizeDegAngle(gradient.angle)));
      break;
    case GradientType::RECTANGULAR:
    case GradientType::DIAMOND:
      m_propList.insert("draw:style", "square");
      m_propList.insert("draw:cx", 0.5, librevenge::RVNG_PERCENT);
      m_propList.insert("draw:cy", 0.5, librevenge::RVNG_PERCENT);
      m_propList.insert("draw:border", 0.35, librevenge::RVNG_PERCENT);
      m_propList.insert("draw:angle", int(normalizeDegAngle(gradient.angle)));
      break;
    }
  }

private:
  librevenge::RVNGPropertyList &m_propList;
};

}

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
  addObject<Box>(box, &QXPContentCollector::drawBox);
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

    unsigned i = 0;
    for (const auto &obj : page.objects)
    {
      if (processedObjects.find(i++) == processedObjects.end())
      {
        obj->draw(page);
      }
    }

    m_painter->endPage();
  }

  m_unprocessedPages.clear();
}

void QXPContentCollector::drawBox(const std::shared_ptr<Box> &box, const QXPContentCollector::CollectedPage &page)
{
  vector<Point> points =
  {
    page.getPoint(box->boundingBox.topLeft()),
    page.getPoint(box->boundingBox.topRight()),
    page.getPoint(box->boundingBox.bottomRight()),
    page.getPoint(box->boundingBox.bottomLeft())
  };

  if (!QXP_ALMOST_ZERO(box->rotation))
  {
    for (auto &p : points)
    {
      p = p.rotateDeg(-box->rotation, page.getPoint(box->boundingBox.center()));
    }
  }

  auto path = createPath(points, true);

  RVNGPropertyList propList;

  writeFrame(propList, box->frame);
  writeFill(propList, box->fill);

  m_painter->setStyle(propList);
  propList.clear();

  propList.insert("svg:d", path);

  m_painter->drawPath(propList);
}

void QXPContentCollector::writeFill(librevenge::RVNGPropertyList &propList, const boost::optional<Fill> &fill)
{
  propList.insert("draw:fill", "none");

  if (fill)
  {
    FillWriter fillWriter(propList);
    boost::apply_visitor(fillWriter, fill.get());
  }
}

void QXPContentCollector::writeFrame(librevenge::RVNGPropertyList &propList, const Frame &frame)
{
  propList.insert("draw:stroke", "none");

  if (frame.color)
  {
    propList.insert("draw:stroke", "solid");
    propList.insert("svg:stroke-color", frame.color->toString());
    propList.insert("svg:stroke-width", frame.width, RVNG_POINT);
  }
}

double QXPContentCollector::CollectedPage::getX(const double x) const
{
  return x - settings.offset.left;
}

double QXPContentCollector::CollectedPage::getX(const std::shared_ptr<Object> &obj) const
{
  return getX(obj->boundingBox.left);
}

double QXPContentCollector::CollectedPage::getY(const double y) const
{
  return y - settings.offset.top;
}

double QXPContentCollector::CollectedPage::getY(const std::shared_ptr<Object> &obj) const
{
  return getY(obj->boundingBox.top);
}

Point QXPContentCollector::CollectedPage::getPoint(const Point &p) const
{
  return Point(getX(p.x), getY(p.y));
}


}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
