/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SF2Sound.h"
#include "SF2Reader.h"
#include "SFZSample.h"

sfzero::SF2Sound::SF2Sound(const juce::File &file)
    : sfzero::Sound(file)
    , selectedPreset_(0)
    , memoryStream_(nullptr)
{
}

sfzero::SF2Sound::SF2Sound(const void* data, size_t dataSize)
    : sfzero::Sound(juce::File())  // Dummy file for base class
    , selectedPreset_(0)
    , memoryStream_(std::make_unique<juce::MemoryInputStream>(data, dataSize, false))
{
}

sfzero::SF2Sound::~SF2Sound()
{
  // regions_ is a borrowed view; ownership of these Regions lives in Preset::regions.
  // Clear the view explicitly so SFZSound's destructor sees an empty Array.
  // juce::Array::clear() only frees storage and cannot throw, so it is safe in
  // this implicitly-noexcept destructor.
#pragma warning(suppress : 26447)
  getRegions().clear();
  // Sample objects are owned by samplesStorage_ (auto-deleted after this body).
  // The shared AudioSampleBuffer is held by std::shared_ptr inside each Sample,
  // so it disappears once the last Sample is destroyed — no manual delete needed.
}

class PresetComparator
{
public:
  static int compareElements(const sfzero::SF2Sound::Preset *first, const sfzero::SF2Sound::Preset *second) noexcept
  {
    const int cmp = first->bank - second->bank;

    if (cmp != 0)
    {
      return cmp;
    }
    return first->preset - second->preset;
  }
};

void sfzero::SF2Sound::loadRegions()
{
  // Create reader either from memory or file
  if (memoryStream_)
  {
    // Create a new MemoryInputStream from the same data
    auto stream = std::make_unique<juce::MemoryInputStream>(
      memoryStream_->getData(),
      memoryStream_->getDataSize(),
      false
    );
    sfzero::SF2Reader reader(this, std::move(stream));
    reader.read();
  }
  else
  {
    sfzero::SF2Reader reader(this, getFile());
    reader.read();
  }

  // Sort the presets.
  PresetComparator comparator;
  presets_.sort(comparator);

  useSubsound(0);
}

void sfzero::SF2Sound::loadSamples(juce::AudioFormatManager * /*formatManager*/, double *progressVar, juce::Thread *thread)
{
  std::shared_ptr<juce::AudioSampleBuffer> buffer;

  // Create reader either from memory or file
  if (memoryStream_)
  {
    // Create a new MemoryInputStream from the same data
    auto stream = std::make_unique<juce::MemoryInputStream>(
      memoryStream_->getData(),
      memoryStream_->getDataSize(),
      false
    );
    sfzero::SF2Reader reader(this, std::move(stream));
    buffer = reader.readSamples(progressVar, thread);
  }
  else
  {
    sfzero::SF2Reader reader(this, getFile());
    buffer = reader.readSamples(progressVar, thread);
  }

  if (buffer)
  {
    // All the SFZSamples will share the buffer.
    for (juce::HashMap<int, sfzero::Sample *>::Iterator i(samplesByRate_); i.next();)
    {
      i.getValue()->setBuffer(buffer);
    }
  }

  if (progressVar)
  {
    *progressVar = 1.0;
  }
}

void sfzero::SF2Sound::addPreset(std::unique_ptr<sfzero::SF2Sound::Preset> preset) { presets_.add(preset.release()); }

int sfzero::SF2Sound::numSubsounds() { return presets_.size(); }

juce::String sfzero::SF2Sound::subsoundName(int whichSubsound)
{
  // juce::OwnedArray::operator[] is range-safe (returns nullptr if out of range).
#pragma warning(suppress : 26446)
  const Preset *preset = presets_[whichSubsound];
  juce::String result;

  if (preset->bank != 0)
  {
    result += preset->bank;
    result += "/";
  }
  result += preset->preset;
  result += ": ";
  result += preset->name;
  return result;
}

void sfzero::SF2Sound::useSubsound(int whichSubsound)
{
  selectedPreset_ = whichSubsound;
  getRegions().clear();
  // juce::OwnedArray::operator[] is range-safe (returns nullptr if out of range).
#pragma warning(suppress : 26446)
  getRegions().addArray(presets_[whichSubsound]->regions);
}

int sfzero::SF2Sound::selectedSubsound() { return selectedPreset_; }

sfzero::Sample *sfzero::SF2Sound::sampleFor(double sampleRate)
{
  sfzero::Sample *sample = samplesByRate_[static_cast<int>(sampleRate)];

  if (sample == nullptr)
  {
    sample = samplesStorage_.add(std::make_unique<sfzero::Sample>(sampleRate));
    samplesByRate_.set(static_cast<int>(sampleRate), sample);
  }
  return sample;
}

void sfzero::SF2Sound::setSamplesBuffer(std::shared_ptr<juce::AudioSampleBuffer> buffer)
{
  for (juce::HashMap<int, sfzero::Sample *>::Iterator i(samplesByRate_); i.next();)
  {
    i.getValue()->setBuffer(buffer);
  }
}
