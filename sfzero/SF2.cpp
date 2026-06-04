/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SF2.h"
#include "RIFF.h"
#include "SFZSafeCast.h"
#include <span>

#define readAbyte(name, file) name = (byte)file->readByte();
#define readAchar(name, file) name = file->readByte();
#define readAdword(name, file) name = (dword)file->readInt();
#define readAword(name, file) name = (word)file->readShort();
#define readAshort(name, file) name = file->readShort();
#define readAchar20(name, file)                                                                                                  \
  {                                                                                                                              \
    const std::span<char> name##Bytes{name};                                                                                    \
    file->read(name##Bytes.data(), sfzero::narrowCast<int>(name##Bytes.size()));                                                \
  }
#define readAgenAmountType(name, file) name.shortAmount = file->readShort();

#define SF2Field(type, name) readA##type(name, file)

void sfzero::SF2::iver::readFrom(juce::InputStream *file)
{
#include "sf2-chunks/iver.h"
}

void sfzero::SF2::phdr::readFrom(juce::InputStream *file)
{
#include "sf2-chunks/phdr.h"
}

void sfzero::SF2::pbag::readFrom(juce::InputStream *file)
{
#include "sf2-chunks/pbag.h"
}

void sfzero::SF2::pmod::readFrom(juce::InputStream *file)
{
#include "sf2-chunks/pmod.h"
}

void sfzero::SF2::pgen::readFrom(juce::InputStream *file)
{
#include "sf2-chunks/pgen.h"
}

void sfzero::SF2::inst::readFrom(juce::InputStream *file)
{
#include "sf2-chunks/inst.h"
}

void sfzero::SF2::ibag::readFrom(juce::InputStream *file)
{
#include "sf2-chunks/ibag.h"
}

void sfzero::SF2::imod::readFrom(juce::InputStream *file)
{
#include "sf2-chunks/imod.h"
}

void sfzero::SF2::igen::readFrom(juce::InputStream *file)
{
#include "sf2-chunks/igen.h"
}

void sfzero::SF2::shdr::readFrom(juce::InputStream *file)
{
#include "sf2-chunks/shdr.h"
}

void sfzero::SF2::Hydra::readFrom(juce::InputStream *file, juce::int64 pdtaChunkEnd)
{
  int numItems = 0;

#define HandleChunk(chunkName)                                                                                                   \
  if (FourCCEquals(chunk.id, #chunkName))                                                                                        \
  {                                                                                                                              \
    numItems = int (chunk.size / SF2::chunkName::sizeInFile);                                                                    \
    chunkName##NumItems = numItems;                                                                                              \
    chunkName##Items = std::make_unique<SF2::chunkName[]>(numItems);                                                             \
    const std::span<SF2::chunkName> items{chunkName##Items.get(), sfzero::narrowCast<size_t>(numItems)};                        \
    for (auto &item : items)                                                                                                     \
    {                                                                                                                            \
      item.readFrom(file);                                                                                                       \
    }                                                                                                                            \
  }                                                                                                                              \
  else

  while (file->getPosition() < pdtaChunkEnd)
  {
    sfzero::RIFFChunk chunk;
    chunk.readFrom(file);

    HandleChunk(phdr) HandleChunk(pbag) HandleChunk(pmod) HandleChunk(pgen) HandleChunk(inst) HandleChunk(ibag) HandleChunk(imod)
        HandleChunk(igen) HandleChunk(shdr)
    {
    }
    chunk.seekAfter(file);
  }
}

bool sfzero::SF2::Hydra::isComplete() noexcept
{
  return phdrItems && pbagItems && pmodItems && pgenItems && instItems && ibagItems && imodItems && igenItems && shdrItems;
}
