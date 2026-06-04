/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "RIFF.h"
#include "SFZSafeCast.h"
#include <span>

void sfzero::RIFFChunk::readFrom(juce::InputStream *file)
{
  // span view over the fourcc so the read target is bounds-described rather
  // than relying on array-to-pointer decay.
  const std::span<char> idBytes{id};
  file->read(idBytes.data(), narrowCast<int>(idBytes.size()));
  size = sfzero::narrowCast<sfzero::dword>(file->readInt());
  start = file->getPosition();

  if (FourCCEquals(id, "RIFF"))
  {
    type = RIFF;
    file->read(idBytes.data(), narrowCast<int>(idBytes.size()));
    start += sizeof(sfzero::fourcc);
    size -= sizeof(sfzero::fourcc);
  }
  else if (FourCCEquals(id, "LIST"))
  {
    type = LIST;
    file->read(idBytes.data(), narrowCast<int>(idBytes.size()));
    start += sizeof(sfzero::fourcc);
    size -= sizeof(sfzero::fourcc);
  }
  else
  {
    type = Custom;
  }
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
