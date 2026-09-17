/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef JSONVALUE_H_INCLUDED
#define JSONVALUE_H_INCLUDED

#include <string>
#include <utility>
#include <vector>

namespace openlayout
{

/** Minimal JSON value. Objects keep the insertion order of their members.
  */
class JsonValue
{
public:
  enum class Type
  {
    NUL,
    BOOLEAN,
    NUMBER,
    STRING,
    ARRAY,
    OBJECT
  };

  typedef std::pair<std::string, JsonValue> Member;

  JsonValue();
  JsonValue(bool value);
  JsonValue(int value);
  JsonValue(unsigned value);
  JsonValue(double value);
  JsonValue(const char *value);
  JsonValue(const std::string &value);

  static JsonValue array();
  static JsonValue object();

  Type type() const;
  bool isNull() const;
  bool isObject() const;
  bool isArray() const;
  const std::string &asString() const;

  /// Sets an object member, replacing an existing member of the same name.
  JsonValue &set(const std::string &key, const JsonValue &value);
  const JsonValue *get(const std::string &key) const;
  JsonValue *get(const std::string &key);
  bool has(const std::string &key) const;
  void erase(const std::string &key);
  const std::vector<Member> &members() const;

  /// Appends an array item.
  JsonValue &push(const JsonValue &value);
  const std::vector<JsonValue> &items() const;
  std::vector<JsonValue> &items();

  bool empty() const;
  size_t size() const;

  /** Serializes the value.
    *
    * @param pretty if true, containers are indented; short containers stay on one line.
    */
  std::string toString(bool pretty = false) const;

  bool operator==(const JsonValue &other) const;
  bool operator!=(const JsonValue &other) const;

private:
  void write(std::string &out, bool pretty, unsigned indent) const;
  void writeInline(std::string &out) const;

  Type m_type;
  bool m_bool;
  double m_number;
  std::string m_string;
  std::vector<JsonValue> m_items;
  std::vector<Member> m_members;
};

}

#endif // JSONVALUE_H_INCLUDED

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
