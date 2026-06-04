/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#ifndef SF2_H_INCLUDED
#define SF2_H_INCLUDED

#include "SF2WinTypes.h"
#include <memory>

#define SF2Field(type, name) type name;

namespace sfzero
{

namespace SF2
{

struct rangesType
{
  byte lo, hi;
};

union genAmountType {
  rangesType range;
  short shortAmount;
  word wordAmount;
};

struct iver
{
#include "sf2-chunks/iver.h"
  void readFrom(juce::InputStream *file);
};

struct phdr
{
#include "sf2-chunks/phdr.h"
  void readFrom(juce::InputStream *file);

  static const int sizeInFile = 38;
};

struct pbag
{
#include "sf2-chunks/pbag.h"
  void readFrom(juce::InputStream *file);

  static const int sizeInFile = 4;
};

struct pmod
{
#include "sf2-chunks/pmod.h"
  void readFrom(juce::InputStream *file);

  static const int sizeInFile = 10;
};

struct pgen
{
#include "sf2-chunks/pgen.h"
  void readFrom(juce::InputStream *file);

  static const int sizeInFile = 4;
};

struct inst
{
#include "sf2-chunks/inst.h"
  void readFrom(juce::InputStream *file);

  static const int sizeInFile = 22;
};

struct ibag
{
#include "sf2-chunks/ibag.h"
  void readFrom(juce::InputStream *file);

  static const int sizeInFile = 4;
};

struct imod
{
#include "sf2-chunks/imod.h"
  void readFrom(juce::InputStream *file);

  static const int sizeInFile = 10;
};

struct igen
{
#include "sf2-chunks/igen.h"
  void readFrom(juce::InputStream *file);

  static const int sizeInFile = 4;
};

struct shdr
{
#include "sf2-chunks/shdr.h"
  void readFrom(juce::InputStream *file);

  static const int sizeInFile = 46;
};

struct Hydra
{
  std::unique_ptr<phdr[]> phdrItems;
  std::unique_ptr<pbag[]> pbagItems;
  std::unique_ptr<pmod[]> pmodItems;
  std::unique_ptr<pgen[]> pgenItems;
  std::unique_ptr<inst[]> instItems;
  std::unique_ptr<ibag[]> ibagItems;
  std::unique_ptr<imod[]> imodItems;
  std::unique_ptr<igen[]> igenItems;
  std::unique_ptr<shdr[]> shdrItems;

  int phdrNumItems = 0, pbagNumItems = 0, pmodNumItems = 0, pgenNumItems = 0;
  int instNumItems = 0, ibagNumItems = 0, imodNumItems = 0, igenNumItems = 0;
  int shdrNumItems = 0;

  Hydra() = default;
  ~Hydra() = default;

  void readFrom(juce::InputStream *file, juce::int64 pdtaChunkEnd);
  bool isComplete() noexcept;
};
}
}

#undef SF2Field

#endif // SF2_H_INCLUDED
