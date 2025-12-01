/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#ifndef SFZSAMPLE_H_INCLUDED
#define SFZSAMPLE_H_INCLUDED

#include "SFZCommon.h"
#include <memory>

namespace sfzero
{

class Sample
{
public:
  explicit Sample(const juce::File &fileIn) : file_(fileIn), sampleRate_(0), sampleLength_(0), loopStart_(0), loopEnd_(0) {}
  explicit Sample(double sampleRateIn) : sampleRate_(sampleRateIn), sampleLength_(0), loopStart_(0), loopEnd_(0) {}
  ~Sample() = default;

  bool load(juce::AudioFormatManager *formatManager);

  juce::File getFile() { return (file_); }
  juce::AudioSampleBuffer *getBuffer() { return buffer_.get(); }
  double getSampleRate() { return (sampleRate_); }
  juce::String getShortName();

  /** Takes ownership of the buffer. */
  void setBuffer(juce::AudioSampleBuffer *newBuffer);

  /** Releases ownership of the buffer to the caller. */
  juce::AudioSampleBuffer *detachBuffer();

  juce::String dump();
  juce::uint64 getSampleLength() const { return sampleLength_; }
  juce::uint64 getLoopStart() const { return loopStart_; }
  juce::uint64 getLoopEnd() const { return loopEnd_; }

#ifdef JUCE_DEBUG
  void checkIfZeroed(const char *where);
#endif

private:
  juce::File file_;
  std::unique_ptr<juce::AudioSampleBuffer> buffer_;  ///< Owned sample data
  double sampleRate_;
  juce::uint64 sampleLength_, loopStart_, loopEnd_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Sample)
};
}

#endif // SFZSAMPLE_H_INCLUDED
