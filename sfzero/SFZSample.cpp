/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SFZSample.h"
#include "SFZDebug.h"

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

  buffer_ = std::make_shared<juce::AudioSampleBuffer>(reader->numChannels, static_cast<int>(sampleLength_ + 4));
  reader->read(buffer_.get(), 0, static_cast<int>(sampleLength_ + 4), 0, true, true);

  juce::StringPairArray *metadata = &reader->metadataValues;
  int numLoops = metadata->getValue("NumSampleLoops", "0").getIntValue();
  if (numLoops > 0)
  {
    loopStart_ = metadata->getValue("Loop0Start", "0").getLargeIntValue();
    loopEnd_ = metadata->getValue("Loop0End", "0").getLargeIntValue();
  }
  return true;
}

juce::String sfzero::Sample::getShortName() { return (file_.getFileName()); }

void sfzero::Sample::setBuffer(std::shared_ptr<juce::AudioSampleBuffer> newBuffer)
{
  buffer_ = std::move(newBuffer);
  sampleLength_ = buffer_ ? buffer_->getNumSamples() : 0;
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
  const float *p = buffer_->getReadPointer(0);
  for (; samplesLeft > 0; --samplesLeft)
  {
    if (*p++ == 0.0)
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
