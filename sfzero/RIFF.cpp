/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "RIFF.h"
#include "SFZSafeCast.h"
#include <span>

bool sfzero::RIFFChunk::readFrom(juce::InputStream *file)
{
  // span view over the fourcc so the read target is bounds-described rather
  // than relying on array-to-pointer decay.
  const std::span<char> idBytes{id};
  const int idLen = narrowCast<int>(idBytes.size());

  const auto fail = [&](juce::int64 headerStart) {
    // Truncated header: leave a well-defined (empty) chunk so a stale id from
    // a previous read can't be mistaken for a real chunk.
    for (auto &b : idBytes)
    {
      b = 0;
    }
    size = 0;
    type = Custom;
    start = headerStart;
    return false;
  };

  const juce::int64 headerStart = file->getPosition();
  const int idRead = file->read(idBytes.data(), idLen);

  // The RIFF size field is always 4 bytes little-endian ON DISK. Do not derive
  // this width from sizeof(dword): dword is `unsigned long`, which is 4 bytes
  // on Windows but 8 on Android/iOS/macOS (LP64) - a sizeof-based check there
  // rejected every SoundFont, bundled included.
  constexpr int kRiffSizeFieldBytes = 4;
  char sizeBytes[kRiffSizeFieldBytes] = {};
  const int sizeRead = file->read(sizeBytes, kRiffSizeFieldBytes);

  if (idRead != idLen || sizeRead != kRiffSizeFieldBytes)
  {
    return fail(headerStart);
  }

  size = sfzero::narrowCast<sfzero::dword>(juce::ByteOrder::littleEndianInt(sizeBytes));
  start = file->getPosition();

  if (FourCCEquals(id, "RIFF") || FourCCEquals(id, "LIST"))
  {
    type = FourCCEquals(id, "RIFF") ? RIFF : LIST;
    if (file->read(idBytes.data(), idLen) != idLen || size < sizeof(sfzero::fourcc))
    {
      return fail(headerStart);
    }
    start += sizeof(sfzero::fourcc);
    size -= sizeof(sfzero::fourcc);
  }
  else
  {
    type = Custom;
  }
  return true;
}

void sfzero::RIFFChunk::seek(juce::InputStream *file) { file->setPosition(start); }
void sfzero::RIFFChunk::seekAfter(juce::InputStream *file)
{
  juce::int64 next = start + size;

  if (next % 2 != 0)
  {
    next += 1;
  }
  file->setPosition(next);
}

juce::String sfzero::RIFFChunk::readString(juce::InputStream *file)
{
  // Bound the read so a malformed chunk header can't cause a multi-GB
  // allocation. SF2 metadata strings (INAM, IPRD, etc.) are bounded by
  // the spec to 256 bytes; 1 MB is two orders of magnitude over.
  constexpr sfzero::dword kMaxChunkStringSize = 1u * 1024u * 1024u;
  if (size > kMaxChunkStringSize)
  {
    DBG("RIFFChunk::readString: chunk size " << static_cast<juce::int64>(size)
        << " exceeds cap " << static_cast<juce::int64>(kMaxChunkStringSize) << "; truncating to empty string");
    return {};
  }

  juce::MemoryBlock memoryBlock(size);
  file->read(memoryBlock.getData(), sfzero::narrowCast<int>(memoryBlock.getSize()));
  return memoryBlock.toString();
}
