/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#ifndef SFZREADER_H_INCLUDED
#define SFZREADER_H_INCLUDED

#include "SFZCommon.h"
#include "SFZSafeCast.h"
#include <cstring>

namespace sfzero
{

struct Region;
class Sound;

class Reader
{
public:
  explicit Reader(Sound *sound) noexcept;
  ~Reader();

  // Rule of five: copy ops deleted by JUCE_DECLARE_NON_COPYABLE below; delete
  // move ops too (C26432) without re-declaring copy.
  Reader(Reader &&) = delete;
  Reader &operator=(Reader &&) = delete;

  void read(const juce::File &file);
  void read(const char *text, unsigned int length);

private:
  const char *handleLineEnd(const char *p) noexcept;
  const char *readPathInto(juce::String *pathOut, const char *p, const char *end);
  int keyValue(const juce::String &str);
  int triggerValue(const juce::String &str) noexcept;
  int loopModeValue(const juce::String &str) noexcept;
  void finishRegion(Region *region);
  void error(const juce::String &message);

  Sound *sound_;
  int line_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Reader)
};

class StringSlice
{
public:
  StringSlice(const char *startIn, const char *endIn) noexcept : start_(startIn), end_(endIn) {}

  unsigned int length() const noexcept { return narrowCast<unsigned int>(end_ - start_); }
  bool operator==(const char *other) const noexcept { return (strncmp(start_, other, length()) == 0); }
  bool operator!=(const char *other) const noexcept { return (strncmp(start_, other, length()) != 0); }
  const char *getStart() const noexcept { return start_; }
  const char *getEnd() const noexcept { return end_; }
private:
  const char *start_;
  const char *end_;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StringSlice)
};
}

#endif // SFZREADER_H_INCLUDED
