/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "RIFF.h"

void sfzero::RIFFChunk::readFrom(juce::InputStream *file)
{
  file->read(&id, sizeof(sfzero::fourcc));
  size = static_cast<sfzero::dword>(file->readInt());
  start = file->getPosition();

  if (FourCCEquals(id, "RIFF"))
  {
    type = RIFF;
    file->read(&id, sizeof(sfzero::fourcc));
    start += sizeof(sfzero::fourcc);
    size -= sizeof(sfzero::fourcc);
  }
  else if (FourCCEquals(id, "LIST"))
  {
    type = LIST;
    file->read(&id, sizeof(sfzero::fourcc));
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
  file->read(memoryBlock.getData(), static_cast<int>(memoryBlock.getSize()));
  return memoryBlock.toString();
}
