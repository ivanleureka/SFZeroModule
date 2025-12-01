/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SFZSynth.h"
#include "SFZSound.h"
#include "SF2SoundInstance.h"
#include "SFZVoice.h"

sfzero::Synth::Synth() : Synthesiser() {}

void sfzero::Synth::noteOn(int midiChannel, int midiNoteNumber, float velocity)
{
  int i;

  const juce::ScopedLock locker(lock);

  int midiVelocity = static_cast<int>(velocity * 127);

  // Get the sound - check for both Sound and SF2SoundInstance
  sfzero::Sound *sound = nullptr;
  sfzero::SF2SoundInstance *soundInstance = nullptr;

  if (getNumSounds() > 0)
  {
    auto* rawSound = getSound(0).get();
    sound = dynamic_cast<sfzero::Sound *>(rawSound);
    if (sound == nullptr)
    {
      soundInstance = dynamic_cast<sfzero::SF2SoundInstance *>(rawSound);
    }
  }

  // First, stop any currently-playing sounds in the group.
  //*** Currently, this only pays attention to the first matching region.
  int group = 0;
  sfzero::Region *regionForGroup = nullptr;

  if (sound)
  {
    regionForGroup = sound->getRegionFor(midiNoteNumber, midiVelocity);
  }
  else if (soundInstance)
  {
    regionForGroup = soundInstance->getRegionFor(midiNoteNumber, midiVelocity);
  }

  if (regionForGroup)
  {
    group = regionForGroup->group;
  }

  if (group != 0)
  {
    for (i = voices.size(); --i >= 0;)
    {
      sfzero::Voice *voice = dynamic_cast<sfzero::Voice *>(voices.getUnchecked(i));
      if (voice == nullptr)
      {
        continue;
      }
      if (voice->getOffBy() == group)
      {
        voice->stopNoteForGroup();
      }
    }
  }

  // Are any notes playing?  (Needed for first/legato trigger handling.)
  // Also stop any voices still playing this note.
  bool anyNotesPlaying = false;
  for (i = voices.size(); --i >= 0;)
  {
    sfzero::Voice *voice = dynamic_cast<sfzero::Voice *>(voices.getUnchecked(i));
    if (voice == nullptr)
    {
      continue;
    }
    if (voice->isPlayingChannel(midiChannel))
    {
      if (voice->isPlayingNoteDown())
      {
        if (voice->getCurrentlyPlayingNote() == midiNoteNumber)
        {
          if (!voice->isPlayingOneShot())
          {
            voice->stopNoteQuick();
          }
        }
        else
        {
          anyNotesPlaying = true;
        }
      }
    }
  }

  // Play *all* matching regions.
  sfzero::Region::Trigger trigger = (anyNotesPlaying ? sfzero::Region::legato : sfzero::Region::first);

  // Handle Sound type
  if (sound)
  {
    int numRegions = sound->getNumRegions();
    for (i = 0; i < numRegions; ++i)
    {
      sfzero::Region *region = sound->regionAt(i);
      if (region->matches(midiNoteNumber, midiVelocity, trigger))
      {
        sfzero::Voice *voice =
            dynamic_cast<sfzero::Voice *>(findFreeVoice(sound, midiNoteNumber, midiChannel, isNoteStealingEnabled()));
        if (voice)
        {
          voice->setRegion(region);
          startVoice(voice, sound, midiChannel, midiNoteNumber, velocity);
        }
      }
    }
  }
  // Handle SF2SoundInstance type
  else if (soundInstance)
  {
    int numRegions = soundInstance->getNumRegions();
    for (i = 0; i < numRegions; ++i)
    {
      sfzero::Region *region = soundInstance->regionAt(i);
      if (region->matches(midiNoteNumber, midiVelocity, trigger))
      {
        sfzero::Voice *voice =
            dynamic_cast<sfzero::Voice *>(findFreeVoice(soundInstance, midiNoteNumber, midiChannel, isNoteStealingEnabled()));
        if (voice)
        {
          voice->setRegion(region);
          startVoice(voice, soundInstance, midiChannel, midiNoteNumber, velocity);
        }
      }
    }
  }

  noteVelocities_[midiNoteNumber] = midiVelocity;
}

void sfzero::Synth::noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff)
{
  const juce::ScopedLock locker(lock);

  Synthesiser::noteOff(midiChannel, midiNoteNumber, velocity, allowTailOff);

  // Start release region.
  // Get the sound - check for both Sound and SF2SoundInstance
  sfzero::Sound *sound = nullptr;
  sfzero::SF2SoundInstance *soundInstance = nullptr;

  if (getNumSounds() > 0)
  {
    auto* rawSound = getSound(0).get();
    sound = dynamic_cast<sfzero::Sound *>(rawSound);
    if (sound == nullptr)
    {
      soundInstance = dynamic_cast<sfzero::SF2SoundInstance *>(rawSound);
    }
  }

  sfzero::Region *region = nullptr;

  if (sound)
  {
    region = sound->getRegionFor(midiNoteNumber, noteVelocities_[midiNoteNumber], sfzero::Region::release);
    if (region)
    {
      sfzero::Voice *voice = dynamic_cast<sfzero::Voice *>(findFreeVoice(sound, midiNoteNumber, midiChannel, false));
      if (voice)
      {
        // Synthesiser is too locked-down (ivars are private rt protected), so
        // we have to use a "setRegion()" mechanism.
        voice->setRegion(region);
        startVoice(voice, sound, midiChannel, midiNoteNumber, noteVelocities_[midiNoteNumber] / 127.0f);
      }
    }
  }
  else if (soundInstance)
  {
    region = soundInstance->getRegionFor(midiNoteNumber, noteVelocities_[midiNoteNumber], sfzero::Region::release);
    if (region)
    {
      sfzero::Voice *voice = dynamic_cast<sfzero::Voice *>(findFreeVoice(soundInstance, midiNoteNumber, midiChannel, false));
      if (voice)
      {
        voice->setRegion(region);
        startVoice(voice, soundInstance, midiChannel, midiNoteNumber, noteVelocities_[midiNoteNumber] / 127.0f);
      }
    }
  }
}

int sfzero::Synth::numVoicesUsed()
{
  int numUsed = 0;

  for (int i = voices.size(); --i >= 0;)
  {
    if (voices.getUnchecked(i)->getCurrentlyPlayingNote() >= 0)
    {
      numUsed += 1;
    }
  }
  return numUsed;
}

juce::String sfzero::Synth::voiceInfoString()
{
  enum
  {
    maxShownVoices = 20,
  };

  juce::StringArray lines;
  int numUsed = 0, numShown = 0;
  for (int i = voices.size(); --i >= 0;)
  {
    sfzero::Voice *voice = dynamic_cast<sfzero::Voice *>(voices.getUnchecked(i));
    if (voice->getCurrentlyPlayingNote() < 0)
    {
      continue;
    }
    numUsed += 1;
    if (numShown >= maxShownVoices)
    {
      continue;
    }
    lines.add(voice->infoString());
  }
  lines.insert(0, "voices used: " + juce::String(numUsed));
  return lines.joinIntoString("\n");
}
