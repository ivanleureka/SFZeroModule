/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SF2Reader.h"
#include "RIFF.h"
#include "SF2.h"
#include "SF2Generator.h"
#include "SF2Sound.h"
#include "SFZSafeCast.h"
#include <array>
#include <span>
#include <vector>

sfzero::SF2Reader::SF2Reader(sfzero::SF2Sound *soundIn, const juce::File &fileIn)
    : sound_(soundIn)
    , file_(fileIn.createInputStream())
{
}

sfzero::SF2Reader::SF2Reader(sfzero::SF2Sound *soundIn, std::unique_ptr<juce::InputStream> stream) noexcept
    : sound_(soundIn)
    , file_(std::move(stream))
{
}

void sfzero::SF2Reader::read()
{
#pragma warning(push)
#pragma warning(disable : 26446)   // span::operator[] is unchecked; indices are span-bounded (file-load, not real-time)
  if (file_ == nullptr)
  {
    sound_->addError("Couldn't open file.");
    return;
  }

  // Read the hydra.
  sfzero::SF2::Hydra hydra;
  file_->setPosition(0);
  sfzero::RIFFChunk riffChunk;
  riffChunk.readFrom(file_.get());
  while (file_->getPosition() < riffChunk.end())
  {
    sfzero::RIFFChunk chunk;
    chunk.readFrom(file_.get());
    if (FourCCEquals(chunk.id, "pdta"))
    {
      hydra.readFrom(file_.get(), chunk.end());
      break;
    }
    chunk.seekAfter(file_.get());
  }
  if (!hydra.isComplete())
  {
    sound_->addError("Invalid SF2 file (missing or incomplete hydra).");
    return;
  }

  // Span views over the parsed hydra arrays so element access and the SF2
  // "next item" index lookups are bounds-described rather than raw pointer
  // arithmetic. (File-load path, not real-time: operator[] is span-bounded.)
  const std::span<sfzero::SF2::phdr> phdrItems{hydra.phdrItems.get(), narrowCast<size_t>(hydra.phdrNumItems)};
  const std::span<sfzero::SF2::pbag> pbagItems{hydra.pbagItems.get(), narrowCast<size_t>(hydra.pbagNumItems)};
  const std::span<sfzero::SF2::pgen> pgenItems{hydra.pgenItems.get(), narrowCast<size_t>(hydra.pgenNumItems)};
  const std::span<sfzero::SF2::inst> instItems{hydra.instItems.get(), narrowCast<size_t>(hydra.instNumItems)};
  const std::span<sfzero::SF2::ibag> ibagItems{hydra.ibagItems.get(), narrowCast<size_t>(hydra.ibagNumItems)};
  const std::span<sfzero::SF2::igen> igenItems{hydra.igenItems.get(), narrowCast<size_t>(hydra.igenNumItems)};
  const std::span<sfzero::SF2::shdr> shdrItems{hydra.shdrItems.get(), narrowCast<size_t>(hydra.shdrNumItems)};

  // Read each preset.
  for (int whichPreset = 0; whichPreset < hydra.phdrNumItems - 1; ++whichPreset)
  {
    const sfzero::SF2::phdr &phdr = phdrItems[narrowCast<size_t>(whichPreset)];
    auto presetOwner = std::make_unique<sfzero::SF2Sound::Preset>(phdr.presetName, phdr.bank, phdr.preset);
    sfzero::SF2Sound::Preset *preset = presetOwner.get(); // Borrowed view used below; lifetime owned by SF2Sound::presets_ after addPreset.
    sound_->addPreset(std::move(presetOwner));

    // Zones.
    //*** TODO: Handle global zone (modulators only).
    const int zoneEnd = phdrItems[narrowCast<size_t>(whichPreset) + 1].presetBagNdx;
    for (int whichZone = phdr.presetBagNdx; whichZone < zoneEnd; ++whichZone)
    {
      const sfzero::SF2::pbag &pbag = pbagItems[narrowCast<size_t>(whichZone)];
      sfzero::Region presetRegion;
      presetRegion.clearForRelativeSF2();

      // Generators.
      const int genEnd = pbagItems[narrowCast<size_t>(whichZone) + 1].genNdx;
      for (int whichGen = pbag.genNdx; whichGen < genEnd; ++whichGen)
      {
        const sfzero::SF2::pgen &pgen = pgenItems[narrowCast<size_t>(whichGen)];

        // Instrument.
        if (pgen.genOper == sfzero::SF2Generator::instrument)
        {
          const sfzero::word whichInst = pgen.genAmount.wordAmount;
          if (whichInst < hydra.instNumItems)
          {
            sfzero::Region instRegion;
            instRegion.clearForSF2();
            // Preset generators are supposed to be "relative" modifications of
            // the instrument settings, but that makes no sense for ranges.
            // For those, we'll have the instrument's generator take
            // precedence, though that may not be correct.
            instRegion.lokey = presetRegion.lokey;
            instRegion.hikey = presetRegion.hikey;
            instRegion.lovel = presetRegion.lovel;
            instRegion.hivel = presetRegion.hivel;

            const sfzero::SF2::inst &inst = instItems[whichInst];
            const int firstZone = inst.instBagNdx;
            const int zoneEnd2 = instItems[narrowCast<size_t>(whichInst) + 1].instBagNdx;
            for (int whichZone2 = firstZone; whichZone2 < zoneEnd2; ++whichZone2)
            {
              const sfzero::SF2::ibag &ibag = ibagItems[narrowCast<size_t>(whichZone2)];

              // Generators.
              sfzero::Region zoneRegion = instRegion;
              bool hadSampleID = false;
              const int genEnd2 = ibagItems[narrowCast<size_t>(whichZone2) + 1].instGenNdx;
              for (int whichGen2 = ibag.instGenNdx; whichGen2 < genEnd2; ++whichGen2)
              {
                const sfzero::SF2::igen &igen = igenItems[narrowCast<size_t>(whichGen2)];
                if (igen.genOper == sfzero::SF2Generator::sampleID)
                {
                  const int whichSample = igen.genAmount.wordAmount;
                  const sfzero::SF2::shdr &shdr = shdrItems[narrowCast<size_t>(whichSample)];
                  zoneRegion.addForSF2(&presetRegion);
                  zoneRegion.sf2ToSFZ();
                  zoneRegion.offset += shdr.start;
                  zoneRegion.end += shdr.end;
                  zoneRegion.loop_start += shdr.startLoop;
                  zoneRegion.loop_end += shdr.endLoop;
                  if (shdr.endLoop > 0)
                  {
                    zoneRegion.loop_end -= 1;
                  }
                  if (zoneRegion.pitch_keycenter == -1)
                  {
                    zoneRegion.pitch_keycenter = shdr.originalPitch;
                  }
                  zoneRegion.tune += shdr.pitchCorrection;

                  // Pin initialAttenuation to max +6dB.
                  if (zoneRegion.volume > 6.0)
                  {
                    zoneRegion.volume = 6.0;
                    sound_->addUnsupportedOpcode("extreme gain in initialAttenuation");
                  }

                  auto newRegion = std::make_unique<sfzero::Region>();
                  *newRegion = zoneRegion;
                  newRegion->sample = sound_->sampleFor(shdr.sampleRate);
                  preset->addRegion(std::move(newRegion));
                  hadSampleID = true;
                }
                else
                {
                  addGeneratorToRegion(igen.genOper, &igen.genAmount, &zoneRegion);
                }
              }

              // Handle instrument's global zone.
              if ((whichZone2 == firstZone) && !hadSampleID)
              {
                instRegion = zoneRegion;
              }

              // Modulators.
              const int modEnd = ibagItems[narrowCast<size_t>(whichZone2) + 1].instModNdx;
              const int whichMod = ibag.instModNdx;
              if (whichMod < modEnd)
              {
                sound_->addUnsupportedOpcode(
                    (whichZone2 == firstZone)
                        ? "instrument global zone modulator"
                        : "instrument zone modulator");
              }
            }
          }
          else
          {
            sound_->addError("Instrument out of range.");
          }
        }
        // Other generators.
        else
        {
          addGeneratorToRegion(pgen.genOper, &pgen.genAmount, &presetRegion);
        }
      }

      // Modulators. The first zone of each preset is the global zone
      // (modulators only) per SF2 spec; tag it distinctly.
      const int modEnd = pbagItems[narrowCast<size_t>(whichZone) + 1].modNdx;
      const int whichMod = pbag.modNdx;
      if (whichMod < modEnd)
      {
        sound_->addUnsupportedOpcode(
            (whichZone == phdr.presetBagNdx)
                ? "preset global zone modulator"
                : "preset zone modulator");
      }
    }
  }
#pragma warning(pop)
}

std::shared_ptr<juce::AudioSampleBuffer> sfzero::SF2Reader::readSamples(double *progressVar, juce::Thread *thread)
{
#pragma warning(push)
#pragma warning(disable : 26446)   // span::operator[] is unchecked; indices are span-bounded (file-load, not real-time)
  constexpr int bufferSize = 32768;

  if (file_ == nullptr)
  {
    sound_->addError("Couldn't open file.");
    return nullptr;
  }

  // Find the "sdta" chunk.
  file_->setPosition(0);
  sfzero::RIFFChunk riffChunk;
  riffChunk.readFrom(file_.get());
  bool foundSdta = false;
  sfzero::RIFFChunk chunk;
  while (file_->getPosition() < riffChunk.end())
  {
    chunk.readFrom(file_.get());
    if (FourCCEquals(chunk.id, "sdta"))
    {
      foundSdta = true;
      break;
    }
    chunk.seekAfter(file_.get());
  }
  // Guard: if no "sdta" chunk was found, `chunk` describes the last chunk we
  // scanned (or is zero-initialised) and chunk.end() would be meaningless — bail
  // before reading it. (Fixes C6001: read of uninitialised/stale chunk.end().)
  if (!foundSdta)
  {
    sound_->addError("SF2 is missing its \"sdta\" chunk.");
    return nullptr;
  }
  const juce::int64 sdtaEnd = chunk.end();
  bool found = false;
  while (file_->getPosition() < sdtaEnd)
  {
    chunk.readFrom(file_.get());
    if (FourCCEquals(chunk.id, "smpl"))
    {
      found = true;
      break;
    }
    chunk.seekAfter(file_.get());
  }
  if (!found)
  {
    sound_->addError("SF2 is missing its \"smpl\" chunk.");
    return nullptr;
  }

  // Allocate the shared sample buffer; every SFZSample built from this SF2 will share it.
  const int numSamples = narrowCast<int>(chunk.size / sizeof(short));
  auto sampleBuffer = std::make_shared<juce::AudioSampleBuffer>(1, numSamples);

  // Read and convert using RAII buffer. Span views replace raw pointer
  // arithmetic over the read buffer and the destination channel.
  std::vector<short> buffer(bufferSize);
  const std::span<float> outSamples{sampleBuffer->getWritePointer(0), narrowCast<size_t>(numSamples)};
  int samplesLeft = numSamples;
  size_t outIndex = 0;
  while (samplesLeft > 0)
  {
    // Read the buffer.
    int samplesToRead = bufferSize;
    if (samplesToRead > samplesLeft)
    {
      samplesToRead = samplesLeft;
    }
    file_->read(buffer.data(), samplesToRead * narrowCast<int>(sizeof(short)));

    // Convert from signed 16-bit to float.
    const std::span<short> inSamples{buffer.data(), narrowCast<size_t>(samplesToRead)};
    for (const short sampleValue : inSamples)
    {
      // If we ever need to compile for big-endian platforms, we'll need to
      // byte-swap here.
      outSamples[outIndex++] = sampleValue / 32767.0f;
    }

    samplesLeft -= samplesToRead;

    if (progressVar)
    {
      *progressVar = static_cast<float>(numSamples - samplesLeft) / numSamples;
    }
    if (thread && thread->threadShouldExit())
    {
      return nullptr;  // shared_ptr drops the buffer
    }
  }

  if (progressVar)
  {
    *progressVar = 1.0;
  }

  return sampleBuffer;
#pragma warning(pop)
}

void sfzero::SF2Reader::addGeneratorToRegion(sfzero::word genOper, const sfzero::SF2::genAmountType *amount, sfzero::Region *region)
{
  switch (genOper)
  {
  case sfzero::SF2Generator::startAddrsOffset:
    region->offset += amount->shortAmount;
    break;

  case sfzero::SF2Generator::endAddrsOffset:
    region->end += amount->shortAmount;
    break;

  case sfzero::SF2Generator::startloopAddrsOffset:
    region->loop_start += amount->shortAmount;
    break;

  case sfzero::SF2Generator::endloopAddrsOffset:
    region->loop_end += amount->shortAmount;
    break;

  case sfzero::SF2Generator::startAddrsCoarseOffset:
    region->offset += juce::int64(amount->shortAmount) * 32768;
    break;

  case sfzero::SF2Generator::endAddrsCoarseOffset:
    region->end += juce::int64(amount->shortAmount) * 32768;
    break;

  case sfzero::SF2Generator::pan:
    region->pan = amount->shortAmount * (2.0f / 10.0f);
    break;

  case sfzero::SF2Generator::delayVolEnv:
    region->ampeg.delay = amount->shortAmount;
    break;

  case sfzero::SF2Generator::attackVolEnv:
    region->ampeg.attack = amount->shortAmount;
    break;

  case sfzero::SF2Generator::holdVolEnv:
    region->ampeg.hold = amount->shortAmount;
    break;

  case sfzero::SF2Generator::decayVolEnv:
    region->ampeg.decay = amount->shortAmount;
    break;

  case sfzero::SF2Generator::sustainVolEnv:
    region->ampeg.sustain = amount->shortAmount;
    break;

  case sfzero::SF2Generator::releaseVolEnv:
    region->ampeg.release = amount->shortAmount;
    break;

  case sfzero::SF2Generator::keyRange:
    region->lokey = amount->range.lo;
    region->hikey = amount->range.hi;
    break;

  case sfzero::SF2Generator::velRange:
    region->lovel = amount->range.lo;
    region->hivel = amount->range.hi;
    break;

  case sfzero::SF2Generator::startloopAddrsCoarseOffset:
    region->loop_start += juce::int64(amount->shortAmount) * 32768;
    break;

  case sfzero::SF2Generator::initialAttenuation:
    // The spec says "initialAttenuation" is in centibels.  But everyone
    // seems to treat it as millibels.
    region->volume += -amount->shortAmount / 100.0f;
    break;

  case sfzero::SF2Generator::endloopAddrsCoarseOffset:
    region->loop_end += juce::int64(amount->shortAmount) * 32768;
    break;

  case sfzero::SF2Generator::coarseTune:
    region->transpose += amount->shortAmount;
    break;

  case sfzero::SF2Generator::fineTune:
    region->tune += amount->shortAmount;
    break;

  case sfzero::SF2Generator::sampleModes:
  {
    static constexpr std::array<sfzero::Region::LoopMode, 4> loopModes = {
        sfzero::Region::no_loop, sfzero::Region::loop_continuous, sfzero::Region::no_loop, sfzero::Region::loop_sustain};
    region->loop_mode = loopModes.at(narrowCast<size_t>(amount->wordAmount & 0x03));
  }
  break;

  case sfzero::SF2Generator::scaleTuning:
    region->pitch_keytrack = amount->shortAmount;
    break;

  case sfzero::SF2Generator::exclusiveClass:
    region->off_by = amount->wordAmount;
    region->group = narrowCast<int>(region->off_by);
    break;

  case sfzero::SF2Generator::overridingRootKey:
    region->pitch_keycenter = amount->shortAmount;
    break;

  case sfzero::SF2Generator::endOper:
    // Ignore.
    break;

  // Phase C - filter, mod-env, and vibrato-LFO generators. Each maps to a
  // Region field carrying its raw SF2 amount; conversions (timecents->seconds,
  // centibels->0-100, etc.) happen in Region::sf2ToSFZ() / at startNote time.
  case sfzero::SF2Generator::initialFilterFc:
    region->initialFilterFc = amount->shortAmount;
    break;
  case sfzero::SF2Generator::initialFilterQ:
    region->initialFilterQ = amount->shortAmount;
    break;
  case sfzero::SF2Generator::modEnvToFilterFc:
    region->modEnvToFilterFc = amount->shortAmount;
    break;
  case sfzero::SF2Generator::modEnvToPitch:
    region->modEnvToPitch = amount->shortAmount;
    break;
  case sfzero::SF2Generator::delayModEnv:
    region->modeg.delay = amount->shortAmount;
    break;
  case sfzero::SF2Generator::attackModEnv:
    region->modeg.attack = amount->shortAmount;
    break;
  case sfzero::SF2Generator::holdModEnv:
    region->modeg.hold = amount->shortAmount;
    break;
  case sfzero::SF2Generator::decayModEnv:
    region->modeg.decay = amount->shortAmount;
    break;
  case sfzero::SF2Generator::sustainModEnv:
    region->modeg.sustain = amount->shortAmount;
    break;
  case sfzero::SF2Generator::releaseModEnv:
    region->modeg.release = amount->shortAmount;
    break;
  case sfzero::SF2Generator::delayVibLFO:
    region->delayVibLFO = amount->shortAmount;
    break;
  case sfzero::SF2Generator::freqVibLFO:
    region->freqVibLFO = amount->shortAmount;
    break;
  case sfzero::SF2Generator::vibLfoToPitch:
    region->vibLfoToPitch = amount->shortAmount;
    break;

  case sfzero::SF2Generator::modLfoToPitch:
  case sfzero::SF2Generator::modLfoToFilterFc:
  case sfzero::SF2Generator::modLfoToVolume:
  case sfzero::SF2Generator::unused1:
  case sfzero::SF2Generator::chorusEffectsSend:
  case sfzero::SF2Generator::reverbEffectsSend:
  case sfzero::SF2Generator::unused2:
  case sfzero::SF2Generator::unused3:
  case sfzero::SF2Generator::unused4:
  case sfzero::SF2Generator::delayModLFO:
  case sfzero::SF2Generator::freqModLFO:
  case sfzero::SF2Generator::keynumToModEnvHold:
  case sfzero::SF2Generator::keynumToModEnvDecay:
  case sfzero::SF2Generator::keynumToVolEnvHold:
  case sfzero::SF2Generator::keynumToVolEnvDecay:
  case sfzero::SF2Generator::instrument:
  // Only allowed in certain places, where we already special-case it.
  case sfzero::SF2Generator::reserved1:
  case sfzero::SF2Generator::keynum:
  case sfzero::SF2Generator::velocity:
  case sfzero::SF2Generator::reserved2:
  case sfzero::SF2Generator::sampleID:
  // Only allowed in certain places, where we already special-case it.
  case sfzero::SF2Generator::reserved3:
  case sfzero::SF2Generator::unused5:
  {
    const sfzero::SF2Generator *generator = sfzero::GeneratorFor(static_cast<int>(genOper));
    sound_->addUnsupportedOpcode(generator->name, amount->shortAmount);
  }
  break;

  default:
    // Operator outside the known SF2 generator enum range; ignore (matches the
    // original fall-through-to-nothing behaviour, but explicit for C26818).
    break;
  }
}
