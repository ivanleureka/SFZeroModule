/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SFZSample.h"
#include "SFZDebug.h"
#include "SFZSafeCast.h"
#include <cmath>

std::shared_ptr<sfzero::SampleBuffer> sfzero::SampleBuffer::fromFloat(const juce::AudioSampleBuffer &source)
{
  auto out = std::make_shared<SampleBuffer>(source.getNumChannels(), source.getNumSamples());
  const int numSamples = source.getNumSamples();
  for (int ch = 0; ch < source.getNumChannels(); ++ch)
  {
    const float *in = source.getReadPointer(ch);
    std::int16_t *dest = out->getWritePointer(ch);
    for (int i = 0; i < numSamples; ++i)
    {
      // Load-time conversion; sequential walk bounded by numSamples (C26481).
#pragma warning(suppress : 26481)
      const float clipped = juce::jlimit(-1.0f, 1.0f, in[i]);
#pragma warning(suppress : 26481)
      dest[i] = static_cast<std::int16_t>(std::lrint(clipped * 32767.0f));
    }
  }
  return out;
}

bool sfzero::Sample::load(juce::AudioFormatManager *formatManager)
{
  std::unique_ptr<juce::AudioFormatReader> reader(formatManager->createReaderFor(file_));

  if (reader == nullptr)
  {
    return false;
  }
  sampleRate_ = reader->sampleRate;
  sampleLength_ = reader->lengthInSamples;
  // Read some extra samples, which will be filled with zeros, so interpolation
  // can be done without having to check for the edge all the time.
  jassert(sampleLength_ < std::numeric_limits<int>::max());

  juce::AudioSampleBuffer floatBuffer(reader->numChannels, narrowCast<int>(sampleLength_ + 4));
  floatBuffer.clear();
  reader->read(&floatBuffer, 0, narrowCast<int>(sampleLength_ + 4), 0, true, true);
  buffer_ = SampleBuffer::fromFloat(floatBuffer);

  const juce::StringPairArray *metadata = &reader->metadataValues;
  const int numLoops = metadata->getValue("NumSampleLoops", "0").getIntValue();
  if (numLoops > 0)
  {
    loopStart_ = metadata->getValue("Loop0Start", "0").getLargeIntValue();
    loopEnd_ = metadata->getValue("Loop0End", "0").getLargeIntValue();
  }
  return true;
}

juce::String sfzero::Sample::getShortName() { return (file_.getFileName()); }

void sfzero::Sample::setBuffer(std::shared_ptr<SampleBuffer> newBuffer) noexcept
{
  buffer_ = std::move(newBuffer);
  sampleLength_ = buffer_ ? static_cast<juce::uint64>(buffer_->getNumSamples()) : 0;
}

void sfzero::Sample::setBuffer(std::shared_ptr<juce::AudioSampleBuffer> floatBuffer)
{
  setBuffer(floatBuffer ? SampleBuffer::fromFloat(*floatBuffer) : nullptr);
}

juce::String sfzero::Sample::dump() { return file_.getFullPathName() + "\n"; }

#ifdef JUCE_DEBUG
void sfzero::Sample::checkIfZeroed(const char *where)
{
  if (!buffer_)
  {
    sfzero::dbgprintf("SFZSample::checkIfZeroed(%s): no buffer!", where);
    return;
  }

  int samplesLeft = buffer_->getNumSamples();
  juce::int64 nonzero = 0, zero = 0;
  const std::int16_t *p = buffer_->getReadPointer(0);
  for (; samplesLeft > 0; --samplesLeft)
  {
    // Debug-only zero-check; sequential pointer walk (C26481) over the read
    // pointer is bounded by getNumSamples().
#pragma warning(suppress : 26481)
    if (*p++ == 0)
    {
      zero += 1;
    }
    else
    {
      nonzero += 1;
    }
  }
  if (nonzero > 0)
  {
    sfzero::dbgprintf("Buffer not zeroed at %s (%lu vs. %lu).", where, nonzero, zero);
  }
  else
  {
    sfzero::dbgprintf("Buffer zeroed at %s!  (%lu zeros)", where, zero);
  }
}

#endif // JUCE_DEBUG
