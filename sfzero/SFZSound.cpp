/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SFZSound.h"
#include "SFZReader.h"
#include "SFZRegion.h"
#include "SFZSample.h"

sfzero::Sound::Sound(const juce::File &fileIn) : file_(fileIn) {}
sfzero::Sound::~Sound() = default;

bool sfzero::Sound::appliesToNote(int /*midiNoteNumber*/)
{
  // Just say yes; we can't truly know unless we're told the velocity as well.
  return true;
}

bool sfzero::Sound::appliesToChannel(int /*midiChannel*/) { return true; }
void sfzero::Sound::addRegion(std::unique_ptr<sfzero::Region> region)
{
  regions_.add(region.get());          // Borrowed view of the just-added region.
  sfzOwnedRegions_.add(region.release()); // OwnedArray now owns the lifetime.
}
sfzero::Sample *sfzero::Sound::addSample(juce::String path, juce::String defaultPath)
{
  path = path.replaceCharacter('\\', '/');
  defaultPath = defaultPath.replaceCharacter('\\', '/');
  juce::File sampleFile;
  if (defaultPath.isEmpty())
  {
    sampleFile = file_.getSiblingFile(path);
  }
  else
  {
    juce::File defaultDir = file_.getSiblingFile(defaultPath);
    sampleFile = defaultDir.getChildFile(path);
  }
  juce::String samplePath = sampleFile.getFullPathName();
  sfzero::Sample *sample = samplesByPath_[samplePath];
  if (sample == nullptr)
  {
    sample = samplesStorage_.add(std::make_unique<sfzero::Sample>(sampleFile));
    samplesByPath_.set(samplePath, sample);
  }
  return sample;
}

void sfzero::Sound::addError(const juce::String &message) { errors_.add(message); }

juce::StringArray sfzero::Sound::getUnsupportedOpcodes()
{
  juce::StringArray result;
  for (juce::HashMap<juce::String, OpcodeStats>::Iterator it(unsupportedOpcodes_); it.next();)
  {
    result.add(it.getKey());
  }
  return result;
}

juce::StringArray sfzero::Sound::getUnsupportedOpcodeReport()
{
  juce::StringArray result;
  for (juce::HashMap<juce::String, OpcodeStats>::Iterator it(unsupportedOpcodes_); it.next();)
  {
    const auto &name = it.getKey();
    const auto &s = it.getValue();
    juce::String line;
    line << name << ": ";
    if (s.hasAmount)
    {
      line << s.total << " regions, " << s.nonZero << " non-zero";
      if (s.nonZero > 0)
      {
        line << " (range " << s.minAmount << ".." << s.maxAmount << ")";
      }
    }
    else
    {
      line << s.total << " occurrences";
    }
    result.add(line);
  }
  return result;
}

void sfzero::Sound::addUnsupportedOpcode(const juce::String &opcode)
{
  const bool firstSeen = !unsupportedOpcodes_.contains(opcode);
  OpcodeStats stats = firstSeen ? OpcodeStats{} : unsupportedOpcodes_[opcode];
  stats.total += 1;
  unsupportedOpcodes_.set(opcode, stats);
  if (firstSeen)
  {
    juce::String warning = "unsupported opcode: ";
    warning << opcode;
    warnings_.add(warning);
  }
}

void sfzero::Sound::addUnsupportedOpcode(const juce::String &opcode, int amount)
{
  const bool firstSeen = !unsupportedOpcodes_.contains(opcode);
  OpcodeStats stats = firstSeen ? OpcodeStats{} : unsupportedOpcodes_[opcode];
  stats.total += 1;
  if (!stats.hasAmount)
  {
    stats.hasAmount = true;
    stats.minAmount = amount;
    stats.maxAmount = amount;
  }
  else
  {
    if (amount < stats.minAmount) stats.minAmount = amount;
    if (amount > stats.maxAmount) stats.maxAmount = amount;
  }
  if (amount != 0)
  {
    stats.nonZero += 1;
  }
  unsupportedOpcodes_.set(opcode, stats);
  if (firstSeen)
  {
    juce::String warning = "unsupported opcode: ";
    warning << opcode;
    warnings_.add(warning);
  }
}

void sfzero::Sound::loadRegions()
{
  sfzero::Reader reader(this);

  reader.read(file_);
}

void sfzero::Sound::loadSamples(juce::AudioFormatManager *formatManager, double *progressVar, juce::Thread *thread)
{
  if (progressVar)
  {
    *progressVar = 0.0;
  }

  double numSamplesLoaded = 1.0, numSamples = samplesByPath_.size();
  for (juce::HashMap<juce::String, sfzero::Sample *>::Iterator i(samplesByPath_); i.next();)
  {
    sfzero::Sample *sample = i.getValue();
    bool ok = sample->load(formatManager);
    if (!ok)
    {
      addError("Couldn't load sample \"" + sample->getShortName() + "\"");
    }

    numSamplesLoaded += 1.0;
    if (progressVar)
    {
      *progressVar = numSamplesLoaded / numSamples;
    }
    if (thread && thread->threadShouldExit())
    {
      return;
    }
  }

  if (progressVar)
  {
    *progressVar = 1.0;
  }
}

sfzero::Region *sfzero::Sound::getRegionFor(int note, int velocity, sfzero::Region::Trigger trigger)
{
  int numRegions = regions_.size();

  for (int i = 0; i < numRegions; ++i)
  {
    sfzero::Region *region = regions_[i];
    if (region->matches(note, velocity, trigger))
    {
      return region;
    }
  }

  return nullptr;
}

int sfzero::Sound::numSubsounds() { return 1; }

juce::String sfzero::Sound::subsoundName(int /*whichSubsound*/) { return {}; }

void sfzero::Sound::useSubsound(int /*whichSubsound*/) {}

int sfzero::Sound::selectedSubsound() { return 0; }

juce::String sfzero::Sound::dump()
{
  juce::String info;
  auto &errors = getErrors();
  if (errors.size() > 0)
  {
    info << errors.size() << " errors: \n";
    info << errors.joinIntoString("\n");
    info << "\n";
  }
  else
  {
    info << "no errors.\n\n";
  }

  auto &warnings = getWarnings();
  if (warnings.size() > 0)
  {
    info << warnings.size() << " warnings: \n";
    info << warnings.joinIntoString("\n");
  }
  else
  {
    info << "no warnings.\n";
  }

  if (regions_.size() > 0)
  {
    info << regions_.size() << " regions: \n";
    for (int i = 0; i < regions_.size(); ++i)
    {
      info << regions_[i]->dump();
    }
  }
  else
  {
    info << "no regions.\n";
  }

  if (samplesByPath_.size() > 0)
  {
    info << samplesByPath_.size() << " samples: \n";
    for (juce::HashMap<juce::String, sfzero::Sample *>::Iterator i(samplesByPath_); i.next();)
    {
      info << i.getValue()->dump();
    }
  }
  else
  {
    info << "no samples.\n";
  }
  return info;
}
