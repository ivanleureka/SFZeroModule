/*************************************************************************************
 * Original code copyright (C) 2012 Steve Folta
 * Converted to Juce module (C) 2016 Leo Olivers
 * Forked from https://github.com/stevefolta/SFZero
 * For license info please see the LICENSE file distributed with this source code
 *************************************************************************************/
#include "SFZDebug.h"
#include "SFZRegion.h"
#include "SFZSample.h"
#include "SFZSound.h"
#include "SF2SoundInstance.h"
#include "SFZVoice.h"
#include <cmath>
#include <math.h>

static constexpr float globalGain = -1.0f;

sfzero::Voice::Voice() noexcept
    : region_(nullptr), trigger_(0), curMidiNote_(0), curPitchWheel_(0), pitchRatio_(0), noteGainLeft_(0), noteGainRight_(0),
      sourceSamplePosition_(0), sampleEnd_(0), loopStart_(0), loopEnd_(0),
      bufferKeepAlive_(), inL_(nullptr), inR_(nullptr), bufferNumSamples_(0),
      currentCutoffHz_(20000.0f), currentQ_(0.7071068f), bypassFilter_(true),
      modegInUse_(false), modegFilterActive_(false), modegPitchActive_(false),
      vibInUse_(false), vibPhase_(0.0f), vibPhaseInc_(0.0f), vibDelaySamples_(0),
      numLoops_(0), curVelocity_(0)
{
  ampeg_.setExponentialDecay(true);
}

sfzero::Voice::~Voice() = default;

bool sfzero::Voice::canPlaySound(juce::SynthesiserSound *sound)
{
  // Support both regular Sound and SF2SoundInstance
  return dynamic_cast<sfzero::Sound *>(sound) != nullptr
      || dynamic_cast<sfzero::SF2SoundInstance *>(sound) != nullptr;
}

void sfzero::Voice::startNote(int midiNoteNumber, float floatVelocity, juce::SynthesiserSound *soundIn,
                              int currentPitchWheelPosition)
{
  // Try regular Sound first, then SF2SoundInstance
  sfzero::Sound *sound = dynamic_cast<sfzero::Sound *>(soundIn);
  sfzero::SF2SoundInstance *soundInstance = nullptr;

  if (sound == nullptr)
  {
    soundInstance = dynamic_cast<sfzero::SF2SoundInstance *>(soundIn);
    if (soundInstance == nullptr)
    {
      killNote();
      return;
    }
  }

  const int velocity = static_cast<int>(floatVelocity * 127.0);
  curVelocity_ = velocity;
  if (region_ == nullptr)
  {
    // Get region from whichever type of sound we have
    if (sound)
      region_ = sound->getRegionFor(midiNoteNumber, velocity);
    else
      region_ = soundInstance->getRegionFor(midiNoteNumber, velocity);
  }
  if ((region_ == nullptr) || (region_->sample == nullptr) || (region_->sample->getBuffer() == nullptr))
  {
    killNote();
    return;
  }
  if (region_->negative_end)
  {
    killNote();
    return;
  }

  // Hold a shared_ptr to the buffer for the note's lifetime so an in-flight
  // tail-off survives an SF2Sound swap (e.g. unloadSoundFont while voices play).
  bufferKeepAlive_ = region_->sample->getBufferShared();
  inL_ = bufferKeepAlive_->getReadPointer(0, 0);
  inR_ = bufferKeepAlive_->getNumChannels() > 1 ? bufferKeepAlive_->getReadPointer(1, 0) : nullptr;
  bufferNumSamples_ = bufferKeepAlive_->getNumSamples();
  jassert(region_->offset >= 0 && static_cast<int>(region_->offset) < bufferNumSamples_);

  // Pitch.
  curMidiNote_ = midiNoteNumber;
  curPitchWheel_ = currentPitchWheelPosition;
  calcPitchRatio();

  // Gain.
  double noteGainDB = globalGain + region_->volume;
  // Thanks to <http:://www.drealm.info/sfz/plj-sfz.xhtml> for explaining the
  // velocity curve in a way that I could understand, although they mean
  // "log10" when they say "log".
  // Widen velocity to double before squaring so the multiply can't overflow a
  // 32-bit int before the division (C26451).
  const double velocityD = static_cast<double>(velocity);
  double velocityGainDB = -20.0 * log10((127.0 * 127.0) / (velocityD * velocityD));
  velocityGainDB *= region_->amp_veltrack / 100.0;
  noteGainDB += velocityGainDB;
  noteGainLeft_ = noteGainRight_ = static_cast<float>(juce::Decibels::decibelsToGain(noteGainDB));
  // Region pan from SF2/SFZ is intentionally ignored - pan is sourced from MIDI
  // CC10 at the channel-mix stage in SFZeroAudioProcessor::processBlock.
  ampeg_.startNote(&region_->ampeg, floatVelocity, getSampleRate(), &region_->ampeg_veltrack);

  // Phase C - mod envelope (drives filter cutoff and/or pitch).
  modegPitchActive_ = (region_->modEnvToPitch != 0.0f);
  modegFilterActive_ = (region_->modEnvToFilterFc != 0.0f);
  modegInUse_ = modegPitchActive_ || modegFilterActive_;
  if (modegInUse_)
  {
    modeg_.startNote(&region_->modeg, floatVelocity, getSampleRate());
  }

  // Phase C - vibrato LFO (sine, with delayed onset). The LFO is silent until
  // vibDelaySamples_ counts down to zero; the phase advances regardless so the
  // sine waveform doesn't snap to zero phase when delay ends.
  vibInUse_ = (region_->vibLfoToPitch != 0.0f);
  if (vibInUse_)
  {
    const double sr = getSampleRate();
    const float vibFreqHz = sfzero::Region::absoluteCentsToHz(region_->freqVibLFO);
    constexpr float twoPi = 2.0f * juce::MathConstants<float>::pi;
    vibPhase_ = 0.0f;
    vibPhaseInc_ = twoPi * vibFreqHz / static_cast<float>(sr);
    const float delaySecs = sfzero::Region::timecents2Secs(static_cast<int>(region_->delayVibLFO));
    vibDelaySamples_ = static_cast<int>(delaySecs * sr);
    if (vibDelaySamples_ < 0) vibDelaySamples_ = 0;
  }

  // Phase C - initial filter LPF. Skip the filter entirely when the region's
  // cutoff is at/above the SF2 "no filter" default and no mod-env contribution
  // is configured; this keeps the cost off unfiltered GM patches.
  bypassFilter_ = (region_->initialFilterFc >= 13500.0f) && !modegFilterActive_;
  if (!bypassFilter_)
  {
    const double sr = getSampleRate();
    float cutoffHz = sfzero::Region::absoluteCentsToHz(region_->initialFilterFc);
    // Clamp to [30 Hz, 0.45 * sampleRate] to keep the biquad well-behaved near
    // Nyquist and at sub-audible cutoffs.
    const float maxHz = static_cast<float>(0.45 * sr);
    if (cutoffHz < 30.0f) cutoffHz = 30.0f;
    if (cutoffHz > maxHz) cutoffHz = maxHz;
    // SF2 initialFilterQ is centibels of resonance gain. Convert to a usable Q
    // value with a 0.7071 baseline; clamp to a sane range.
    float qLinear = 0.7071068f * sfzero::Region::centibelsToLinear(region_->initialFilterQ);
    if (qLinear < 0.5f) qLinear = 0.5f;
    if (qLinear > 8.0f) qLinear = 8.0f;
    currentCutoffHz_ = cutoffHz;
    currentQ_ = qLinear;
    auto coeffs = juce::IIRCoefficients::makeLowPass(sr, cutoffHz, qLinear);
    filterL_.setCoefficients(coeffs);
    filterR_.setCoefficients(coeffs);
    filterL_.reset();
    filterR_.reset();
  }

  // Offset/end.
  sourceSamplePosition_ = static_cast<double>(region_->offset);
  sampleEnd_ = region_->sample->getSampleLength();
  if ((region_->end > 0) && (region_->end < sampleEnd_))
  {
    sampleEnd_ = region_->end + 1;
  }

  // Loop.
  loopStart_ = loopEnd_ = 0;
  sfzero::Region::LoopMode loopMode = region_->loop_mode;
  if (loopMode == sfzero::Region::sample_loop)
  {
    if (region_->sample->getLoopStart() < region_->sample->getLoopEnd())
    {
      loopMode = sfzero::Region::loop_continuous;
    }
    else
    {
      loopMode = sfzero::Region::no_loop;
    }
  }
  if ((loopMode != sfzero::Region::no_loop) && (loopMode != sfzero::Region::one_shot))
  {
    if (region_->loop_start < region_->loop_end)
    {
      loopStart_ = region_->loop_start;
      loopEnd_ = region_->loop_end;
    }
    else
    {
      loopStart_ = region_->sample->getLoopStart();
      loopEnd_ = region_->sample->getLoopEnd();
    }
  }
  numLoops_ = 0;
}

void sfzero::Voice::stopNote(float /*velocity*/, bool allowTailOff)
{
  if (!allowTailOff || (region_ == nullptr))
  {
    killNote();
    return;
  }

  if (region_->loop_mode != sfzero::Region::one_shot)
  {
    ampeg_.noteOff();
    if (modegInUse_)
    {
      modeg_.noteOff();
    }
  }
  if (region_->loop_mode == sfzero::Region::loop_sustain)
  {
    // Continue playing, but stop looping.
    loopEnd_ = loopStart_;
  }
}

void sfzero::Voice::stopNoteForGroup()
{
  if (region_->off_mode == sfzero::Region::fast)
  {
    ampeg_.fastRelease();
    if (modegInUse_)
    {
      modeg_.fastRelease();
    }
  }
  else
  {
    ampeg_.noteOff();
    if (modegInUse_)
    {
      modeg_.noteOff();
    }
  }
}

void sfzero::Voice::stopNoteQuick()
{
  ampeg_.fastRelease();
  if (modegInUse_)
  {
    modeg_.fastRelease();
  }
}
void sfzero::Voice::pitchWheelMoved(int newValue)
{
  if (region_ == nullptr)
  {
    return;
  }

  curPitchWheel_ = newValue;
  calcPitchRatio();
}

void sfzero::Voice::controllerMoved(int /*controllerNumber*/, int /*newValue*/) { /***/}
void sfzero::Voice::renderNextBlock(juce::AudioSampleBuffer &outputBuffer, int startSample, int numSamples)
{
  if (region_ == nullptr)
  {
    return;
  }

  // Cached at startNote() — render path does not re-dereference region_->sample.
  const float *inL = inL_;
  const float *inR = inR_;
  const int bufferNumSamples = bufferNumSamples_;

  float *outL = outputBuffer.getWritePointer(0, startSample);
  float *outR = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer(1, startSample) : nullptr;

  // Cache some values, to give them at least some chance of ending up in
  // registers.
  double sourceSamplePosition = this->sourceSamplePosition_;
  float ampegGain = ampeg_.getLevel();
  float ampegSlope = ampeg_.getSlope();
  int samplesUntilNextAmpSegment = ampeg_.getSamplesUntilNextSegment();
  bool ampSegmentIsExponential = ampeg_.getSegmentIsExponential();
  const float loopStart = static_cast<float>(this->loopStart_);
  const float loopEnd = static_cast<float>(this->loopEnd_);
  const float sampleEnd = static_cast<float>(this->sampleEnd_);

  // Phase C - mod-env / vibrato state cached the same way ampeg is.
  // effectivePitchRatio is the pitch ratio after envelope/LFO modulation; we
  // update it at control rate (every kModUpdateRate samples) to keep filter
  // coefficient recomputes and pow()/sin() calls off the audio rate. The mod-
  // env level and the LFO phase still advance per sample so the modulation
  // sources have the right rate; only the *application* is throttled.
  const double sampleRate = getSampleRate();
  const float modEnvToPitch = region_->modEnvToPitch;
  const float modEnvToFilterFc = region_->modEnvToFilterFc;
  const float initialFilterFc = region_->initialFilterFc;
  const float vibLfoToPitch = region_->vibLfoToPitch;
  constexpr float twoPi = 2.0f * juce::MathConstants<float>::pi;
  const bool pitchModActive = modegPitchActive_ || vibInUse_;
  const bool needsControlRateUpdate = pitchModActive || modegFilterActive_;
  double effectivePitchRatio = pitchRatio_;
  float modegLevel = 0.0f, modegSlope = 0.0f;
  int samplesUntilNextModSegment = 0;
  bool modSegmentIsExponential = false;
  if (modegInUse_)
  {
    modegLevel = modeg_.getLevel();
    modegSlope = modeg_.getSlope();
    samplesUntilNextModSegment = modeg_.getSamplesUntilNextSegment();
    modSegmentIsExponential = modeg_.getSegmentIsExponential();
  }
  float vibPhase = vibPhase_;
  const float vibPhaseInc = vibPhaseInc_;
  int vibDelaySamples = vibDelaySamples_;
  static constexpr int kModUpdateRate = 32;
  int modUpdateCounter = 0;

  // Real-time per-sample render loop. The interpolation and output writes index
  // the cached audio pointers (inL/inR/outL/outR); this pointer arithmetic
  // (C26481) is bounded by bufferNumSamples / numSamples and must stay branch-
  // and throw-free on the audio thread, so .at()/span are not used here. The
  // C26481 checks are suppressed for the loop body.
#pragma warning(push)
#pragma warning(disable : 26481)
  while (--numSamples >= 0)
  {
    const int pos = static_cast<int>(sourceSamplePosition);
    const float alpha = static_cast<float>(sourceSamplePosition - pos);
    const float invAlpha = 1.0f - alpha;
    int nextPos = pos + 1;
    if ((loopStart < loopEnd) && (nextPos > loopEnd))
    {
      nextPos = static_cast<int>(loopStart);
    }

    // Simple linear interpolation with buffer overrun check
    const float nextL = nextPos < bufferNumSamples ? inL[nextPos] : inL[pos];
    const float nextR = inR ? (nextPos < bufferNumSamples ? inR[nextPos] : inR[pos]) : nextL;
    float l = (inL[pos] * invAlpha + nextL * alpha);
    float r = inR ? (inR[pos] * invAlpha + nextR * alpha) : l;

    //// Simple linear interpolation, old version (possible buffer overrun with non-loop??)
    // float l = (inL[pos] * invAlpha + inL[nextPos] * alpha);
    // float r = inR ? (inR[pos] * invAlpha + inR[nextPos] * alpha) : l;

    // Phase C - LPF (only when not bypassed). Applied before the volume gain
    // so the envelope still shapes the filtered signal. For mono sources r is
    // aliased to l above, so filter once and mirror to keep them coherent.
    if (!bypassFilter_)
    {
      l = filterL_.processSingleSampleRaw(l);
      if (inR)
      {
        r = filterR_.processSingleSampleRaw(r);
      }
      else
      {
        r = l;
      }
    }

    const float gainLeft = noteGainLeft_ * ampegGain;
    const float gainRight = noteGainRight_ * ampegGain;
    l *= gainLeft;
    r *= gainRight;
    // Shouldn't we dither here?

    if (outR)
    {
      *outL++ += l;
      *outR++ += r;
    }
    else
    {
      *outL++ += (l + r) * 0.5f;
    }

    // Next sample.
    sourceSamplePosition += effectivePitchRatio;
    if ((loopStart < loopEnd) && (sourceSamplePosition > loopEnd))
    {
      sourceSamplePosition = loopStart;
      numLoops_ += 1;
    }

    // Update EG.
    if (ampSegmentIsExponential)
    {
      ampegGain *= ampegSlope;
    }
    else
    {
      ampegGain += ampegSlope;
    }
    if (--samplesUntilNextAmpSegment < 0)
    {
      ampeg_.setLevel(ampegGain);
      ampeg_.nextSegment();
      ampegGain = ampeg_.getLevel();
      ampegSlope = ampeg_.getSlope();
      samplesUntilNextAmpSegment = ampeg_.getSamplesUntilNextSegment();
      ampSegmentIsExponential = ampeg_.getSegmentIsExponential();
    }

    // Phase C - mod-env / vibrato update + control-rate modulation. Sample-rate
    // updates (level, phase) are cheap (one mul/add); the expensive bits (pow
    // for pitch, sin for LFO output, biquad coefficient recompute) are
    // throttled to every kModUpdateRate samples.
    if (modegInUse_)
    {
      if (modSegmentIsExponential)
      {
        modegLevel *= modegSlope;
      }
      else
      {
        modegLevel += modegSlope;
      }
      if (--samplesUntilNextModSegment < 0)
      {
        modeg_.setLevel(modegLevel);
        modeg_.nextSegment();
        modegLevel = modeg_.getLevel();
        modegSlope = modeg_.getSlope();
        samplesUntilNextModSegment = modeg_.getSamplesUntilNextSegment();
        modSegmentIsExponential = modeg_.getSegmentIsExponential();
      }
    }

    if (vibInUse_)
    {
      vibPhase += vibPhaseInc;
      if (vibPhase > twoPi) vibPhase -= twoPi;
      if (vibDelaySamples > 0) --vibDelaySamples;
    }

    if (needsControlRateUpdate && --modUpdateCounter < 0)
    {
      modUpdateCounter = kModUpdateRate;

      if (pitchModActive)
      {
        float pitchModCents = 0.0f;
        if (modegPitchActive_)
        {
          pitchModCents += modegLevel * modEnvToPitch;
        }
        if (vibInUse_ && vibDelaySamples == 0)
        {
          pitchModCents += std::sin(vibPhase) * vibLfoToPitch;
        }
        effectivePitchRatio = pitchRatio_ * pow(2.0, pitchModCents / 1200.0);
      }
      if (modegFilterActive_)
      {
        float fcCents = initialFilterFc + modegLevel * modEnvToFilterFc;
        if (fcCents < 1500.0f) fcCents = 1500.0f;
        if (fcCents > 13500.0f) fcCents = 13500.0f;
        float fcHz = sfzero::Region::absoluteCentsToHz(fcCents);
        const float maxHz = static_cast<float>(0.45 * sampleRate);
        if (fcHz < 30.0f) fcHz = 30.0f;
        if (fcHz > maxHz) fcHz = maxHz;
        if (fcHz != currentCutoffHz_)
        {
          currentCutoffHz_ = fcHz;
          auto coeffs = juce::IIRCoefficients::makeLowPass(sampleRate, fcHz, currentQ_);
          filterL_.setCoefficients(coeffs);
          filterR_.setCoefficients(coeffs);
        }
      }
    }

    if ((sourceSamplePosition >= sampleEnd) || ampeg_.isDone())
    {
      killNote();
      break;
    }
  }
#pragma warning(pop)

  this->sourceSamplePosition_ = sourceSamplePosition;
  ampeg_.setLevel(ampegGain);
  ampeg_.setSamplesUntilNextSegment(samplesUntilNextAmpSegment);
  if (modegInUse_)
  {
    modeg_.setLevel(modegLevel);
    modeg_.setSamplesUntilNextSegment(samplesUntilNextModSegment);
  }
  if (vibInUse_)
  {
    vibPhase_ = vibPhase;
    vibDelaySamples_ = vibDelaySamples;
  }
}

bool sfzero::Voice::isPlayingNoteDown() const noexcept { return region_ && region_->trigger != sfzero::Region::release; }

bool sfzero::Voice::isPlayingOneShot() const noexcept { return region_ && region_->loop_mode == sfzero::Region::one_shot; }

int sfzero::Voice::getGroup() const noexcept { return region_ ? region_->group : 0; }

juce::uint64 sfzero::Voice::getOffBy() const noexcept { return region_ ? region_->off_by : 0; }

void sfzero::Voice::setRegion(sfzero::Region *nextRegion) noexcept { region_ = nextRegion; }

juce::String sfzero::Voice::infoString()
{
  juce::String info;
  info << "note: " << curMidiNote_ << ", vel: " << curVelocity_ << ", pan: " << region_->pan
       << ", eg: " << ampeg_.segmentName() << ", loops: " << numLoops_;
  return info;
}

void sfzero::Voice::calcPitchRatio()
{
  double note = curMidiNote_;

  note += region_->transpose;
  note += region_->tune / 100.0;

  double adjustedPitch = region_->pitch_keycenter + (note - region_->pitch_keycenter) * (region_->pitch_keytrack / 100.0);
  if (curPitchWheel_ != 8192)
  {
    const double wheel = ((2.0 * curPitchWheel_ / 16383.0) - 1.0);
    if (wheel > 0)
    {
      adjustedPitch += wheel * region_->bend_up / 100.0;
    }
    else
    {
      adjustedPitch += wheel * region_->bend_down / -100.0;
    }
  }
  const double targetFreq = fractionalMidiNoteInHz(adjustedPitch);
  const double naturalFreq = juce::MidiMessage::getMidiNoteInHertz(region_->pitch_keycenter);
  pitchRatio_ = (targetFreq * region_->sample->getSampleRate()) / (naturalFreq * getSampleRate());
}

void sfzero::Voice::killNote()
{
  region_ = nullptr;
  bufferKeepAlive_.reset();
  inL_ = nullptr;
  inR_ = nullptr;
  bufferNumSamples_ = 0;
  clearCurrentNote();
}

double sfzero::Voice::fractionalMidiNoteInHz(double note, double freqOfA) noexcept
{
  // Like MidiMessage::getMidiNoteInHertz(), but with a float note.
  note -= 69;
  // Now 0 = A
  return freqOfA * pow(2.0, note / 12.0);
}
