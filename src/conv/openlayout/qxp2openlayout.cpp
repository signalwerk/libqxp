/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <librevenge-stream/librevenge-stream.h>

#include <libqxp/libqxp.h>

#include "QXPDetector.h"
#include "QXPHeader.h"
#include "QXPParser.h"
#include "libqxp_utils.h"

#include "OpenLayoutCollector.h"

namespace
{

int printUsage()
{
  std::printf("`qxp2openlayout' converts QuarkXPress documents to OpenLayout JSON.\n");
  std::printf("\n");
  std::printf("Usage: qxp2openlayout [OPTION] FILE\n");
  std::printf("\n");
  std::printf("Options:\n");
  std::printf("\t-o, --output FILE     write the document to FILE instead of standard output\n");
  std::printf("\t--unit pt|mm          unit of geometry, default pt (type sizes and strokes are always pt);\n");
  std::printf("\t                      unitless path coordinates always use CSS pixels\n");
  std::printf("\t--compact             write JSON without indentation\n");
  std::printf("\t--no-master-links     do not inherit master items: hide them and keep every item\n");
  std::printf("\t                      on its page (the page still uses its master)\n");
  std::printf("\t--help                show this help message\n");
  std::printf("\t--version             print version and exit\n");
  std::printf("\n");
  std::printf("Exit status: 0 on success, 1 on error, 2 if the document was only partially converted.\n");
  std::printf("\n");
  std::printf("Report bugs to <http://bugs.documentfoundation.org/>.\n");
  return -1;
}

int printVersion()
{
  std::printf("qxp2openlayout " VERSION "\n");
  return 0;
}

std::string titleOf(const std::string &path)
{
  std::string name = path;
  const auto slash = name.find_last_of("/\\");
  if (slash != std::string::npos)
    name.erase(0, slash + 1);
  const auto dot = name.find_last_of('.');
  if (dot != std::string::npos && dot > 0)
    name.erase(dot);
  return name;
}

} // anonymous namespace

using libqxp::QXPDocument;

int main(int argc, char *argv[])
{
  if (argc < 2)
    return printUsage();

  const char *file = nullptr;
  const char *output = nullptr;
  bool pretty = true;
  libqxp::OpenLayoutOptions options;

  for (int i = 1; i < argc; i++)
  {
    if (!std::strcmp(argv[i], "--version"))
      return printVersion();
    else if (!std::strcmp(argv[i], "--help"))
      return printUsage();
    else if ((!std::strcmp(argv[i], "-o") || !std::strcmp(argv[i], "--output")) && i + 1 < argc)
      output = argv[++i];
    else if (!std::strcmp(argv[i], "--unit") && i + 1 < argc)
    {
      ++i;
      if (!std::strcmp(argv[i], "pt"))
        options.unit = libqxp::OpenLayoutUnit::POINT;
      else if (!std::strcmp(argv[i], "mm"))
        options.unit = libqxp::OpenLayoutUnit::MILLIMETER;
      else
        return printUsage();
    }
    else if (!std::strcmp(argv[i], "--compact"))
      pretty = false;
    else if (!std::strcmp(argv[i], "--no-master-links"))
      options.linkMasters = false;
    else if (!file && std::strncmp(argv[i], "-", 1))
      file = argv[i];
    else
      return printUsage();
  }

  if (!file)
    return printUsage();

  librevenge::RVNGFileStream input(file);
  const std::shared_ptr<librevenge::RVNGInputStream> stream(&input, libqxp::QXPDummyDeleter());

  libqxp::QXPDetector detector;
  try
  {
    detector.detect(stream);
  }
  catch (...)
  {
  }

  if (!detector.isSupported() || (detector.type() != QXPDocument::TYPE_DOCUMENT && detector.type() != QXPDocument::TYPE_TEMPLATE))
  {
    std::cerr << "ERROR: Unsupported file format" << std::endl;
    return 1;
  }

  const auto &header = detector.header();
  options.title = titleOf(file);
  options.generator = "qxp2openlayout " VERSION;
  options.version = header->version();
  options.bigEndian = header->isBigEndian();

  libqxp::OpenLayoutCollector collector(options);
  // the parser owns data referenced by the collected objects (e.g. line styles), keep it alive
  std::unique_ptr<libqxp::QXPParser> parser;
  bool complete = false;
  try
  {
    parser = header->createParser(detector.input(), nullptr);
    complete = parser->parse(collector);
  }
  catch (...)
  {
    complete = false;
  }

  const std::string json = collector.createDocument().toString(pretty) + "\n";

  if (output)
  {
    std::ofstream out(output, std::ios::binary);
    out << json;
    if (!out)
    {
      std::cerr << "ERROR: Cannot write " << output << std::endl;
      return 1;
    }
  }
  else
  {
    std::cout << json;
  }

  if (!complete)
  {
    std::cerr << "WARNING: The document could not be parsed completely, the output is partial." << std::endl;
    return 2;
  }

  return 0;
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
