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

using librevenge::RVNGInputStream;

using std::equal;
using std::shared_ptr;

namespace libqxp
{

QXPAPI QXPDocument::Confidence QXPDocument::isSupported(librevenge::RVNGInputStream *const input, Type *const type) try
{
  if (type)
    *type = TYPE_UNKNOWN;

  QXPHeader header;
  if (header.load(std::shared_ptr<RVNGInputStream>(input, QXPDummyDeleter())) && header.isSupported())
  {
    return CONFIDENCE_EXCELLENT;
  }

  return CONFIDENCE_NONE;
}
catch (...)
{
  return CONFIDENCE_NONE;
}

QXPAPI QXPDocument::Result QXPDocument::parse(librevenge::RVNGInputStream *const input, librevenge::RVNGDrawingInterface *const document, const char *const)
{
  Type type;
  Confidence confidence = isSupported(input, &type);
  if (CONFIDENCE_NONE == confidence)
    return RESULT_UNSUPPORTED_FORMAT;
  else if (CONFIDENCE_SUPPORTED_PART == confidence)
    return RESULT_UNSUPPORTED_FORMAT;
  else if (CONFIDENCE_UNSUPPORTED_ENCRYPTION == confidence)
    return RESULT_UNSUPPORTED_ENCRYPTION;

  return parse(input, document, type);
}

QXPAPI QXPDocument::Result QXPDocument::parse(librevenge::RVNGInputStream *const input, librevenge::RVNGDrawingInterface *const document, const QXPDocument::Type type, const char *const) try
{
  // sanity check
  if (QXPDocument::TYPE_UNKNOWN == type)
    return QXPDocument::RESULT_UNSUPPORTED_FORMAT;
  if (QXPDocument::TYPE_RESERVED1 <= type)
    return QXPDocument::RESULT_UNSUPPORTED_FORMAT;

  const std::shared_ptr<RVNGInputStream> input_(input, QXPDummyDeleter());

  input_->seek(0, librevenge::RVNG_SEEK_SET);

  return RESULT_UNKNOWN_ERROR;
}
catch (const FileAccessError &)
{
  return RESULT_FILE_ACCESS_ERROR;
}
catch (const PackageError &)
{
  return RESULT_PACKAGE_ERROR;
}
catch (const PasswordMismatch &)
{
  return RESULT_PASSWORD_MISMATCH;
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
