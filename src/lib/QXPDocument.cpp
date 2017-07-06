/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <algorithm>
#include <cstring>
#include <memory>

#include <librevenge-stream/librevenge-stream.h>

#include <libqxp/libqxp.h>

#include "libqxp_utils.h"
#include "QXPHeader.h"
#include "QXP33Parser.h"
#include "QXP4Parser.h"

using librevenge::RVNGInputStream;

using std::equal;
using std::shared_ptr;

namespace libqxp
{

namespace
{

QXPDocument::Confidence checkSupport(librevenge::RVNGInputStream *const input, QXPHeader &header) try
{
  if (header.load(std::shared_ptr<RVNGInputStream>(input, QXPDummyDeleter())) && header.isSupported())
  {
    return QXPDocument::CONFIDENCE_EXCELLENT;
  }

  return QXPDocument::CONFIDENCE_NONE;
}
catch (...)
{
  return QXPDocument::CONFIDENCE_NONE;
}

}

QXPAPI QXPDocument::Confidence QXPDocument::isSupported(librevenge::RVNGInputStream *const input)
{
  QXPHeader header;
  return checkSupport(input, header);
}

QXPAPI QXPDocument::Result QXPDocument::parse(librevenge::RVNGInputStream *const input, librevenge::RVNGDrawingInterface *const document) try
{
  QXPHeader header;
  const Confidence confidence = checkSupport(input, header);
  if (CONFIDENCE_NONE == confidence)
    return RESULT_UNSUPPORTED_FORMAT;
  else if (CONFIDENCE_SUPPORTED_PART == confidence)
    return RESULT_UNSUPPORTED_FORMAT;
  else if (CONFIDENCE_UNSUPPORTED_ENCRYPTION == confidence)
    return RESULT_UNSUPPORTED_ENCRYPTION;

  const std::shared_ptr<RVNGInputStream> input_(input, QXPDummyDeleter());

  input_->seek(0, librevenge::RVNG_SEEK_SET);

  switch (header.version())
  {
  case QXPVersion::QXP_33:
    return QXP33Parser(input_, document, header).parse() ? RESULT_OK : RESULT_UNKNOWN_ERROR;
  case QXPVersion::QXP_4:
    return QXP4Parser(input_, document, header).parse() ? RESULT_OK : RESULT_UNKNOWN_ERROR;
  default:
    return RESULT_UNSUPPORTED_FORMAT;
  }

  return RESULT_UNKNOWN_ERROR;
}
catch (const FileAccessError &)
{
  return RESULT_FILE_ACCESS_ERROR;
}
catch (const UnsupportedEncryption &)
{
  return RESULT_UNSUPPORTED_ENCRYPTION;
}
catch (const UnsupportedFormat &)
{
  return RESULT_UNSUPPORTED_FORMAT;
}
catch (...)
{
  return RESULT_UNKNOWN_ERROR;
}

} // namespace libqxp

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
