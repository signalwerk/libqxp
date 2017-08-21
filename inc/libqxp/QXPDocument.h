/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_LIBQXP_QXPDOCUMENT_H
#define INCLUDED_LIBQXP_QXPDOCUMENT_H

#include <librevenge/librevenge.h>
#include <librevenge-stream/librevenge-stream.h>

#ifdef DLL_EXPORT
#ifdef LIBQXP_BUILD
#define QXPAPI __declspec(dllexport)
#else
#define QXPAPI __declspec(dllimport)
#endif
#else // !DLL_EXPORT
#ifdef LIBQXP_VISIBILITY
#define QXPAPI __attribute__((visibility("default")))
#else
#define QXPAPI
#endif
#endif

namespace libqxp
{

class QXPDocument
{
public:
  /** Result of parsing the file.
    */
  enum Result
  {
    RESULT_OK, //< parsed without any problem
    RESULT_FILE_ACCESS_ERROR, //< problem when accessing the file
    RESULT_PARSE_ERROR, //< problem when parsing the file
    RESULT_UNSUPPORTED_FORMAT, //< unsupported file format
    RESULT_UNKNOWN_ERROR //< an unspecified error
  };

  /** Type of document.
    */
  enum Type
  {
    TYPE_UNKNOWN, //< unrecognized file

    TYPE_DOCUMENT,
    TYPE_TEMPLATE,
    TYPE_BOOK,
    TYPE_LIBRARY
  };

  static QXPAPI bool isSupported(librevenge::RVNGInputStream *input, Type *type = 0);
  static QXPAPI Result parse(librevenge::RVNGInputStream *input, librevenge::RVNGDrawingInterface *document);
};

} // namespace libqxp

#endif // INCLUDED_LIBQXP_QXPDOCUMENT_H

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
