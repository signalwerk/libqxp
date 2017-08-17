/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXPDetector.h"

#include "QXP33Header.h"
#include "QXP3HeaderBase.h"
#include "QXP4Header.h"
#include "QXPParser.h"

namespace libqxp
{

namespace
{

class QXP3Detector : public QXP3HeaderBase
{
public:
  bool isSupported()
  {
    return m_signature == "XPR" && (version() == QXP_33 || version() == QXP_4);
  }

  std::shared_ptr<QXPHeader> createHeader(const boost::optional<QXPDocument::Type> type = boost::none)
  {
    switch (m_version)
    {
    case QXPVersion::QXP_33:
      return std::make_shared<QXP33Header>(type);
    case QXPVersion::QXP_4:
      return std::make_shared<QXP4Header>(type);
    default:
      return nullptr;
    }
  }

private:
  QXPDocument::Type getType() const override
  {
    return QXPDocument::TYPE_UNKNOWN;
  }

  std::unique_ptr<QXPParser> createParser(const std::shared_ptr<librevenge::RVNGInputStream> &, librevenge::RVNGDrawingInterface *) override
  {
    return nullptr;
  }
};

}

QXPDetector::QXPDetector()
  : m_input()
  , m_header()
  , m_confidence(QXPDocument::CONFIDENCE_NONE)
  , m_type(QXPDocument::TYPE_UNKNOWN)
{
}

void QXPDetector::detect(const std::shared_ptr<librevenge::RVNGInputStream> &input)
{
  QXP3Detector detector;
  if (detector.load(input) && detector.isSupported())
  {
    m_input = input;
    m_header = detector.createHeader();
    if (bool(m_header))
    {
      m_input->seek(0, librevenge::RVNG_SEEK_SET);
      m_header->load(m_input);
      m_type = m_header->getType();
      if (m_type != QXPDocument::TYPE_UNKNOWN)
        m_confidence = QXPDocument::CONFIDENCE_EXCELLENT;
    }
  }
}

const std::shared_ptr<librevenge::RVNGInputStream> &QXPDetector::input() const
{
  return m_input;
}

const std::shared_ptr<QXPHeader> &QXPDetector::header() const
{
  return m_header;
}

const QXPDocument::Confidence &QXPDetector::confidence() const
{
  return m_confidence;
}

const QXPDocument::Type &QXPDetector::type() const
{
  return m_type;
}

} // namespace libqxp

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
