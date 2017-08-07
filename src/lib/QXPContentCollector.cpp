/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXPContentCollector.h"
#include <algorithm>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/variant.hpp>

namespace libqxp
{

using librevenge::RVNG_POINT;
using librevenge::RVNGPropertyList;
using librevenge::RVNGPropertyListVector;
using librevenge::RVNGString;
using std::vector;
using std::string;

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

void writeZIndex(librevenge::RVNGPropertyList &propList, unsigned value)
{
  propList.insert("draw:z-index", static_cast<int>(value));
}

void flushText(librevenge::RVNGDrawingInterface *painter, string &text)
{
  if (!text.empty())
  {
    painter->insertText(RVNGString(text.c_str()));
    text.clear();
  }
}

void insertText(librevenge::RVNGDrawingInterface *painter, const RVNGString &text)
{
  // TODO: need to remember wasSpace on span change?
  bool wasSpace = false;
  std::string curText;

  RVNGString::Iter iter(text);
  iter.rewind();
  while (iter.next())
  {
    const char *const utf8Char = iter();
    switch (utf8Char[0])
    {
    case '\r':
      wasSpace = false;
      break;
    case '\n':
      wasSpace = false;
      flushText(painter, curText);
      painter->insertLineBreak();
      break;
    case '\t':
      wasSpace = false;
      flushText(painter, curText);
      painter->insertTab();
      break;
    case ' ':
      if (wasSpace)
      {
        flushText(painter, curText);
        painter->insertSpace();
      }
      else
      {
        wasSpace = true;
        curText.push_back(' ');
      }
      break;
    default:
      wasSpace = false;
      curText.append(utf8Char);
      break;
    }
  }

  flushText(painter, curText);
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
      m_propList.insert("draw:border", GradientType::CIRCULAR == gradient.type ? 0.25 : 0.0, librevenge::RVNG_PERCENT);
      m_propList.insert("draw:angle", int(normalizeDegAngle(gradient.angle)));
      break;
    case GradientType::RECTANGULAR:
    case GradientType::DIAMOND:
      m_propList.insert("draw:style", "square");
      m_propList.insert("draw:cx", 0.5, librevenge::RVNG_PERCENT);
      m_propList.insert("draw:cy", 0.5, librevenge::RVNG_PERCENT);
      m_propList.insert("draw:border", 0.0, librevenge::RVNG_PERCENT);
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
  , m_linkIndexedTextObjectsMap()
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

  if (!m_unprocessedPages.empty())
    draw(true);

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
  addObject<Line>(line, &QXPContentCollector::drawLine);
}

void QXPContentCollector::collectBox(const std::shared_ptr<Box> &box)
{
  addObject<Box>(box, &QXPContentCollector::drawBox);
}

void QXPContentCollector::collectTextBox(const std::shared_ptr<TextBox> &textbox)
{
  addObject<TextBox>(textbox, &QXPContentCollector::drawTextBox);

  if (0 == textbox->linkSettings.linkId)
  {
    QXP_DEBUG_MSG(("Collected textbox with link ID 0"));
  }

  collectTextObject(textbox, getInsertionPage(textbox));
}

void QXPContentCollector::collectTextPath(const std::shared_ptr<TextPath> &textPath)
{
  addObject<TextPath>(textPath, &QXPContentCollector::drawTextPath);

  if (0 == textPath->linkSettings.linkId)
  {
    QXP_DEBUG_MSG(("Collected text path with link ID 0"));
  }

  collectTextObject(textPath, getInsertionPage(textPath));
}

void QXPContentCollector::collectGroup(const std::shared_ptr<Group> &group)
{
  auto collectedObj = addObject<Group>(group, &QXPContentCollector::drawGroup);

  getInsertionPage(group).groups.push_back(collectedObj);
}

void QXPContentCollector::collectText(const std::shared_ptr<Text> &text, const unsigned linkId)
{
  m_linkTextMap[linkId] = text;

  auto it = m_linkIndexedTextObjectsMap.find(linkId);
  if (it != m_linkIndexedTextObjectsMap.end())
  {
    for (const auto &indexTextbox : it->second)
    {
      if (!indexTextbox.second->text)
      {
        indexTextbox.second->text = text;
      }
    }
  }
}

QXPContentCollector::CollectedPage &QXPContentCollector::getInsertionPage(const std::shared_ptr<Object> &obj)
{
  if (m_isCollectingFacingPage && obj->boundingBox.left < m_unprocessedPages.back().settings.offset.left)
  {
    return m_unprocessedPages[m_unprocessedPages.size() - 2];
  }
  return m_unprocessedPages.back();
}

void QXPContentCollector::draw(bool force)
{
  updateLinkedTexts();

  if (hasUnfinishedLinkedTexts())
  {
    if (!force)
    {
      return;
    }
    QXP_DEBUG_MSG(("Drawing with unfinished linked texts\n"));
  }

  for (auto &page : m_unprocessedPages)
  {
    RVNGPropertyList propList;
    propList.insert("svg:width", page.settings.offset.width(), RVNG_POINT);
    propList.insert("svg:height", page.settings.offset.height(), RVNG_POINT);
    m_painter->startPage(propList);

    {
      unsigned i = 0;
      for (auto &obj : boost::adaptors::reverse(page.objects))
      {
        obj->setZIndex(i);
        // we can't just increment by 1 because some objects may need to create several elements (such as box + text)
        // also we can't just have a counter instead of this field because groups may not be consecutive
        i += 100;
      }
    }

    // handle groups first
    // afaik groups that are part of other group never go before that group
    for (const auto &group : page.groups)
    {
      group->draw(page);
    }

    for (auto &obj : page.objects)
    {
      obj->draw(page);
    }

    m_painter->endPage();
  }

  m_unprocessedPages.clear();
}

void QXPContentCollector::collectTextObject(const std::shared_ptr<TextObject> &textObj, CollectedPage &page)
{
  if (textObj->linkSettings.linkedIndex > 0)
  {
    m_linkIndexedTextObjectsMap[textObj->linkSettings.linkId][textObj->linkSettings.linkedIndex] = textObj;
  }

  if (textObj->isLinked())
  {
    page.linkedTextObjects.push_back(textObj);
  }

  if (!textObj->text)
  {
    auto textIt = m_linkTextMap.find(textObj->linkSettings.linkId);
    if (textIt != m_linkTextMap.end())
    {
      textObj->text = textIt->second;
    }
  }
}

void QXPContentCollector::updateLinkedTexts()
{
  for (const auto &page : m_unprocessedPages)
  {
    for (const auto &textObj : page.linkedTextObjects)
    {
      if (textObj->linkSettings.nextLinkedIndex > 0 && !textObj->linkSettings.textLength)
      {
        const auto textObjects = m_linkIndexedTextObjectsMap.at(textObj->linkSettings.linkId);
        const auto nextTextObjectIt = textObjects.find(textObj->linkSettings.nextLinkedIndex);
        if (nextTextObjectIt != textObjects.end())
        {
          textObj->linkSettings.textLength = nextTextObjectIt->second->linkSettings.offsetIntoText - textObj->linkSettings.offsetIntoText;
        }
      }
    }
  }
}

bool QXPContentCollector::hasUnfinishedLinkedTexts()
{
  for (const auto &page : m_unprocessedPages)
  {
    for (const auto &textObj : page.linkedTextObjects)
    {
      if (!textObj->text || (textObj->linkSettings.nextLinkedIndex > 0 && !textObj->linkSettings.textLength))
      {
        return true;
      }
    }
  }

  return false;
}

void QXPContentCollector::drawLine(const std::shared_ptr<Line> &line, const QXPContentCollector::CollectedPage &page)
{
  vector<Point> points =
  {
    page.getPoint(line->boundingBox.topLeft()),
    page.getPoint(line->boundingBox.bottomRight()),
  };

  if (!QXP_ALMOST_ZERO(line->rotation))
  {
    for (auto &p : points)
    {
      p = p.rotateDeg(-line->rotation, page.getPoint(line->boundingBox.center()));
    }
  }

  auto path = createPath(points, true);

  RVNGPropertyList propList;

  writeFrame(propList, line->style, true);

  m_painter->setStyle(propList);
  propList.clear();

  propList.insert("svg:d", path);
  writeZIndex(propList, line->zIndex);

  m_painter->drawPath(propList);
}

void QXPContentCollector::drawBox(const std::shared_ptr<Box> &box, const QXPContentCollector::CollectedPage &page)
{
  switch (box->boxType)
  {
  default:
  case BoxType::RECTANGLE:
    drawRectangle(box, page);
    break;
  case BoxType::OVAL:
    drawOval(box, page);
    break;
  case BoxType::POLYGON:
    drawPolygon(box, page);
    break;
  case BoxType::BEZIER:
    drawBezierBox(box, page);
    break;
  }
}

void QXPContentCollector::drawRectangle(const std::shared_ptr<Box> &box, const QXPContentCollector::CollectedPage &page)
{
  const auto bbox = box->boundingBox.shrink(box->frame.width / 2);
  vector<Point> points =
  {
    page.getPoint(bbox.topLeft()),
    page.getPoint(bbox.topRight()),
    page.getPoint(bbox.bottomRight()),
    page.getPoint(bbox.bottomLeft())
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
  writeZIndex(propList, box->zIndex);

  m_painter->drawPath(propList);
}

void QXPContentCollector::drawOval(const std::shared_ptr<Box> &oval, const QXPContentCollector::CollectedPage &page)
{
  librevenge::RVNGPropertyList propList;

  writeFrame(propList, oval->frame);
  writeFill(propList, oval->fill);

  m_painter->setStyle(propList);
  propList.clear();

  propList.insert("svg:cx", page.getX(oval->boundingBox.center().x), RVNG_POINT);
  propList.insert("svg:cy", page.getY(oval->boundingBox.center().y), RVNG_POINT);
  propList.insert("svg:rx", oval->boundingBox.width() / 2 - oval->frame.width / 2, RVNG_POINT);
  propList.insert("svg:ry", oval->boundingBox.height() / 2 - oval->frame.width / 2, RVNG_POINT);
  if (!QXP_ALMOST_ZERO(oval->rotation))
  {
    propList.insert("librevenge:rotate", oval->rotation);
  }

  writeZIndex(propList, oval->zIndex);

  m_painter->drawEllipse(propList);
}

void QXPContentCollector::drawPolygon(const std::shared_ptr<Box> &polygon, const QXPContentCollector::CollectedPage &page)
{
  vector<Point> points;
  points.reserve(polygon->customPoints.size());
  std::transform(polygon->customPoints.begin(), polygon->customPoints.end(), std::back_inserter(points),
                 [page, &polygon](const Point &p)
  {
    return page.getPoint(p.rotateDeg(-polygon->rotation, polygon->boundingBox.center()));
  });

  auto path = createPath(points, true);

  RVNGPropertyList propList;

  writeFrame(propList, polygon->frame);
  writeFill(propList, polygon->fill);

  m_painter->setStyle(propList);
  propList.clear();

  propList.insert("svg:d", path);
  writeZIndex(propList, polygon->zIndex);

  m_painter->drawPath(propList);
}

void QXPContentCollector::drawBezierBox(const std::shared_ptr<Box> &box, const QXPContentCollector::CollectedPage &page)
{
  drawRectangle(box, page);
}

void QXPContentCollector::drawTextBox(const std::shared_ptr<TextBox> &textbox, const QXPContentCollector::CollectedPage &page)
{
  drawBox(textbox, page);

  const auto bbox = textbox->boundingBox.shrink(textbox->frame.width);

  librevenge::RVNGPropertyList textObjPropList;

  textObjPropList.insert("svg:x", page.getX(bbox.left), RVNG_POINT);
  textObjPropList.insert("svg:y", page.getY(bbox.top), RVNG_POINT);
  textObjPropList.insert("svg:width", bbox.width(), RVNG_POINT);
  textObjPropList.insert("svg:height", bbox.height(), RVNG_POINT);

  textObjPropList.insert("fo:padding-top", 0, RVNG_POINT);
  textObjPropList.insert("fo:padding-right", 0, RVNG_POINT);
  textObjPropList.insert("fo:padding-bottom", 0, RVNG_POINT);
  textObjPropList.insert("fo:padding-left", 3, RVNG_POINT);

  switch (textbox->settings.verticalAlignment)
  {
  case VerticalAlignment::TOP:
    textObjPropList.insert("draw:textarea-vertical-align", "top");
    break;
  case VerticalAlignment::CENTER:
    textObjPropList.insert("draw:textarea-vertical-align", "middle");
    break;
  case VerticalAlignment::BOTTOM:
    textObjPropList.insert("draw:textarea-vertical-align", "bottom");
    break;
  case VerticalAlignment::JUSTIFIED:
    textObjPropList.insert("draw:textarea-vertical-align", "justify");
    break;
  }
  if (!QXP_ALMOST_ZERO(textbox->rotation))
  {
    textObjPropList.insert("librevenge:rotate", -textbox->rotation);
  }

  writeZIndex(textObjPropList, textbox->zIndex + 1);

  m_painter->startTextObject(textObjPropList);

  if (textbox->text)
  {
    drawText(textbox->text.get(), textbox->linkSettings);
  }

  m_painter->endTextObject();
}

void QXPContentCollector::drawTextPath(const std::shared_ptr<TextPath> &textPath, const QXPContentCollector::CollectedPage &page)
{
  drawLine(textPath, page);

  if (!textPath->text || textPath->text.get()->charFormats.empty())
  {
    return;
  }

  double lineY;
  switch (textPath->settings.lineAlignment)
  {
  default:
  case TextPathLineAlignment::TOP:
    lineY = textPath->boundingBox.top - textPath->style.width / 2;
    break;
  case TextPathLineAlignment::CENTER:
    lineY = textPath->boundingBox.top;
    break;
  case TextPathLineAlignment::BOTTOM:
    lineY = textPath->boundingBox.top + textPath->style.width / 2;
    break;
  }

  const double height = std::max_element(textPath->text.get()->charFormats.begin(), textPath->text.get()->charFormats.end(),
                                         [](const CharFormatSpec &f1, const CharFormatSpec &f2)
  {
    return f1.format->fontSize < f2.format->fontSize;
  })->format->fontSize;

  double textY;
  switch (textPath->settings.alignment)
  {
  default:
  case TextPathAlignment::DESCENT:
  case TextPathAlignment::BASELINE:
    textY = lineY - height;
    break;
  case TextPathAlignment::CENTER:
    textY = lineY - height / 2;
    break;
  case TextPathAlignment::ASCENT:
    textY = lineY;
    break;
  }

  librevenge::RVNGPropertyList textObjPropList;

  textObjPropList.insert("svg:x", page.getX(textPath->boundingBox.left), RVNG_POINT);
  textObjPropList.insert("svg:y", page.getY(textY), RVNG_POINT);
  // shouldn't grow vertically
  textObjPropList.insert("svg:width", textPath->boundingBox.width() + textPath->boundingBox.width() / 2, RVNG_POINT);
  textObjPropList.insert("svg:height", height, RVNG_POINT);

  textObjPropList.insert("fo:padding-top", 0, RVNG_POINT);
  textObjPropList.insert("fo:padding-right", 0, RVNG_POINT);
  textObjPropList.insert("fo:padding-bottom", 0, RVNG_POINT);
  textObjPropList.insert("fo:padding-left", 0, RVNG_POINT);

  if (!QXP_ALMOST_ZERO(textPath->rotation))
  {
    textObjPropList.insert("librevenge:rotate", -textPath->rotation);
  }

  writeZIndex(textObjPropList, textPath->zIndex + 1);

  m_painter->startTextObject(textObjPropList);

  drawText(textPath->text.get(), textPath->linkSettings);

  m_painter->endTextObject();
}

void QXPContentCollector::drawText(const std::shared_ptr<Text> &text, const LinkedTextSettings &linkSettings)
{
  unsigned long spanTextStart = linkSettings.offsetIntoText;
  const unsigned long textEnd = linkSettings.textLength ? (spanTextStart + linkSettings.textLength.get()) : text->text.length();

  for (auto &paragraph : text->paragraphs)
  {
    if (paragraph.startIndex >= textEnd)
    {
      break;
    }
    if (spanTextStart > paragraph.endIndex())
    {
      continue;
    }

    librevenge::RVNGPropertyList paragraphPropList;

    paragraphPropList.insert("fo:margin-top", paragraph.format->margin.top, RVNG_POINT);
    paragraphPropList.insert("fo:margin-right", paragraph.format->margin.right, RVNG_POINT);
    paragraphPropList.insert("fo:margin-bottom", paragraph.format->margin.bottom, RVNG_POINT);
    paragraphPropList.insert("fo:margin-left", paragraph.format->margin.left, RVNG_POINT);

    paragraphPropList.insert("fo:text-indent", paragraph.format->firstLineIndent, RVNG_POINT);

    switch (paragraph.format->alignment)
    {
    case HorizontalAlignment::LEFT:
      paragraphPropList.insert("fo:text-align", "left");
      break;
    case HorizontalAlignment::RIGHT:
      paragraphPropList.insert("fo:text-align", "end");
      break;
    case HorizontalAlignment::CENTER:
      paragraphPropList.insert("fo:text-align", "center");
      break;
    case HorizontalAlignment::JUSTIFIED:
    case HorizontalAlignment::FORCED:
      paragraphPropList.insert("fo:text-align", "justify");
      break;
    }

    m_painter->openParagraph(paragraphPropList);

    for (auto &charFormat : text->charFormats)
    {
      if (spanTextStart > paragraph.endIndex() || spanTextStart >= textEnd || charFormat.startIndex > paragraph.endIndex() || charFormat.startIndex >= textEnd)
      {
        break;
      }

      if (spanTextStart > charFormat.endIndex())
      {
        continue;
      }

      if (spanTextStart >= text->text.length())
      {
        QXP_DEBUG_MSG(("Span start %lu out of range\n", spanTextStart));
        break;
      }

      const unsigned long spanTextEnd = std::min<unsigned long>({ charFormat.afterEndIndex(), paragraph.afterEndIndex(), text->text.length(), textEnd });

      librevenge::RVNGPropertyList spanPropList;

      spanPropList.insert("style:font-name", charFormat.format->fontName);
      spanPropList.insert("fo:font-size", charFormat.format->fontSize, librevenge::RVNG_POINT);
      spanPropList.insert("fo:font-weight", charFormat.format->bold ? "bold" : "normal");
      spanPropList.insert("fo:font-style", charFormat.format->italic ? "italic" : "normal");
      if (charFormat.format->underline)
      {
        spanPropList.insert("style:text-underline-type", "single");
        spanPropList.insert("style:text-underline-style", "solid");
      }
      if (charFormat.format->strike)
      {
        spanPropList.insert("style:text-line-through-type", "single");
        spanPropList.insert("style:text-line-through-style", "solid");
      }
      spanPropList.insert("fo:font-variant", charFormat.format->smallCaps ? "small-caps" : "normal");
      spanPropList.insert("style:text-outline", charFormat.format->outline);
      spanPropList.insert("fo:color", charFormat.format->color.toString());

      m_painter->openSpan(spanPropList);

      insertText(m_painter, RVNGString(text->text.substr(spanTextStart, spanTextEnd - spanTextStart).c_str()));

      m_painter->closeSpan();

      spanTextStart = spanTextEnd;
    }

    m_painter->closeParagraph();
  }
}

void QXPContentCollector::drawGroup(const std::shared_ptr<Group> &group, const QXPContentCollector::CollectedPage &page)
{
  bool groupOpened = false;

  for (const unsigned &ind : group->objectsIndexes)
  {
    if (ind >= page.objects.size())
    {
      QXP_DEBUG_MSG(("Group element %u not found\n", ind));
      continue;
    }
    const auto &obj = page.objects[ind];

    if (!groupOpened)
    {
      RVNGPropertyList propList;
      writeZIndex(propList, obj->zIndex() - 1);
      m_painter->openGroup(propList);

      groupOpened = true;
    }

    obj->draw(page);
  }

  if (groupOpened)
  {
    m_painter->closeGroup();
  }
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

void QXPContentCollector::writeFrame(librevenge::RVNGPropertyList &propList, const Frame &frame, bool allowHairline)
{
  propList.insert("draw:stroke", "none");

  if (frame.color && (allowHairline || !QXP_ALMOST_ZERO(frame.width)))
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
