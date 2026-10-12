/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "JsonValue.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace openlayout
{

namespace
{

// containers whose compact form is not longer than this stay on one line
const size_t MAX_INLINE_LENGTH = 120;

void writeString(std::string &out, const std::string &str)
{
  out.push_back('"');
  for (const char c : str)
  {
    switch (c)
    {
    case '"':
      out.append("\\\"");
      break;
    case '\\':
      out.append("\\\\");
      break;
    case '\n':
      out.append("\\n");
      break;
    case '\r':
      out.append("\\r");
      break;
    case '\t':
      out.append("\\t");
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20)
      {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
        out.append(buf);
      }
      else
      {
        out.push_back(c);
      }
    }
  }
  out.push_back('"');
}

void writeNumber(std::string &out, const double value)
{
  if (!std::isfinite(value))
  {
    out.append("null");
    return;
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.10g", value);
  out.append(buf);
}

}

JsonValue::JsonValue()
  : m_type(Type::NUL), m_bool(false), m_number(0), m_string(), m_items(), m_members()
{
}

JsonValue::JsonValue(const bool value)
  : m_type(Type::BOOLEAN), m_bool(value), m_number(0), m_string(), m_items(), m_members()
{
}

JsonValue::JsonValue(const int value)
  : m_type(Type::NUMBER), m_bool(false), m_number(value), m_string(), m_items(), m_members()
{
}

JsonValue::JsonValue(const unsigned value)
  : m_type(Type::NUMBER), m_bool(false), m_number(value), m_string(), m_items(), m_members()
{
}

JsonValue::JsonValue(const double value)
  : m_type(Type::NUMBER), m_bool(false), m_number(value), m_string(), m_items(), m_members()
{
}

JsonValue::JsonValue(const char *const value)
  : m_type(Type::STRING), m_bool(false), m_number(0), m_string(value ? value : ""), m_items(), m_members()
{
}

JsonValue::JsonValue(const std::string &value)
  : m_type(Type::STRING), m_bool(false), m_number(0), m_string(value), m_items(), m_members()
{
}

JsonValue JsonValue::array()
{
  JsonValue value;
  value.m_type = Type::ARRAY;
  return value;
}

JsonValue JsonValue::object()
{
  JsonValue value;
  value.m_type = Type::OBJECT;
  return value;
}

JsonValue::Type JsonValue::type() const
{
  return m_type;
}

bool JsonValue::isNull() const
{
  return m_type == Type::NUL;
}

bool JsonValue::isObject() const
{
  return m_type == Type::OBJECT;
}

bool JsonValue::isArray() const
{
  return m_type == Type::ARRAY;
}

const std::string &JsonValue::asString() const
{
  return m_string;
}

JsonValue &JsonValue::set(const std::string &key, const JsonValue &value)
{
  m_type = Type::OBJECT;
  for (auto &member : m_members)
  {
    if (member.first == key)
    {
      member.second = value;
      return member.second;
    }
  }
  // A node's content should read after its metadata and formatting, even when
  // children were attached before later attributes were discovered.
  if (key != "children")
  {
    const auto children = std::find_if(m_members.begin(), m_members.end(), [](const Member &member)
    {
      return member.first == "children";
    });
    if (children != m_members.end())
      return m_members.insert(children, Member(key, value))->second;
  }
  m_members.push_back(Member(key, value));
  return m_members.back().second;
}

const JsonValue *JsonValue::get(const std::string &key) const
{
  for (const auto &member : m_members)
  {
    if (member.first == key)
      return &member.second;
  }
  return nullptr;
}

JsonValue *JsonValue::get(const std::string &key)
{
  for (auto &member : m_members)
  {
    if (member.first == key)
      return &member.second;
  }
  return nullptr;
}

bool JsonValue::has(const std::string &key) const
{
  return get(key) != nullptr;
}

void JsonValue::erase(const std::string &key)
{
  m_members.erase(std::remove_if(m_members.begin(), m_members.end(), [&key](const Member &member)
  {
    return member.first == key;
  }), m_members.end());
}

const std::vector<JsonValue::Member> &JsonValue::members() const
{
  return m_members;
}

JsonValue &JsonValue::push(const JsonValue &value)
{
  m_type = Type::ARRAY;
  m_items.push_back(value);
  return m_items.back();
}

const std::vector<JsonValue> &JsonValue::items() const
{
  return m_items;
}

std::vector<JsonValue> &JsonValue::items()
{
  return m_items;
}

bool JsonValue::empty() const
{
  return m_items.empty() && m_members.empty();
}

size_t JsonValue::size() const
{
  return m_type == Type::OBJECT ? m_members.size() : m_items.size();
}

std::string JsonValue::toString(const bool pretty) const
{
  std::string out;
  write(out, pretty, 0);
  return out;
}

bool JsonValue::operator==(const JsonValue &other) const
{
  if (m_type != other.m_type)
    return false;
  switch (m_type)
  {
  case Type::NUL:
    return true;
  case Type::BOOLEAN:
    return m_bool == other.m_bool;
  case Type::NUMBER:
    return m_number == other.m_number;
  case Type::STRING:
    return m_string == other.m_string;
  case Type::ARRAY:
    return m_items == other.m_items;
  case Type::OBJECT:
    return m_members == other.m_members;
  }
  return false;
}

bool JsonValue::operator!=(const JsonValue &other) const
{
  return !(*this == other);
}

void JsonValue::writeInline(std::string &out) const
{
  if (m_type != Type::ARRAY && m_type != Type::OBJECT)
  {
    write(out, false, 0);
    return;
  }

  const bool isObj = m_type == Type::OBJECT;
  if (empty())
  {
    out.append(isObj ? "{}" : "[]");
    return;
  }

  out.append(isObj ? "{ " : "[ ");
  const size_t count = size();
  for (size_t i = 0; i < count; ++i)
  {
    if (i > 0)
      out.append(", ");
    if (isObj)
    {
      writeString(out, m_members[i].first);
      out.append(": ");
      m_members[i].second.writeInline(out);
    }
    else
    {
      m_items[i].writeInline(out);
    }
  }
  out.append(isObj ? " }" : " ]");
}

void JsonValue::write(std::string &out, const bool pretty, const unsigned indent) const
{
  switch (m_type)
  {
  case Type::NUL:
    out.append("null");
    return;
  case Type::BOOLEAN:
    out.append(m_bool ? "true" : "false");
    return;
  case Type::NUMBER:
    writeNumber(out, m_number);
    return;
  case Type::STRING:
    writeString(out, m_string);
    return;
  case Type::ARRAY:
  case Type::OBJECT:
    break;
  }

  const bool isObj = m_type == Type::OBJECT;
  const char open = isObj ? '{' : '[';
  const char close = isObj ? '}' : ']';

  if (empty())
  {
    out.push_back(open);
    out.push_back(close);
    return;
  }

  if (pretty)
  {
    std::string compact;
    write(compact, false, 0);
    if (compact.size() <= MAX_INLINE_LENGTH)
    {
      // keep short containers readable on one line
      writeInline(out);
      return;
    }
  }

  out.push_back(open);
  const std::string childIndent(pretty ? indent + 2 : 0, ' ');
  const size_t count = size();
  for (size_t i = 0; i < count; ++i)
  {
    if (i > 0)
      out.push_back(',');
    if (pretty)
    {
      out.push_back('\n');
      out.append(childIndent);
    }
    if (isObj)
    {
      writeString(out, m_members[i].first);
      out.append(pretty ? ": " : ":");
      m_members[i].second.write(out, pretty, indent + 2);
    }
    else
    {
      m_items[i].write(out, pretty, indent + 2);
    }
  }
  if (pretty)
  {
    out.push_back('\n');
    out.append(std::string(indent, ' '));
  }
  out.push_back(close);
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
