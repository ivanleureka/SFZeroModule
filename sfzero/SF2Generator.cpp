/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SF2Generator.h"
#include "SFZSafeCast.h"
#include <span>

#define SF2GeneratorValue(name, type)                                                                                            \
  {                                                                                                                              \
    #name, sfzero::SF2Generator::type                                                                                            \
  }

static const sfzero::SF2Generator generators[] = {

#include "sf2-chunks/generators.h"

};

#undef SF2GeneratorValue

const sfzero::SF2Generator *sfzero::GeneratorFor(int index) noexcept
{
#pragma warning(push)
#pragma warning(disable : 26446)   // span::operator[] is unchecked; indices are span-bounded (file-load, not real-time)
  const std::span<const sfzero::SF2Generator> gens{generators};

  if (index < 0 || sfzero::narrowCast<size_t>(index) >= gens.size())
  {
    return nullptr;
  }
  // Index was bounds-checked above, so operator[] is span-bounded here despite noexcept.
  return &gens[sfzero::narrowCast<size_t>(index)];
#pragma warning(pop)
}
