/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "OpenLayoutCollector.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <set>

#include <boost/variant.hpp>

#include "QXPHeader.h"

namespace libqxp
{

using openlayout::JsonValue;
using std::string;
using std::vector;

namespace
{

const double PT_PER_MM = 72.0 / 25.4;

string formatNumber(const double value, const int decimals = 3)
{
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.*f", decimals, value);
  string str(buf);
  if (str.find('.') != string::npos)
  {
    while (!str.empty() && str.back() == '0')
      str.pop_back();
    if (!str.empty() && str.back() == '.')
      str.pop_back();
  }
  if (str == "-0")
    str = "0";
  return str;
}

string pt(const double value)
{
  const string num = formatNumber(value);
  return num == "0" ? num : num + "pt";
}

string percent(const double fraction)
{
  return formatNumber(fraction * 100, 2) + "%";
}

/// Transliteration of common Latin-1 letters (given as the second byte of their UTF-8 form, after 0xc3).
const char *transliterate(const unsigned char c)
{
  static const char *const table[64] =
  {
    "a", "a", "a", "a", "ae", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
    "d", "n", "o", "o", "o", "o", "oe", nullptr, "o", "u", "u", "u", "ue", "y", "th", "ss",
    "a", "a", "a", "a", "ae", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
    "d", "n", "o", "o", "o", "o", "oe", nullptr, "o", "u", "u", "u", "ue", "y", "th", "y"
  };
  return c >= 0x80 && c < 0xc0 ? table[c - 0x80] : nullptr;
}

string slugify(const string &name)
{
  string slug;
  bool dash = false;
  for (size_t i = 0; i < name.size(); ++i)
  {
    const unsigned char uc = static_cast<unsigned char>(name[i]);
    const char *const replacement = uc == 0xc3 && i + 1 < name.size() ? transliterate(static_cast<unsigned char>(name[i + 1])) : nullptr;
    if (replacement)
    {
      slug.append(replacement);
      dash = false;
      ++i;
    }
    else if ((uc >= 'a' && uc <= 'z') || (uc >= '0' && uc <= '9'))
    {
      slug.push_back(char(uc));
      dash = false;
    }
    else if (uc >= 'A' && uc <= 'Z')
    {
      slug.push_back(char(uc - 'A' + 'a'));
      dash = false;
    }
    else if (!dash && !slug.empty() && (uc < 0x80 || uc >= 0xc0))
    {
      slug.push_back('-');
      dash = true;
    }
  }
  while (!slug.empty() && slug.back() == '-')
    slug.pop_back();
  return slug;
}

/// Maps the QuarkXPress model tag to the model names of the spec (3.3).
string colorModel(const string &tag, const bool hasCmyk)
{
  if (tag == "QCMK")
    return "cmyk";
  if (tag == "QRGB")
    return "rgb";
  if (tag == "QHSB")
    return "hsb";
  if (tag == "QLAB")
    return "lab";
  if (tag == "PnHx")
    return "hexachrome";
  if (!tag.empty())
    return tag;
  return hasCmyk ? "cmyk" : "rgb";
}

JsonValue node(const char *const type)
{
  JsonValue n = JsonValue::object();
  n.set("type", type);
  return n;
}

JsonValue textNode(const string &value)
{
  JsonValue n = node("text");
  n.set("value", value);
  return n;
}

string versionName(const unsigned version)
{
  switch (version)
  {
  case QXP_1:
    return "1.x";
  case QXP_31_MAC:
  case QXP_31:
    return "3.1";
  case QXP_33:
    return "3.3";
  case QXP_4:
    return "4.x";
  default:
    break;
  }
  char buf[16];
  std::snprintf(buf, sizeof(buf), "0x%x", version);
  return buf;
}

string tabAlign(const TabStopType type)
{
  switch (type)
  {
  case TabStopType::CENTER:
    return "center";
  case TabStopType::RIGHT:
    return "right";
  case TabStopType::ALIGN:
    return "char";
  case TabStopType::LEFT:
  default:
    break;
  }
  return "left";
}

/// Default values used when a local format removes a property set by a style.
JsonValue defaultValue(const string &property)
{
  if (property == "fontWeight")
    return JsonValue(400);
  if (property == "fontStyle" || property == "fontVariantCaps")
    return JsonValue("normal");
  if (property == "textDecorationLine" || property == "textTransform" || property == "borderTop" || property == "borderBottom")
    return JsonValue("none");
  if (property == "ruleAbove" || property == "ruleBelow")
    return JsonValue("none");
  if (property == "verticalAlign")
    return JsonValue("baseline");
  if (property == "fontVariantPosition")
    return JsonValue("normal");
  if (property == "textStroke" || property == "textShadow" || property == "contentTransform")
    return JsonValue("none");
  if (property == "baselineShift")
    return JsonValue("0");
  if (property == "hj")
    return JsonValue();
  if (property == "textAlignLast")
    return JsonValue("auto");
  if (property == "tabStops")
    return JsonValue::array();
  if (property.compare(0, 4, "qxp:") == 0)
    return JsonValue(false);
  if (property.compare(0, 6, "margin") == 0 || property == "textIndent")
    return JsonValue("0");
  return JsonValue("initial");
}

/// Properties of local that differ from base; properties only in base are reset.
JsonValue diffStyle(const JsonValue &local, const JsonValue &base)
{
  JsonValue diff = JsonValue::object();
  for (const auto &member : local.members())
  {
    const JsonValue *const baseValue = base.get(member.first);
    if (!baseValue || *baseValue != member.second)
      diff.set(member.first, member.second);
  }
  for (const auto &member : base.members())
  {
    if (!local.has(member.first))
      diff.set(member.first, defaultValue(member.first));
  }
  return diff;
}

/// Moves the hj reference out of the style object: in the spec it is a key of the node.
void takeHj(JsonValue &n, JsonValue &style)
{
  const JsonValue *const hj = style.get("hj");
  if (!hj)
    return;
  if (!hj->isNull())
    n.set("hj", *hj);
  style.erase("hj");
}

void mergeStyle(JsonValue &target, const JsonValue &source)
{
  for (const auto &member : source.members())
    target.set(member.first, member.second);
}

bool isSameStyle(const JsonValue &a, const JsonValue &b)
{
  return a.toString() == b.toString();
}

/// Converts a path as stored by QuarkXPress to a slash separated path.
string convertPath(const string &path)
{
  string result(path);
  if (path.find('\\') != string::npos)
  {
    std::replace(result.begin(), result.end(), '\\', '/');
  }
  else if (path.find(':') != string::npos && path.find('/') == string::npos)
  {
    // classic Mac OS path: "Volume:Folder:File"
    std::replace(result.begin(), result.end(), ':', '/');
    result.insert(0, "/");
  }
  return result;
}

struct Geometry
{
  double left;
  double top;
  double width;
  double height;
};

Rect boundsOf(const std::vector<Point> &points)
{
  Rect r(points.front().y, points.front().x, points.front().y, points.front().x);
  for (const auto &p : points)
  {
    r.left = std::min(r.left, p.x);
    r.right = std::max(r.right, p.x);
    r.top = std::min(r.top, p.y);
    r.bottom = std::max(r.bottom, p.y);
  }
  return r;
}

}

/** Builds the OpenLayout JSON tree from the collected content.
  */
class OpenLayoutBuilder
{
  // disable copying
  OpenLayoutBuilder(const OpenLayoutBuilder &other) = delete;
  OpenLayoutBuilder &operator=(const OpenLayoutBuilder &other) = delete;

public:
  OpenLayoutBuilder(const OpenLayoutOptions &options,
                    const QXPDocumentProperties &docProps,
                    const vector<ColorSpec> &colors,
                    const vector<ParagraphStyle> &paragraphStyles,
                    const vector<CharacterStyle> &characterStyles,
                    const vector<OpenLayoutCollector::Spread> &spreads,
                    const std::unordered_map<unsigned, std::shared_ptr<Text>> &texts);

  JsonValue build();

private:
  typedef OpenLayoutCollector::Item Item;
  typedef OpenLayoutCollector::ItemKind ItemKind;
  typedef OpenLayoutCollector::Spread Spread;

  struct ChainInfo
  {
    string storyId;
    std::map<const TextObject *, unsigned> flowIndexes;

    ChainInfo()
      : storyId(), flowIndexes()
    { }
  };

  struct PageItems
  {
    JsonValue page;
    vector<JsonValue> items;

    PageItems()
      : page(), items()
    { }
  };

  string uniqueId(const string &base);

  // units
  double conv(double value) const;
  string len(double value) const;

  // resources
  void buildColors();
  void buildStyles();
  string colorValue(const boost::optional<ColorRef> &ref, const Color &fallback) const;
  string fontId(const string &family);

  // formats
  string hjId(const HJ &hj);
  JsonValue paragraphCss(const ParagraphFormat &format);
  JsonValue charCss(const CharFormat &format);
  string ruleCss(const ParagraphRule &rule) const;
  JsonValue ruleExtra(const ParagraphRule &rule) const;
  const JsonValue &paragraphStyleCharCss(int styleIndex) const;
  string paragraphStyleId(int styleIndex) const;
  string characterStyleId(int styleIndex) const;

  // text
  void collectChains();
  JsonValue blocks(const Text &text, unsigned start, unsigned end);
  void appendRun(JsonValue &children, const string &styleClass, const JsonValue &style, const string &utf8);

  // items
  vector<unsigned> topLevelItems(const Spread &spread, size_t pageIndex) const;
  size_t pageIndexOf(const Spread &spread, const Object &object) const;
  JsonValue item(const Spread &spread, unsigned index, double originX, double originY);
  JsonValue lineItem(const Line &line, double originX, double originY);
  JsonValue boxItem(const Item &item, const Spread &spread, double originX, double originY);
  JsonValue groupItem(const Group &group, const Spread &spread, double originX, double originY);
  void setGeometry(JsonValue &style, const Geometry &geometry) const;
  void setTransform(JsonValue &style, double rotation, double skew, bool hflip, bool vflip) const;
  void setFill(JsonValue &style, const Box &box, bool svgPaint) const;
  string strokeStyleId(const LineStyle &lineStyle);
  void setFrame(JsonValue &style, const Frame &frame, bool isLine);
  string bezierPath(const vector<CurveComponent> &components, double originX, double originY, bool closable) const;
  string polygonPath(const vector<Point> &points, double originX, double originY) const;
  void setText(JsonValue &n, const TextObject &textObject);

  // pages
  JsonValue pageStyle(const PageSettings &settings, bool leftPage) const;
  void buildMasters(JsonValue &masters);
  void buildPages(JsonValue &pages);
  vector<JsonValue> linkToMaster(const vector<JsonValue> &items, const vector<JsonValue> &masterItems) const;
  vector<JsonValue> detachFromMaster(const vector<JsonValue> &items, const vector<JsonValue> &masterItems) const;

  JsonValue head();

  const OpenLayoutOptions &m_options;
  const QXPDocumentProperties &m_docProps;
  const vector<ColorSpec> &m_colors;
  const vector<ParagraphStyle> &m_paragraphStyles;
  const vector<CharacterStyle> &m_characterStyles;
  const vector<Spread> &m_spreads;
  const std::unordered_map<unsigned, std::shared_ptr<Text>> &m_texts;

  std::set<string> m_usedIds;
  std::map<unsigned, string> m_colorIds;
  JsonValue m_colorNodes;
  vector<string> m_paragraphStyleIds;
  vector<string> m_characterStyleIds;
  vector<JsonValue> m_paragraphStyleCharCss;
  vector<JsonValue> m_characterStyleCss;
  vector<JsonValue> m_paragraphStyleParaCss;
  JsonValue m_styleNodes;
  JsonValue m_hjNodes;
  std::map<string, string> m_hjIds;
  JsonValue m_strokeStyleNodes;
  std::map<string, string> m_strokeStyleIds;
  vector<string> m_fontFamilies;
  std::map<string, string> m_fontIds;
  std::unordered_map<unsigned, ChainInfo> m_chains;
  JsonValue m_stories;
  vector<string> m_masterIds; // by 1-based master index - 1
  std::map<string, vector<PageItems>> m_masterPages;
  bool m_facing;
  JsonValue m_empty;
};

OpenLayoutBuilder::OpenLayoutBuilder(const OpenLayoutOptions &options,
                                     const QXPDocumentProperties &docProps,
                                     const vector<ColorSpec> &colors,
                                     const vector<ParagraphStyle> &paragraphStyles,
                                     const vector<CharacterStyle> &characterStyles,
                                     const vector<Spread> &spreads,
                                     const std::unordered_map<unsigned, std::shared_ptr<Text>> &texts)
  : m_options(options)
  , m_docProps(docProps)
  , m_colors(colors)
  , m_paragraphStyles(paragraphStyles)
  , m_characterStyles(characterStyles)
  , m_spreads(spreads)
  , m_texts(texts)
  , m_usedIds()
  , m_colorIds()
  , m_colorNodes(JsonValue::array())
  , m_paragraphStyleIds()
  , m_characterStyleIds()
  , m_paragraphStyleCharCss()
  , m_characterStyleCss()
  , m_paragraphStyleParaCss()
  , m_styleNodes(JsonValue::array())
  , m_hjNodes(JsonValue::array())
  , m_hjIds()
  , m_strokeStyleNodes(JsonValue::array())
  , m_strokeStyleIds()
  , m_fontFamilies()
  , m_fontIds()
  , m_chains()
  , m_stories(JsonValue::array())
  , m_masterIds()
  , m_masterPages()
  , m_facing(false)
  , m_empty(JsonValue::object())
{
  unsigned masterNumber = 0;
  for (const auto &spread : spreads)
  {
    if (spread.page.isMaster)
      ++masterNumber;
    // the built-in "blank facing pages" master does not make the document facing
    if (spread.page.isFacing() && !(spread.page.isMaster && masterNumber == 2 && spread.items.empty()))
      m_facing = true;
  }
}

string OpenLayoutBuilder::uniqueId(const string &base)
{
  string id = base.empty() ? "id" : base;
  if (m_usedIds.insert(id).second)
    return id;
  for (unsigned i = 2;; ++i)
  {
    const string candidate = id + "-" + std::to_string(i);
    if (m_usedIds.insert(candidate).second)
      return candidate;
  }
}

double OpenLayoutBuilder::conv(const double value) const
{
  return m_options.unit == OpenLayoutUnit::MILLIMETER ? value / PT_PER_MM : value;
}

string OpenLayoutBuilder::len(const double value) const
{
  if (m_options.unit == OpenLayoutUnit::POINT)
    return pt(value);
  const string num = formatNumber(conv(value));
  return num == "0" ? num : num + "mm";
}

void OpenLayoutBuilder::buildColors()
{
  for (const auto &color : m_colors)
  {
    if (m_colorIds.find(color.id) != m_colorIds.end())
      continue;
    const string name = color.name.cstr();
    const string id = uniqueId(slugify(name).empty() ? "color-" + std::to_string(color.id) : slugify(name));
    m_colorIds[color.id] = id;

    JsonValue n = node("color");
    n.set("id", id);
    n.set("name", name);
    if (color.cmyk && color.cmyk->size() == 4)
    {
      const auto &c = color.cmyk.get();
      n.set("value", "color(device-cmyk " + formatNumber(c[0], 4) + " " + formatNumber(c[1], 4) + " " + formatNumber(c[2], 4) + " " + formatNumber(c[3], 4) + ")");
    }
    else
    {
      n.set("value", color.rgb.toString().cstr());
    }
    if (color.spot && color.spot.get())
      n.set("spot", true);
    if (color.registration)
      n.set("registration", true);
    const string model = colorModel(color.model, bool(color.cmyk));
    if (!model.empty())
      n.set("model", model);
    if (color.cmyk)
    {
      // the RGB value is the screen alternate of a CMYK definition
      JsonValue alternates = JsonValue::object();
      alternates.set("rgb", color.rgb.toString().cstr());
      n.set("alternates", alternates);
    }
    n.set("qxp:index", color.id);
    m_colorNodes.push(n);
  }
}

string OpenLayoutBuilder::colorValue(const boost::optional<ColorRef> &ref, const Color &fallback) const
{
  if (ref)
  {
    const auto it = m_colorIds.find(ref->id);
    if (it != m_colorIds.end())
    {
      const string var = "var(--" + it->second + ")";
      if (ref->shade >= 0 && ref->shade < 1.0 - 1e-6)
        return "color-mix(in srgb, " + var + " " + percent(ref->shade) + ", transparent)";
      return var;
    }
  }
  return fallback.toString().cstr();
}

string OpenLayoutBuilder::fontId(const string &family)
{
  const auto it = m_fontIds.find(family);
  if (it != m_fontIds.end())
    return it->second;
  const string id = uniqueId("font-" + slugify(family));
  m_fontIds[family] = id;
  m_fontFamilies.push_back(family);
  return id;
}

JsonValue OpenLayoutBuilder::charCss(const CharFormat &format)
{
  JsonValue css = JsonValue::object();
  const string family = format.fontName.cstr();
  if (!family.empty())
  {
    fontId(family);
    css.set("fontFamily", family);
  }
  css.set("fontSize", pt(format.fontSize));
  if (format.bold)
    css.set("fontWeight", 700);
  if (format.italic)
    css.set("fontStyle", "italic");

  string decoration;
  if (format.underline || format.wordUnderline)
    decoration = "underline";
  if (format.strike)
    decoration += decoration.empty() ? "line-through" : " line-through";
  if (!decoration.empty())
    css.set("textDecorationLine", decoration);
  if (format.wordUnderline)
    css.set("textDecorationSkipSpaces", "all");

  if (format.allCaps)
    css.set("textTransform", "uppercase");
  if (format.smallCaps)
    css.set("fontVariantCaps", "small-caps");

  if (format.superscript)
    css.set("verticalAlign", "super");
  else if (format.subscript)
    css.set("verticalAlign", "sub");
  if (format.superior)
    css.set("fontVariantPosition", "superior");
  // baselineShift is added to the shift of verticalAlign / fontVariantPosition
  if (!QXP_ALMOST_ZERO(format.baselineShift))
    css.set("baselineShift", formatNumber(format.baselineShift, 4) + "em");
  if (format.outline)
    css.set("textStroke", "auto");
  if (format.shadow)
    css.set("textShadow", "auto");

  css.set("color", colorValue(format.colorRef, format.color));
  return css;
}

string OpenLayoutBuilder::ruleCss(const ParagraphRule &rule) const
{
  string style = "solid";
  if (rule.lineStyle)
  {
    if (rule.lineStyle->isStripe)
      style = "double";
    else if (rule.lineStyle->segmentLengths.size() > 1)
      style = "dashed";
  }
  return pt(rule.width) + " " + style + " " + colorValue(rule.colorRef, rule.color);
}

JsonValue OpenLayoutBuilder::ruleExtra(const ParagraphRule &rule) const
{
  JsonValue extra = JsonValue::object();
  extra.set("offset", QXP_ALMOST_ZERO(rule.offset) ? JsonValue("0") : JsonValue(pt(rule.offset)));
  extra.set("indentLeft", len(rule.leftMargin));
  extra.set("indentRight", len(rule.rightMargin));
  // libqxp does not read the length mode yet; the spec default is the column width
  return extra;
}

/** Returns the id of an hj resource (spec 3.5) with these settings, creating it on first use.
  *
  * QuarkXPress H&J sets have names, but libqxp does not read them, so the sets are
  * deduplicated by their values and numbered.
  */
string OpenLayoutBuilder::hjId(const HJ &hj)
{
  JsonValue style = JsonValue::object();
  style.set("hyphens", hj.hyphenate ? "auto" : "manual");
  style.set("hyphenateLimitChars", "auto " + std::to_string(hj.minBefore) + " " + std::to_string(hj.minAfter));
  style.set("hyphenateLimitLines", hj.maxInRow == 0 ? JsonValue("no-limit") : JsonValue(hj.maxInRow));
  if (!hj.singleWordJustify)
    style.set("singleWordJustify", false);

  const string key = style.toString();
  const auto it = m_hjIds.find(key);
  if (it != m_hjIds.end())
    return it->second;

  const string id = uniqueId("hj-" + std::to_string(m_hjIds.size() + 1));
  m_hjIds[key] = id;
  JsonValue n = node("hj");
  n.set("id", id);
  n.set("style", style);
  m_hjNodes.push(n);
  return id;
}

JsonValue OpenLayoutBuilder::paragraphCss(const ParagraphFormat &format)
{
  JsonValue css = JsonValue::object();

  switch (format.alignment)
  {
  case HorizontalAlignment::LEFT:
    break;
  case HorizontalAlignment::CENTER:
    css.set("textAlign", "center");
    break;
  case HorizontalAlignment::RIGHT:
    css.set("textAlign", "right");
    break;
  case HorizontalAlignment::JUSTIFIED:
    css.set("textAlign", "justify");
    break;
  case HorizontalAlignment::FORCED:
    css.set("textAlign", "justify");
    css.set("textAlignLast", "justify");
    break;
  }

  if (!QXP_ALMOST_ZERO(format.margin.left))
    css.set("marginLeft", len(format.margin.left));
  if (!QXP_ALMOST_ZERO(format.margin.right))
    css.set("marginRight", len(format.margin.right));
  if (!QXP_ALMOST_ZERO(format.firstLineIndent))
    css.set("textIndent", len(format.firstLineIndent));
  if (!QXP_ALMOST_ZERO(format.margin.top))
    css.set("marginTop", len(format.margin.top));
  if (!QXP_ALMOST_ZERO(format.margin.bottom))
    css.set("marginBottom", len(format.margin.bottom));

  if (!QXP_ALMOST_ZERO(format.leading) && !format.incrementalLeading)
  {
    css.set("lineHeight", pt(format.leading));
  }
  else
  {
    // auto leading, possibly with an increment
    string autoLeading;
    if (m_docProps.isIncrementalAutoLeading())
      autoLeading = "1em + " + pt(m_docProps.autoLeading());
    else
      autoLeading = formatNumber(1 + m_docProps.autoLeading(), 4) + "em";
    if (QXP_ALMOST_ZERO(format.leading) && !m_docProps.isIncrementalAutoLeading())
      css.set("lineHeight", std::stod(formatNumber(1 + m_docProps.autoLeading(), 4)));
    else if (QXP_ALMOST_ZERO(format.leading))
      css.set("lineHeight", "calc(" + autoLeading + ")");
    else
      css.set("lineHeight", "calc(" + autoLeading + (format.leading < 0 ? " - " : " + ") + pt(std::fabs(format.leading)) + ")");
  }

  if (format.hj)
    css.set("hj", hjId(*format.hj));

  if (!format.tabStops.empty())
  {
    JsonValue tabs = JsonValue::array();
    for (const auto &tab : format.tabStops)
    {
      JsonValue t = JsonValue::object();
      t.set("position", len(tab.position));
      t.set("align", tabAlign(tab.type));
      const string leader = tab.fillChar.cstr();
      if (!leader.empty() && leader != " " && leader[0] != '\0')
        t.set("leader", leader);
      if (tab.type == TabStopType::ALIGN)
        t.set("character", tab.alignChar.cstr());
      tabs.push(t);
    }
    css.set("tabStops", tabs);
  }

  if (format.ruleAbove)
  {
    css.set("borderTop", ruleCss(*format.ruleAbove));
    css.set("ruleAbove", ruleExtra(*format.ruleAbove));
  }
  if (format.ruleBelow)
  {
    css.set("borderBottom", ruleCss(*format.ruleBelow));
    css.set("ruleBelow", ruleExtra(*format.ruleBelow));
  }

  return css;
}

string OpenLayoutBuilder::paragraphStyleId(const int styleIndex) const
{
  if (styleIndex <= 0 || size_t(styleIndex) > m_paragraphStyleIds.size())
    return string();
  return m_paragraphStyleIds[size_t(styleIndex) - 1];
}

string OpenLayoutBuilder::characterStyleId(const int styleIndex) const
{
  if (styleIndex <= 0 || size_t(styleIndex) > m_characterStyleIds.size())
    return string();
  return m_characterStyleIds[size_t(styleIndex) - 1];
}

const JsonValue &OpenLayoutBuilder::paragraphStyleCharCss(const int styleIndex) const
{
  if (styleIndex <= 0 || size_t(styleIndex) > m_paragraphStyleCharCss.size())
    return m_empty;
  return m_paragraphStyleCharCss[size_t(styleIndex) - 1];
}

void OpenLayoutBuilder::buildStyles()
{
  for (const auto &style : m_characterStyles)
  {
    const string name = style.name.cstr();
    m_characterStyleIds.push_back(uniqueId("cs-" + (slugify(name).empty() ? std::to_string(m_characterStyleIds.size() + 1) : slugify(name))));
  }
  for (const auto &style : m_paragraphStyles)
  {
    const string name = style.name.cstr();
    m_paragraphStyleIds.push_back(uniqueId("ps-" + (slugify(name).empty() ? std::to_string(m_paragraphStyleIds.size() + 1) : slugify(name))));
  }

  for (size_t i = 0; i < m_characterStyles.size(); ++i)
    m_characterStyleCss.push_back(charCss(m_characterStyles[i].format));

  for (size_t i = 0; i < m_paragraphStyles.size(); ++i)
  {
    const auto &style = m_paragraphStyles[i];
    m_paragraphStyleParaCss.push_back(paragraphCss(style.paragraphFormat));
    if (style.charStyleIndex > 0 && size_t(style.charStyleIndex) <= m_characterStyleCss.size())
      m_paragraphStyleCharCss.push_back(m_characterStyleCss[size_t(style.charStyleIndex) - 1]);
    else
      m_paragraphStyleCharCss.push_back(charCss(style.charFormat));
  }

  for (size_t i = 0; i < m_paragraphStyles.size(); ++i)
  {
    const auto &style = m_paragraphStyles[i];
    JsonValue n = node("paragraphStyle");
    n.set("id", m_paragraphStyleIds[i]);
    n.set("name", style.name.cstr());
    const string next = paragraphStyleId(int(style.nextIndex));
    if (!next.empty())
      n.set("next", next);
    // the paragraph style takes its character formatting from a character style (spec 3.4)
    const string charStyle = characterStyleId(style.charStyleIndex);
    JsonValue css = m_paragraphStyleParaCss[i];
    if (charStyle.empty())
    {
      mergeStyle(css, m_paragraphStyleCharCss[i]);
    }
    else
    {
      n.set("characterStyle", charStyle);
      // only what this style changes on top of the referenced character style
      mergeStyle(css, diffStyle(m_paragraphStyleCharCss[i], m_characterStyleCss[size_t(style.charStyleIndex) - 1]));
    }
    takeHj(n, css);
    n.set("style", css);
    m_styleNodes.push(n);
  }

  for (size_t i = 0; i < m_characterStyles.size(); ++i)
  {
    JsonValue n = node("characterStyle");
    n.set("id", m_characterStyleIds[i]);
    n.set("name", m_characterStyles[i].name.cstr());
    n.set("style", m_characterStyleCss[i]);
    m_styleNodes.push(n);
  }
}

void OpenLayoutBuilder::appendRun(JsonValue &children, const string &styleClass, const JsonValue &style, const string &utf8)
{
  // split the run into text, line breaks and special characters
  JsonValue pieces = JsonValue::array();
  string current;
  const auto flush = [&]()
  {
    if (!current.empty())
    {
      pieces.push(textNode(current));
      current.clear();
    }
  };

  for (const char c : utf8)
  {
    switch (c)
    {
    case '\r':
      break;
    case '\n':
      flush();
      pieces.push(node("br"));
      break;
    case 0x0b:
    {
      flush();
      JsonValue br = node("br");
      br.set("break", "column");
      pieces.push(br);
      break;
    }
    case 0x0c:
    {
      flush();
      JsonValue br = node("br");
      br.set("break", "frame");
      pieces.push(br);
      break;
    }
    case 0x02:
    {
      flush();
      JsonValue field = node("field");
      field.set("name", "previousFramePageNumber");
      pieces.push(field);
      break;
    }
    case 0x03:
    {
      flush();
      JsonValue field = node("field");
      field.set("name", "nextFramePageNumber");
      pieces.push(field);
      break;
    }
    case 0x04:
    {
      flush();
      JsonValue field = node("field");
      field.set("name", "pageNumber");
      pieces.push(field);
      break;
    }
    case 0x07:
      flush();
      pieces.push(node("br"));
      break;
    case 0x08:
      flush();
      pieces.push(node("indentHere"));
      break;
    case 0x0f:
      current.append("\xc2\xa0"); // nonbreaking space
      break;
    case 0x1f:
      current.append("\xc2\xad"); // discretionary hyphen -> soft hyphen
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20 && c != '\t')
      {
        flush();
        JsonValue special = node("qxp:char");
        special.set("code", unsigned(static_cast<unsigned char>(c)));
        pieces.push(special);
      }
      else
      {
        current.push_back(c);
      }
      break;
    }
  }
  flush();

  if (pieces.empty())
    return;

  const bool plain = styleClass.empty() && style.empty();

  // continue the previous span if it has the same formatting
  JsonValue *target = nullptr;
  if (!children.items().empty())
  {
    JsonValue &last = children.items().back();
    const JsonValue *const lastType = last.get("type");
    if (!plain && lastType && lastType->asString() == "span")
    {
      const JsonValue *const lastClass = last.get("class");
      const JsonValue *const lastStyle = last.get("style");
      const string lastClassName = lastClass ? lastClass->asString() : string();
      if (lastClassName == styleClass && isSameStyle(lastStyle ? *lastStyle : m_empty, style))
        target = last.get("children");
    }
  }

  JsonValue *list = &children;
  if (!plain && !target)
  {
    JsonValue span = node("span");
    if (!styleClass.empty())
      span.set("class", styleClass);
    if (!style.empty())
      span.set("style", style);
    span.set("children", JsonValue::array());
    JsonValue &added = children.push(span);
    list = added.get("children");
  }
  else if (target)
  {
    list = target;
  }

  for (const auto &piece : pieces.items())
  {
    if (!list->items().empty() && piece.get("type")->asString() == "text")
    {
      JsonValue &last = list->items().back();
      if (last.get("type")->asString() == "text")
      {
        last.set("value", last.get("value")->asString() + piece.get("value")->asString());
        continue;
      }
    }
    list->push(piece);
  }
}

JsonValue OpenLayoutBuilder::blocks(const Text &text, const unsigned start, const unsigned end)
{
  JsonValue result = JsonValue::array();
  const unsigned textEnd = std::min<unsigned>(end, unsigned(text.text.length()));

  for (const auto &paragraph : text.paragraphs)
  {
    const unsigned pStart = std::max(paragraph.startIndex, start);
    const unsigned pEnd = std::min(paragraph.afterEndIndex(), textEnd);
    if (paragraph.length == 0 || pStart >= pEnd)
      continue;

    JsonValue p = node("p");
    const string styleId = paragraphStyleId(paragraph.format->styleIndex);
    if (!styleId.empty())
      p.set("class", styleId);

    const JsonValue css = paragraphCss(*paragraph.format);
    const JsonValue baseCss = styleId.empty() ? m_empty : m_paragraphStyleParaCss[size_t(paragraph.format->styleIndex) - 1];
    JsonValue localCss = diffStyle(css, baseCss);
    takeHj(p, localCss);
    if (!localCss.empty())
      p.set("style", localCss);

    const JsonValue &paragraphCharCss = styleId.empty() ? m_empty : paragraphStyleCharCss(paragraph.format->styleIndex);
    const int paragraphCharStyle = styleId.empty() ? 0 : m_paragraphStyles[size_t(paragraph.format->styleIndex) - 1].charStyleIndex;

    JsonValue children = JsonValue::array();
    for (const auto &charFormat : text.charFormats)
    {
      const unsigned rStart = std::max(charFormat.startIndex, pStart);
      const unsigned rEnd = std::min(charFormat.afterEndIndex(), pEnd);
      if (charFormat.length == 0 || rStart >= rEnd)
        continue;
      if (charFormat.format->isControlChars)
        continue;

      const string raw = text.text.substr(rStart, rEnd - rStart);
      librevenge::RVNGString converted;
      appendCharacters(converted, raw.c_str(), raw.size(), text.encoding);

      string spanClass;
      const JsonValue *base = &paragraphCharCss;
      const int charStyle = charFormat.format->styleIndex;
      if (charStyle > 0 && charStyle != paragraphCharStyle && !characterStyleId(charStyle).empty())
      {
        spanClass = characterStyleId(charStyle);
        base = &m_characterStyleCss[size_t(charStyle) - 1];
      }
      const JsonValue spanCss = diffStyle(charCss(*charFormat.format), *base);
      appendRun(children, spanClass, spanCss, converted.cstr());
    }
    if (!children.empty())
      p.set("children", children);
    result.push(p);
  }

  return result;
}

void OpenLayoutBuilder::collectChains()
{
  std::unordered_map<unsigned, vector<const TextObject *>> chains;
  vector<unsigned> order;

  std::function<void(const Spread &)> visit = [&](const Spread &spread)
  {
    for (const auto &entry : spread.items)
    {
      const TextObject *textObject = nullptr;
      if (entry.second.kind == ItemKind::TEXT_BOX)
        textObject = std::dynamic_pointer_cast<TextBox>(entry.second.object).get();
      else if (entry.second.kind == ItemKind::TEXT_PATH)
        textObject = std::dynamic_pointer_cast<TextPath>(entry.second.object).get();
      if (!textObject || textObject->linkSettings.linkId == 0)
        continue;
      const unsigned linkId = textObject->linkSettings.linkId;
      if (chains.find(linkId) == chains.end())
        order.push_back(linkId);
      chains[linkId].push_back(textObject);
    }
  };
  for (const auto &spread : m_spreads)
    visit(spread);

  for (const unsigned linkId : order)
  {
    auto &objects = chains[linkId];
    const bool linked = objects.size() > 1 || objects.front()->isLinked() || objects.front()->linkSettings.offsetIntoText > 0;
    if (!linked)
      continue;

    const auto textIt = m_texts.find(linkId);
    if (textIt == m_texts.end())
      continue;

    std::stable_sort(objects.begin(), objects.end(), [](const TextObject *a, const TextObject *b)
    {
      return a->linkSettings.offsetIntoText < b->linkSettings.offsetIntoText;
    });

    ChainInfo info;
    info.storyId = uniqueId("story-" + std::to_string(linkId));
    for (unsigned i = 0; i < objects.size(); ++i)
      info.flowIndexes[objects[i]] = i;

    JsonValue story = node("story");
    story.set("id", info.storyId);
    const JsonValue content = blocks(*textIt->second, 0, unsigned(textIt->second->text.length()));
    if (!content.empty())
      story.set("children", content);
    m_stories.push(story);

    m_chains[linkId] = info;
  }
}

void OpenLayoutBuilder::setText(JsonValue &n, const TextObject &textObject)
{
  const unsigned linkId = textObject.linkSettings.linkId;
  const auto chainIt = m_chains.find(linkId);
  if (linkId != 0 && chainIt != m_chains.end())
  {
    n.set("flowFrom", chainIt->second.storyId);
    const auto indexIt = chainIt->second.flowIndexes.find(&textObject);
    if (indexIt != chainIt->second.flowIndexes.end())
      n.set("flowIndex", indexIt->second);
    return;
  }

  std::shared_ptr<Text> text;
  if (textObject.text)
    text = textObject.text.get();
  else
  {
    const auto textIt = m_texts.find(linkId);
    if (textIt != m_texts.end())
      text = textIt->second;
  }
  if (!text)
    return;

  const JsonValue content = blocks(*text, 0, unsigned(text->text.length()));
  if (!content.empty())
    n.set("children", content);
}

size_t OpenLayoutBuilder::pageIndexOf(const Spread &spread, const Object &object) const
{
  if (spread.page.isFacing() && object.boundingBox.left < spread.page.pageSettings[1].offset.left)
    return 0;
  return spread.page.isFacing() ? 1 : 0;
}

vector<unsigned> OpenLayoutBuilder::topLevelItems(const Spread &spread, const size_t pageIndex) const
{
  std::set<unsigned> grouped;
  for (const auto &entry : spread.items)
  {
    if (entry.second.kind != ItemKind::GROUP)
      continue;
    const auto group = std::dynamic_pointer_cast<Group>(entry.second.object);
    for (const unsigned member : group->objectsIndexes)
      grouped.insert(member);
  }

  vector<unsigned> result;
  // objects are stored front to back; OpenLayout paints later children on top
  for (auto it = spread.items.rbegin(); it != spread.items.rend(); ++it)
  {
    if (grouped.count(it->first))
      continue;
    if (pageIndexOf(spread, *it->second.object) != pageIndex)
      continue;
    result.push_back(it->first);
  }
  return result;
}

void OpenLayoutBuilder::setGeometry(JsonValue &style, const Geometry &geometry) const
{
  style.set("left", len(geometry.left));
  style.set("top", len(geometry.top));
  style.set("width", len(geometry.width));
  style.set("height", len(geometry.height));
}

void OpenLayoutBuilder::setTransform(JsonValue &style, const double rotation, const double skew, const bool hflip, const bool vflip) const
{
  string transform;
  const auto add = [&transform](const string &part)
  {
    if (!transform.empty())
      transform.push_back(' ');
    transform.append(part);
  };
  // QuarkXPress angles are counterclockwise, CSS angles clockwise
  if (!QXP_ALMOST_ZERO(rotation))
    add("rotate(" + formatNumber(-rotation) + "deg)");
  if (!QXP_ALMOST_ZERO(skew))
    add("skewX(" + formatNumber(-skew) + "deg)");
  if (hflip)
    add("scaleX(-1)");
  if (vflip)
    add("scaleY(-1)");
  if (!transform.empty())
    style.set("transform", transform);
}

namespace
{

class FillCss : public boost::static_visitor<void>
{
public:
  FillCss(JsonValue &style, const string &solidProperty, const string &color1, const string &color2)
    : m_style(style), m_solidProperty(solidProperty), m_color1(color1), m_color2(color2)
  { }

  void operator()(const Color &)
  {
    m_style.set(m_solidProperty, m_color1);
  }

  void operator()(const Gradient &gradient)
  {
    // QuarkXPress angles are counterclockwise and start at "left to right", CSS angles
    // are clockwise and start at "to top" (spec 5.6)
    const string angle = formatNumber(90 - gradient.angle) + "deg";
    const char *shape = nullptr;
    switch (gradient.type)
    {
    case GradientType::LINEAR:
      m_style.set("backgroundImage", "linear-gradient(" + angle + ", " + m_color1 + ", " + m_color2 + ")");
      return;
    case GradientType::MIDLINEAR:
      // a mid-linear blend is a plain CSS gradient with three stops
      m_style.set("backgroundImage", "linear-gradient(" + angle + ", " + m_color1 + ", " + m_color2 + ", " + m_color1 + ")");
      return;
    case GradientType::CIRCULAR:
      m_style.set("backgroundImage", "radial-gradient(closest-side, " + m_color2 + ", " + m_color1 + ")");
      return;
    case GradientType::FULLCIRCULAR:
      m_style.set("backgroundImage", "radial-gradient(farthest-corner, " + m_color2 + ", " + m_color1 + ")");
      return;
    case GradientType::RECTANGULAR:
      shape = "rectangular";
      break;
    case GradientType::DIAMOND:
      shape = "diamond";
      break;
    }
    // no CSS gradient for these: the approximation stays as the mandatory fallback
    m_style.set("backgroundImage", "radial-gradient(closest-side, " + m_color2 + ", " + m_color1 + ")");
    JsonValue blend = JsonValue::object();
    blend.set("shape", shape);
    blend.set("angle", angle);
    JsonValue stops = JsonValue::array();
    stops.push(JsonValue(m_color2));
    stops.push(JsonValue(m_color1));
    blend.set("stops", stops);
    m_style.set("blend", blend);
  }

private:
  JsonValue &m_style;
  const string m_solidProperty;
  const string m_color1;
  const string m_color2;
};

}

void OpenLayoutBuilder::setFill(JsonValue &style, const Box &box, const bool svgPaint) const
{
  if (!box.fill)
    return;
  string color1;
  string color2;
  if (const Gradient *const gradient = boost::get<Gradient>(box.fill.get_ptr()))
  {
    color1 = colorValue(gradient->color1Ref, gradient->color1);
    color2 = colorValue(gradient->color2Ref, gradient->color2);
  }
  else if (const Color *const color = boost::get<Color>(box.fill.get_ptr()))
  {
    color1 = colorValue(box.fillColorRef, *color);
  }
  FillCss visitor(style, svgPaint ? "fill" : "backgroundColor", color1, color2);
  boost::apply_visitor(visitor, box.fill.get());
}

/** Returns the id of a strokeStyle resource (spec 3.9), creating it on first use.
  *
  * A solid line needs no resource. QuarkXPress names its "Dashes & Stripes",
  * but libqxp does not read the names, so they are deduplicated and numbered.
  */
string OpenLayoutBuilder::strokeStyleId(const LineStyle &lineStyle)
{
  if (lineStyle.segmentLengths.size() < 2)
    return string();

  JsonValue n = node("strokeStyle");
  JsonValue segments = JsonValue::array();
  for (const double segment : lineStyle.segmentLengths)
    segments.push(JsonValue(std::stod(formatNumber(segment, 4))));
  if (lineStyle.isStripe)
  {
    // shares of the stroke width, from one edge to the other
    n.set("stripes", segments);
  }
  else
  {
    n.set("dashes", segments);
    n.set("patternLength", std::stod(formatNumber(lineStyle.patternLength, 4)));
    n.set("proportional", lineStyle.isProportional);
    switch (lineStyle.endcapType)
    {
    case LineCapType::ROUND:
      n.set("strokeLinecap", "round");
      break;
    case LineCapType::RECT:
      n.set("strokeLinecap", "square");
      break;
    // "stretch" is not a cap in the spec: the pattern is stretched, which
    // proportional + patternLength already say
    case LineCapType::STRETCH:
    case LineCapType::BUTT:
      break;
    }
    switch (lineStyle.joinType)
    {
    case LineJoinType::ROUND:
      n.set("strokeLinejoin", "round");
      break;
    case LineJoinType::BEVEL:
      n.set("strokeLinejoin", "bevel");
      break;
    case LineJoinType::MITER:
      break;
    }
  }

  const string key = n.toString();
  const auto it = m_strokeStyleIds.find(key);
  if (it != m_strokeStyleIds.end())
    return it->second;

  const string id = uniqueId((lineStyle.isStripe ? "stripe-" : "dash-") + std::to_string(m_strokeStyleIds.size() + 1));
  m_strokeStyleIds[key] = id;
  JsonValue withId = node("strokeStyle");
  withId.set("id", id);
  for (const auto &member : n.members())
  {
    if (member.first != "type")
      withId.set(member.first, member.second);
  }
  m_strokeStyleNodes.push(withId);
  return id;
}

void OpenLayoutBuilder::setFrame(JsonValue &style, const Frame &frame, const bool isLine)
{
  if (!frame.color || (!isLine && QXP_ALMOST_ZERO(frame.width)))
    return;

  // every page item is stroked with the SVG vocabulary, frames included (spec 5.8)
  style.set("stroke", colorValue(frame.colorRef, frame.color.get()));
  style.set("strokeWidth", pt(frame.width));

  const LineStyle *const lineStyle = frame.lineStyle;
  const bool patterned = lineStyle && lineStyle->segmentLengths.size() > 1;
  if (patterned)
  {
    const string styleId = strokeStyleId(*lineStyle);
    if (!styleId.empty())
      style.set("strokeStyle", styleId);
  }

  if (!isLine)
  {
    // a box frame sits inside the box
    style.set("strokeAlign", "inside");
  }
  else
  {
    // libqxp knows two arrow shapes: an arrowhead and a feathered tail
    const auto arrowName = [](const Arrow &arrow)
    {
      return arrow.path.compare(0, 4, "m9 5") == 0 ? "tail" : "arrowhead";
    };
    if (frame.startArrow)
      style.set("markerStart", arrowName(*frame.startArrow));
    if (frame.endArrow)
      style.set("markerEnd", arrowName(*frame.endArrow));
  }

  if (patterned && frame.gapColor)
    style.set("strokeGapColor", colorValue(frame.gapColorRef, frame.gapColor.get()));
}

string OpenLayoutBuilder::bezierPath(const vector<CurveComponent> &components, const double originX, const double originY, const bool closable) const
{
  // each anchor point is stored as a triple: control in, anchor, control out
  string d;
  const auto point = [&](const Point &p)
  {
    return formatNumber(conv(p.x - originX)) + " " + formatNumber(conv(p.y - originY));
  };
  for (const auto &component : components)
  {
    const auto &points = component.points;
    if (points.size() < 6)
      continue;
    if (!d.empty())
      d.push_back(' ');
    d.append("M " + point(points[1]));
    for (size_t i = 3; i + 1 < points.size(); i += 3)
      d.append(" C " + point(points[i - 1]) + " " + point(points[i]) + " " + point(points[i + 1]));
    if (closable && points[1] == points[points.size() - 2])
      d.append(" Z");
  }
  return d;
}

string OpenLayoutBuilder::polygonPath(const vector<Point> &points, const double originX, const double originY) const
{
  string d;
  for (size_t i = 0; i < points.size(); ++i)
  {
    d.append(i == 0 ? "M " : " L ");
    d.append(formatNumber(conv(points[i].x - originX)) + " " + formatNumber(conv(points[i].y - originY)));
  }
  if (!d.empty())
    d.append(" Z");
  return d;
}

JsonValue OpenLayoutBuilder::lineItem(const Line &line, const double originX, const double originY)
{
  const TextPath *const textPath = dynamic_cast<const TextPath *>(&line);
  JsonValue style = JsonValue::object();
  JsonValue n;

  if (line.curveComponents.empty())
  {
    const Point center = line.boundingBox.center();
    const Point p1 = line.boundingBox.topLeft().rotateDeg(-line.rotation, center);
    const Point p2 = line.boundingBox.bottomRight().rotateDeg(-line.rotation, center);
    const Rect bounds = boundsOf({p1, p2});

    n = node(textPath ? "textPath" : "line");
    setGeometry(style, Geometry{bounds.left - originX, bounds.top - originY, bounds.width(), bounds.height()});
    if (textPath)
    {
      n.set("d", "M " + formatNumber(conv(p1.x - bounds.left)) + " " + formatNumber(conv(p1.y - bounds.top)) +
            " L " + formatNumber(conv(p2.x - bounds.left)) + " " + formatNumber(conv(p2.y - bounds.top)));
    }
    else
    {
      n.set("x1", len(p1.x - bounds.left));
      n.set("y1", len(p1.y - bounds.top));
      n.set("x2", len(p2.x - bounds.left));
      n.set("y2", len(p2.y - bounds.top));
    }
  }
  else
  {
    n = node(textPath ? "textPath" : "path");
    const Rect &bbox = line.boundingBox;
    setGeometry(style, Geometry{bbox.left - originX, bbox.top - originY, bbox.width(), bbox.height()});
    setTransform(style, line.rotation, 0, false, false);
    n.set("d", bezierPath(line.curveComponents, bbox.left, bbox.top, false));
  }

  setFrame(style, line.style, true);
  if (line.runaround)
    style.set("wrap", "boundingBox");
  n.set("style", style);

  if (textPath)
  {
    const char *alignment = "baseline";
    switch (textPath->settings.alignment)
    {
    case TextPathAlignment::ASCENT:
      alignment = "ascent";
      break;
    case TextPathAlignment::CENTER:
      alignment = "center";
      break;
    case TextPathAlignment::DESCENT:
      alignment = "descent";
      break;
    case TextPathAlignment::BASELINE:
      break;
    }
    const char *lineAlignment = "top";
    switch (textPath->settings.lineAlignment)
    {
    case TextPathLineAlignment::CENTER:
      lineAlignment = "center";
      break;
    case TextPathLineAlignment::BOTTOM:
      lineAlignment = "bottom";
      break;
    case TextPathLineAlignment::TOP:
      break;
    }
    const char *orientation = "none";
    if (textPath->settings.rotate && textPath->settings.skew)
      orientation = "rotate-skew";
    else if (textPath->settings.rotate)
      orientation = "rotate";
    else if (textPath->settings.skew)
      orientation = "skew";

    JsonValue *const pathStyle = n.get("style");
    pathStyle->set("textPathAlign", alignment);
    pathStyle->set("textPathLineAlign", lineAlignment);
    pathStyle->set("textPathOrientation", orientation);
    setText(n, *textPath);
  }

  return n;
}

JsonValue OpenLayoutBuilder::boxItem(const Item &item, const Spread &spread, const double originX, const double originY)
{
  (void) spread;
  const Box &box = dynamic_cast<const Box &>(*item.object);
  const TextBox *const textBox = dynamic_cast<const TextBox *>(&box);
  const PictureBox *const pictureBox = dynamic_cast<const PictureBox *>(&box);
  const Rect &bbox = box.boundingBox;

  JsonValue style = JsonValue::object();
  JsonValue n;

  const bool isFrame = textBox || pictureBox;
  string shapePath;
  switch (box.boxType)
  {
  case BoxType::POLYGON:
    shapePath = polygonPath(box.customPoints, bbox.left, bbox.top);
    break;
  case BoxType::BEZIER:
    shapePath = bezierPath(box.curveComponents, bbox.left, bbox.top, true);
    break;
  case BoxType::UNKNOWN:
  case BoxType::RECTANGLE:
  case BoxType::OVAL:
    break;
  }

  if (textBox)
    n = node("textFrame");
  else if (pictureBox)
    n = node("imageFrame");
  else if (box.boxType == BoxType::OVAL)
    n = node("ellipse");
  else if (!shapePath.empty())
    n = node("path");
  else
    n = node("rect");

  setGeometry(style, Geometry{bbox.left - originX, bbox.top - originY, bbox.width(), bbox.height()});
  setTransform(style, box.rotation, box.skew, box.hflip, box.vflip);

  if (isFrame && box.boxType == BoxType::OVAL)
    style.set("clipPath", "ellipse(50% 50% at 50% 50%)");
  else if (isFrame && !shapePath.empty())
    style.set("clipPath", "path('" + shapePath + "')");
  else if (!shapePath.empty())
    n.set("d", shapePath);

  if (box.boxType == BoxType::RECTANGLE && box.cornerType != CornerType::DEFAULT && !QXP_ALMOST_ZERO(box.cornerRadius))
  {
    style.set("borderRadius", len(box.cornerRadius));
    if (box.cornerType == CornerType::BEVELED)
      style.set("cornerShape", "bevel");
    else if (box.cornerType == CornerType::CONCAVE)
      style.set("cornerShape", "scoop");
  }

  const bool svgPaint = !isFrame && !shapePath.empty();
  setFill(style, box, svgPaint);
  if (svgPaint)
  {
    if (!box.fill)
      style.set("fill", "none");
    setFrame(style, box.frame, true);
  }
  else
  {
    setFrame(style, box.frame, false);
  }

  if (box.runaround)
    style.set("wrap", "boundingBox");

  if (textBox)
  {
    const TextSettings &settings = textBox->settings;
    const Rect &inset = settings.inset;
    if (!QXP_ALMOST_ZERO(inset.top) || !QXP_ALMOST_ZERO(inset.right) || !QXP_ALMOST_ZERO(inset.bottom) || !QXP_ALMOST_ZERO(inset.left))
    {
      if (QXP_ALMOST_ZERO(inset.top - inset.right) && QXP_ALMOST_ZERO(inset.top - inset.bottom) && QXP_ALMOST_ZERO(inset.top - inset.left))
        style.set("padding", len(inset.top));
      else
        style.set("padding", len(inset.top) + " " + len(inset.right) + " " + len(inset.bottom) + " " + len(inset.left));
    }
    if (settings.columnsCount > 1)
    {
      style.set("columnCount", settings.columnsCount);
      style.set("columnGap", len(settings.gutterWidth));
    }
    switch (settings.verticalAlignment)
    {
    case VerticalAlignment::CENTER:
      style.set("alignContent", "center");
      break;
    case VerticalAlignment::BOTTOM:
      style.set("alignContent", "flex-end");
      break;
    case VerticalAlignment::JUSTIFIED:
      style.set("alignContent", "space-between");
      break;
    case VerticalAlignment::TOP:
      break;
    }
    // the text is transformed, the frame stays as it is (spec 5.3);
    // QuarkXPress angles are counterclockwise, CSS angles clockwise
    string contentTransform;
    if (!QXP_ALMOST_ZERO(settings.rotation))
      contentTransform = "rotate(" + formatNumber(-settings.rotation) + "deg)";
    if (!QXP_ALMOST_ZERO(settings.skew))
      contentTransform += string(contentTransform.empty() ? "" : " ") + "skewX(" + formatNumber(-settings.skew) + "deg)";
    if (!contentTransform.empty())
      style.set("contentTransform", contentTransform);
  }

  n.set("style", style);

  if (textBox)
  {
    setText(n, *textBox);
  }
  else if (pictureBox && pictureBox->hasPicture)
  {
    JsonValue img = node("img");
    const string path = pictureBox->filePath.cstr();
    if (!path.empty())
      img.set("src", convertPath(path));
    if (!pictureBox->fileType.empty())
    {
      JsonValue link = JsonValue::object();
      link.set("fileType", pictureBox->fileType);
      img.set("link", link);
    }

    JsonValue imgStyle = JsonValue::object();
    imgStyle.set("left", len(pictureBox->offsetLeft));
    imgStyle.set("top", len(pictureBox->offsetTop));
    string transform;
    if (!QXP_ALMOST_ZERO(pictureBox->scaleHor - 1) || !QXP_ALMOST_ZERO(pictureBox->scaleVert - 1))
      transform = "scale(" + formatNumber(pictureBox->scaleHor, 4) + ", " + formatNumber(pictureBox->scaleVert, 4) + ")";
    if (!QXP_ALMOST_ZERO(pictureBox->pictureRotation))
      transform += string(transform.empty() ? "" : " ") + "rotate(" + formatNumber(-pictureBox->pictureRotation) + "deg)";
    if (!QXP_ALMOST_ZERO(pictureBox->pictureSkew))
      transform += string(transform.empty() ? "" : " ") + "skewX(" + formatNumber(-pictureBox->pictureSkew) + "deg)";
    if (!transform.empty())
    {
      imgStyle.set("transform", transform);
      imgStyle.set("transformOrigin", "0 0");
    }
    img.set("style", imgStyle);

    JsonValue children = JsonValue::array();
    children.push(img);
    n.set("children", children);
  }

  return n;
}

JsonValue OpenLayoutBuilder::groupItem(const Group &group, const Spread &spread, const double originX, const double originY)
{
  JsonValue n = node("group");
  const Rect &bbox = group.boundingBox;
  JsonValue style = JsonValue::object();
  setGeometry(style, Geometry{bbox.left - originX, bbox.top - originY, bbox.width(), bbox.height()});
  n.set("style", style);

  vector<unsigned> members(group.objectsIndexes);
  std::sort(members.begin(), members.end(), std::greater<unsigned>());
  JsonValue children = JsonValue::array();
  for (const unsigned member : members)
  {
    if (spread.items.find(member) == spread.items.end())
      continue;
    children.push(item(spread, member, bbox.left, bbox.top));
  }
  if (!children.empty())
    n.set("children", children);
  return n;
}

JsonValue OpenLayoutBuilder::item(const Spread &spread, const unsigned index, const double originX, const double originY)
{
  const Item &entry = spread.items.at(index);
  switch (entry.kind)
  {
  case ItemKind::LINE:
  case ItemKind::TEXT_PATH:
    return lineItem(dynamic_cast<const Line &>(*entry.object), originX, originY);
  case ItemKind::GROUP:
    return groupItem(dynamic_cast<const Group &>(*entry.object), spread, originX, originY);
  case ItemKind::BOX:
  case ItemKind::TEXT_BOX:
    break;
  }
  return boxItem(entry, spread, originX, originY);
}

JsonValue OpenLayoutBuilder::pageStyle(const PageSettings &settings, const bool leftPage) const
{
  (void) leftPage;
  JsonValue style = JsonValue::object();
  style.set("size", len(settings.offset.width()) + " " + len(settings.offset.height()));
  style.set("marginTop", len(settings.margins.top));
  style.set("marginBottom", len(settings.margins.bottom));
  if (m_facing)
  {
    // QuarkXPress stores the inside margin as left and the outside as right;
    // the spec resolves the logical margins per page side (4.1)
    style.set("marginInside", len(settings.margins.left));
    style.set("marginOutside", len(settings.margins.right));
  }
  else
  {
    style.set("marginLeft", len(settings.margins.left));
    style.set("marginRight", len(settings.margins.right));
  }
  if (settings.columnsCount > 1)
  {
    style.set("columnCount", settings.columnsCount);
    style.set("columnGap", len(settings.gutterWidth));
  }
  return style;
}

void OpenLayoutBuilder::buildMasters(JsonValue &masters)
{
  unsigned masterNumber = 0;
  unsigned userMasterNumber = 0;
  for (const auto &spread : m_spreads)
  {
    if (!spread.page.isMaster)
      continue;
    ++masterNumber;

    // the first two masters are the built-in "blank single page" and "blank facing pages"
    if (masterNumber <= 2 && spread.items.empty() && spread.page.pageSettings.size() == masterNumber)
    {
      m_masterIds.push_back(string());
      continue;
    }

    // default master names are not stored: A-Master A, B-Master B, ...
    string prefix;
    for (unsigned n = userMasterNumber++;; n = n / 26 - 1)
    {
      prefix.insert(prefix.begin(), char('A' + n % 26));
      if (n < 26)
        break;
    }
    string name = spread.page.name.cstr();
    if (name.empty())
      name = prefix + "-Master " + prefix;
    const string id = uniqueId(prefix);
    m_masterIds.push_back(id);

    JsonValue master = node("master");
    master.set("id", id);
    master.set("name", name);
    master.set("qxp:index", masterNumber);

    JsonValue pages = JsonValue::array();
    vector<PageItems> pageItems;
    for (size_t p = 0; p < spread.page.pageSettings.size(); ++p)
    {
      const PageSettings &settings = spread.page.pageSettings[p];
      const bool facing = spread.page.isFacing();
      const bool left = facing && p == 0;

      JsonValue page = node("page");
      if (facing)
        page.set("side", left ? "left" : "right");
      page.set("style", pageStyle(settings, left));

      PageItems items;
      JsonValue children = JsonValue::array();
      unsigned itemNumber = 0;
      for (const unsigned index : topLevelItems(spread, p))
      {
        JsonValue it = item(spread, index, settings.offset.left, settings.offset.top);
        JsonValue withId = JsonValue::object();
        withId.set("type", *it.get("type"));
        withId.set("id", uniqueId(id + (facing ? (left ? "-l-" : "-r-") : "-") + std::to_string(++itemNumber)));
        for (const auto &member : it.members())
        {
          if (member.first != "type")
            withId.set(member.first, member.second);
        }
        children.push(withId);
        items.items.push_back(withId);
      }
      if (!children.empty())
        page.set("children", children);
      pages.push(page);
      items.page = page;
      pageItems.push_back(items);
    }
    master.set("children", pages);
    masters.push(master);
    m_masterPages[id] = pageItems;
  }
}

vector<JsonValue> OpenLayoutBuilder::linkToMaster(const vector<JsonValue> &items, const vector<JsonValue> &masterItems) const
{
  const auto geometryKey = [](const JsonValue &it)
  {
    const JsonValue *const style = it.get("style");
    string key = it.get("type")->asString();
    for (const char *const property : {"left", "top", "width", "height"})
    {
      const JsonValue *const value = style ? style->get(property) : nullptr;
      key += "|" + (value ? value->asString() : string());
    }
    return key;
  };
  const auto withoutId = [](const JsonValue &it)
  {
    JsonValue copy = it;
    copy.erase("id");
    return copy;
  };

  vector<bool> masterUsed(masterItems.size(), false);
  vector<JsonValue> result;
  vector<JsonValue> overrides;

  for (const auto &it : items)
  {
    bool handled = false;
    // an unchanged copy of a master item stays linked to the master
    for (size_t m = 0; m < masterItems.size() && !handled; ++m)
    {
      if (!masterUsed[m] && withoutId(masterItems[m]) == withoutId(it))
      {
        masterUsed[m] = true;
        handled = true;
      }
    }
    // a modified copy becomes an override
    for (size_t m = 0; m < masterItems.size() && !handled; ++m)
    {
      if (masterUsed[m] || geometryKey(masterItems[m]) != geometryKey(it))
        continue;
      masterUsed[m] = true;
      handled = true;

      JsonValue override = node("override");
      override.set("ref", *masterItems[m].get("id"));
      const JsonValue masterItem = withoutId(masterItems[m]);
      const JsonValue pageItem = withoutId(it);
      for (const auto &member : pageItem.members())
      {
        if (member.first == "type")
          continue;
        const JsonValue *const masterValue = masterItem.get(member.first);
        if (member.first == "style" && masterValue)
        {
          const JsonValue diff = diffStyle(member.second, *masterValue);
          if (!diff.empty())
            override.set("style", diff);
        }
        else if (!masterValue || *masterValue != member.second)
        {
          override.set(member.first, member.second);
        }
      }
      if (!pageItem.has("children") && masterItem.has("children"))
        override.set("children", JsonValue::array());
      overrides.push_back(override);
    }
    if (!handled)
      result.push_back(it);
  }

  // master items without a copy on the page were deleted on the page
  for (size_t m = 0; m < masterItems.size(); ++m)
  {
    if (masterUsed[m])
      continue;
    JsonValue override = node("override");
    override.set("ref", *masterItems[m].get("id"));
    JsonValue style = JsonValue::object();
    style.set("display", "none");
    override.set("style", style);
    overrides.push_back(override);
  }

  overrides.insert(overrides.end(), result.begin(), result.end());
  return overrides;
}

vector<JsonValue> OpenLayoutBuilder::detachFromMaster(const vector<JsonValue> &items, const vector<JsonValue> &masterItems) const
{
  // Every master item is hidden and every item of the page is written out, so the page
  // shows exactly what QuarkXPress stored on it. The page keeps its master, and with it
  // the page style.
  vector<JsonValue> result;
  for (const auto &masterItem : masterItems)
  {
    JsonValue override = node("override");
    override.set("ref", *masterItem.get("id"));
    JsonValue style = JsonValue::object();
    style.set("display", "none");
    override.set("style", style);
    result.push_back(override);
  }
  result.insert(result.end(), items.begin(), items.end());
  return result;
}

void OpenLayoutBuilder::buildPages(JsonValue &pages)
{
  unsigned pageNumber = 0;
  for (const auto &spread : m_spreads)
  {
    if (spread.page.isMaster)
      continue;

    const bool facing = spread.page.isFacing();
    vector<JsonValue> pageNodes;
    for (size_t p = 0; p < spread.page.pageSettings.size(); ++p)
    {
      const PageSettings &settings = spread.page.pageSettings[p];
      const bool left = m_facing && (facing ? p == 0 : settings.offset.left < settings.offset.width() / 2);

      JsonValue page = node("page");
      const string pageId = uniqueId("p" + std::to_string(++pageNumber));
      page.set("id", pageId);

      string masterId;
      if (settings.masterIndex > 0 && settings.masterIndex <= m_masterIds.size())
        masterId = m_masterIds[settings.masterIndex - 1];

      const vector<PageItems> *masterPages = nullptr;
      const PageItems *masterPage = nullptr;
      if (!masterId.empty())
      {
        masterPages = &m_masterPages[masterId];
        if (masterPages->size() == 2)
          masterPage = &(*masterPages)[left ? 0 : 1];
        else if (masterPages->size() == 1)
          masterPage = &(*masterPages)[0];
      }

      // the document says which master the page was made from, so OpenLayout says it too
      page.set("master", masterId.empty() ? JsonValue() : JsonValue(masterId));
      if (m_facing && !facing)
        page.set("side", left ? "left" : "right");

      // the page style is inherited from the master; only the differences are written
      const JsonValue style = pageStyle(settings, left);
      if (!masterPage || !isSameStyle(*masterPage->page.get("style"), style))
      {
        const JsonValue *const masterStyle = masterPage ? masterPage->page.get("style") : nullptr;
        page.set("style", masterStyle ? diffStyle(style, *masterStyle) : style);
      }

      vector<JsonValue> items;
      unsigned itemNumber = 0;
      for (const unsigned index : topLevelItems(spread, p))
      {
        JsonValue it = item(spread, index, settings.offset.left, settings.offset.top);
        JsonValue withId = JsonValue::object();
        withId.set("type", *it.get("type"));
        withId.set("id", uniqueId(pageId + "-" + std::to_string(++itemNumber)));
        for (const auto &member : it.members())
        {
          if (member.first != "type")
            withId.set(member.first, member.second);
        }
        items.push_back(withId);
      }

      if (masterPage)
        items = m_options.linkMasters ? linkToMaster(items, masterPage->items) : detachFromMaster(items, masterPage->items);

      if (!items.empty())
      {
        JsonValue children = JsonValue::array();
        for (const auto &it : items)
          children.push(it);
        page.set("children", children);
      }
      pageNodes.push_back(page);
    }

    if (facing)
    {
      JsonValue spreadNode = node("spread");
      JsonValue children = JsonValue::array();
      for (const auto &page : pageNodes)
        children.push(page);
      spreadNode.set("children", children);
      pages.push(spreadNode);
    }
    else
    {
      for (const auto &page : pageNodes)
        pages.push(page);
    }
  }
}

JsonValue OpenLayoutBuilder::head()
{
  JsonValue h = node("head");
  JsonValue children = JsonValue::array();

  const auto meta = [&children](const string &name, const string &content)
  {
    JsonValue m = node("meta");
    m.set("name", name);
    m.set("content", content);
    children.push(m);
  };
  if (!m_options.title.empty())
    meta("title", m_options.title);
  if (!m_options.generator.empty())
    meta("generator", m_options.generator);
  meta("qxp:version", versionName(m_options.version));
  meta("qxp:platform", m_options.bigEndian ? "mac" : "windows");

  JsonValue settings = node("settings");
  settings.set("facingPages", m_facing);
  settings.set("units", m_options.unit == OpenLayoutUnit::MILLIMETER ? "mm" : "pt");
  // unitless numbers in path.d and clipPath: path() use this unit (spec 3.8, 5.5)
  settings.set("geometryUnit", m_options.unit == OpenLayoutUnit::MILLIMETER ? "mm" : "pt");

  JsonValue typography = JsonValue::object();
  // autoLeading is the meaning of lineHeight: "normal": QuarkXPress stores the increment
  typography.set("autoLeading", m_docProps.isIncrementalAutoLeading()
                 ? JsonValue("calc(1em + " + pt(m_docProps.autoLeading()) + ")")
                 : JsonValue(percent(1 + m_docProps.autoLeading())));
  const auto scale = [](const double horizontal, const double vertical)
  {
    return percent(horizontal) + " " + percent(vertical);
  };
  JsonValue superscript = JsonValue::object();
  superscript.set("offset", percent(m_docProps.superscriptOffset));
  superscript.set("scale", scale(m_docProps.superscriptHScale, m_docProps.superscriptVScale));
  typography.set("superscript", superscript);
  JsonValue subscript = JsonValue::object();
  // a subscript is shifted down: the spec counts a positive offset upwards
  subscript.set("offset", percent(m_docProps.subscriptOffset));
  subscript.set("scale", scale(m_docProps.subscriptHScale, m_docProps.subscriptVScale));
  typography.set("subscript", subscript);
  JsonValue superior = JsonValue::object();
  superior.set("scale", scale(m_docProps.superiorHScale, m_docProps.superiorVScale));
  typography.set("superior", superior);
  settings.set("typography", typography);
  children.push(settings);

  for (const auto &family : m_fontFamilies)
  {
    JsonValue font = node("font");
    font.set("id", m_fontIds[family]);
    font.set("fontFamily", family);
    font.set("fontWeight", 400);
    font.set("fontStyle", "normal");
    children.push(font);
  }

  for (const auto &color : m_colorNodes.items())
    children.push(color);
  for (const auto &strokeStyle : m_strokeStyleNodes.items())
    children.push(strokeStyle);
  for (const auto &hj : m_hjNodes.items())
    children.push(hj);
  for (const auto &style : m_styleNodes.items())
    children.push(style);

  h.set("children", children);
  return h;
}

JsonValue OpenLayoutBuilder::build()
{
  buildColors();
  buildStyles();
  collectChains();

  JsonValue masters = node("masters");
  JsonValue masterList = JsonValue::array();
  buildMasters(masterList);
  masters.set("children", masterList);

  JsonValue pages = node("pages");
  JsonValue pageList = JsonValue::array();
  buildPages(pageList);
  pages.set("children", pageList);

  JsonValue stories = node("stories");
  stories.set("children", m_stories);

  JsonValue document = node("document");
  document.set("version", "0.1");
  JsonValue children = JsonValue::array();
  children.push(head());
  children.push(masters);
  children.push(pages);
  children.push(stories);
  document.set("children", children);
  return document;
}

OpenLayoutCollector::OpenLayoutCollector(const OpenLayoutOptions &options)
  : m_options(options)
  , m_docProps()
  , m_colors()
  , m_paragraphStyles()
  , m_characterStyles()
  , m_spreads()
  , m_texts()
  , m_currentObjectIndex(0)
{
}

bool OpenLayoutCollector::collectsMasterPages() const
{
  return true;
}

void OpenLayoutCollector::startPage(const Page &page)
{
  m_spreads.push_back(Spread(page));
  m_currentObjectIndex = 0;
}

void OpenLayoutCollector::collectDocumentProperties(const QXPDocumentProperties &props)
{
  m_docProps = props;
}

void OpenLayoutCollector::collectColor(const ColorSpec &color)
{
  m_colors.push_back(color);
}

void OpenLayoutCollector::collectParagraphStyle(const ParagraphStyle &style)
{
  m_paragraphStyles.push_back(style);
}

void OpenLayoutCollector::collectCharacterStyle(const CharacterStyle &style)
{
  m_characterStyles.push_back(style);
}

void OpenLayoutCollector::addItem(const ItemKind kind, const std::shared_ptr<Object> &object)
{
  if (m_spreads.empty())
    return;
  m_spreads.back().items[m_currentObjectIndex] = Item(kind, object);
  ++m_currentObjectIndex;
}

void OpenLayoutCollector::collectLine(const std::shared_ptr<Line> &line)
{
  addItem(ItemKind::LINE, line);
}

void OpenLayoutCollector::collectBox(const std::shared_ptr<Box> &box)
{
  addItem(ItemKind::BOX, box);
}

void OpenLayoutCollector::collectTextBox(const std::shared_ptr<TextBox> &box)
{
  addItem(ItemKind::TEXT_BOX, box);
}

void OpenLayoutCollector::collectTextPath(const std::shared_ptr<TextPath> &textPath)
{
  addItem(ItemKind::TEXT_PATH, textPath);
}

void OpenLayoutCollector::collectGroup(const std::shared_ptr<Group> &group)
{
  addItem(ItemKind::GROUP, group);
}

void OpenLayoutCollector::collectText(const std::shared_ptr<Text> &text, const unsigned linkId)
{
  m_texts[linkId] = text;
}

JsonValue OpenLayoutCollector::createDocument() const
{
  OpenLayoutBuilder builder(m_options, m_docProps, m_colors, m_paragraphStyles, m_characterStyles, m_spreads, m_texts);
  return builder.build();
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
