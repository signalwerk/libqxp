/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libqxp project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QXPBlockParser.h"

#include <librevenge-stream/librevenge-stream.h>
#include <algorithm>
#include <memory>
#include <vector>

#include "QXPHeader.h"

namespace libqxp
{

using librevenge::RVNGInputStream;
using librevenge::RVNGStringStream;
using std::make_shared;
using std::vector;

QXPBlockParser::QXPBlockParser(const std::shared_ptr<RVNGInputStream> &input, const std::shared_ptr<QXPHeader> &header)
  : m_input(input)
  , be(header->isBigEndian())
  , m_blockLength(256)
{
}

std::shared_ptr<RVNGInputStream> QXPBlockParser::getBlock(const uint32_t index)
{
  seek(m_input, (index - 1) * m_blockLength);
  auto block = readNBytes(m_input, m_blockLength);
  return make_shared<RVNGStringStream>(block, m_blockLength);
}

std::shared_ptr<RVNGInputStream> QXPBlockParser::getChain(const uint32_t index)
{
  vector<unsigned char> chain;
  bool isBig = false;
  uint32_t next = index;
  while (next > 0)
  {
    seek(m_input, (next - 1) * m_blockLength);
    const uint16_t count = isBig ? readU16(m_input, be) : 1;

    uint32_t len = (next - 1 + count) * m_blockLength - 4 - m_input->tell();
    auto block = readNBytes(m_input, len);
    std::copy(block, block + len, std::back_inserter(chain));

    const int32_t nextVal = readS32(m_input, be);
    isBig = nextVal < 0;
    next = abs(nextVal);
  }
  return make_shared<RVNGStringStream>(chain.data(), chain.size());
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
