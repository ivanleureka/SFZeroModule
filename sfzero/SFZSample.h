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

  juce::File getFile() const noexcept { return (file_); }
  juce::AudioSampleBuffer *getBuffer() const noexcept { return buffer_.get(); }
  std::shared_ptr<juce::AudioSampleBuffer> getBufferShared() const noexcept { return buffer_; }
  double getSampleRate() const noexcept { return sampleRate_; }
  juce::String getShortName();

  /** Shares ownership of the buffer with the caller. Multiple Samples may share
      the same underlying AudioSampleBuffer (e.g., the SF2 case where one buffer
      backs every Sample). */
  void setBuffer(std::shared_ptr<juce::AudioSampleBuffer> newBuffer);

  juce::String dump();
  juce::uint64 getSampleLength() const noexcept { return sampleLength_; }
  juce::uint64 getLoopStart() const noexcept { return loopStart_; }
  juce::uint64 getLoopEnd() const noexcept { return loopEnd_; }

#ifdef JUCE_DEBUG
  void checkIfZeroed(const char *where);
#endif

private:
  juce::File file_;
  std::shared_ptr<juce::AudioSampleBuffer> buffer_;  ///< Shared buffer; refcount drops to zero when last Sample releases it.
  double sampleRate_;
  juce::uint64 sampleLength_, loopStart_, loopEnd_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Sample)
};
}

#endif // SFZSAMPLE_H_INCLUDED
