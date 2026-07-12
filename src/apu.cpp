#include "apu.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr uint8_t kDutyTable[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
    {1, 0, 0, 0, 0, 0, 0, 1}, // 25%
    {1, 0, 0, 0, 0, 1, 1, 1}, // 50%
    {0, 1, 1, 1, 1, 1, 1, 0}, // 75%
};

constexpr uint8_t kNoiseDivisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};

constexpr uint16_t kDivFsBit = 1u << 12;

void push16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
bool read16(const uint8_t*& ptr, const uint8_t* end, uint16_t& v) {
    if (end - ptr < 2) return false;
    v = static_cast<uint16_t>(ptr[0] | (ptr[1] << 8));
    ptr += 2;
    return true;
}
void push32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
}
bool read32(const uint8_t*& ptr, const uint8_t* end, uint32_t& v) {
    if (end - ptr < 4) return false;
    v = static_cast<uint32_t>(ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24));
    ptr += 4;
    return true;
}
void push8(std::vector<uint8_t>& out, uint8_t v) { out.push_back(v); }
bool read8(const uint8_t*& ptr, const uint8_t* end, uint8_t& v) {
    if (end - ptr < 1) return false;
    v = *ptr++;
    return true;
}
void pushBool(std::vector<uint8_t>& out, bool v) { out.push_back(v ? 1 : 0); }
bool readBool(const uint8_t*& ptr, const uint8_t* end, bool& v) {
    if (end - ptr < 1) return false;
    v = (*ptr++ != 0);
    return true;
}
} // namespace

// ---------- Square ----------
uint32_t APU::SquareChannel::period() const {
    return static_cast<uint32_t>((2048u - (frequency & 0x7FFu)) * 4u);
}

uint16_t APU::SquareChannel::calcSweep() const {
    const uint16_t delta = shadowFreq >> sweepShift;
    if (sweepDecrease) return static_cast<uint16_t>(shadowFreq - delta);
    return static_cast<uint16_t>(shadowFreq + delta);
}

void APU::SquareChannel::trigger(bool isCh1) {
    enabled = dacEnabled;
    freqTimer = period();
    if (freqTimer == 0) freqTimer = 4;
    dutyStep = 0;
    envelopeTimer = envelopePeriod ? envelopePeriod : 8;
    envelopeRunning = true;
    volume = (nr2 >> 4) & 0x0F;
    if (length == 0) length = 64;

    if (isCh1 && hasSweep) {
        shadowFreq = frequency;
        sweepTimer = sweepPeriod ? sweepPeriod : 8;
        sweepEnabled = (sweepPeriod != 0) || (sweepShift != 0);
        sweepNegateUsed = false;
        if (sweepShift != 0) {
            const uint16_t f = calcSweep();
            if (sweepDecrease) sweepNegateUsed = true;
            if (f > 0x7FF) enabled = false;
        }
    }
}

void APU::SquareChannel::clockLength() {
    if (lengthEnabled && length > 0) {
        --length;
        if (length == 0) enabled = false;
    }
}

void APU::SquareChannel::clockEnvelope() {
    if (!envelopeRunning || envelopePeriod == 0) return;
    if (envelopeTimer > 0) --envelopeTimer;
    if (envelopeTimer == 0) {
        envelopeTimer = envelopePeriod ? envelopePeriod : 8;
        if (envelopeIncrease) {
            if (volume < 15) ++volume;
            else envelopeRunning = false;
        } else {
            if (volume > 0) --volume;
            else envelopeRunning = false;
        }
    }
}

void APU::SquareChannel::clockSweep() {
    if (!hasSweep || !sweepEnabled) return;
    if (sweepTimer > 0) --sweepTimer;
    if (sweepTimer != 0) return;

    sweepTimer = sweepPeriod ? sweepPeriod : 8;
    if (sweepPeriod == 0) return;

    const uint16_t newFreq = calcSweep();
    if (sweepDecrease) sweepNegateUsed = true;
    if (newFreq > 0x7FF) {
        enabled = false;
        return;
    }
    if (sweepShift != 0) {
        shadowFreq = newFreq;
        frequency = newFreq & 0x7FF;
        if (calcSweep() > 0x7FF) enabled = false;
    }
}

uint8_t APU::SquareChannel::digitalOutput() const {
    if (!enabled || !dacEnabled) return 0;
    return kDutyTable[duty & 3][dutyStep & 7] ? volume : 0;
}

void APU::SquareChannel::tickTimer() {
    if (!enabled) return;
    if (freqTimer > 0) --freqTimer;
    if (freqTimer == 0) {
        freqTimer = period();
        if (freqTimer == 0) freqTimer = 4;
        dutyStep = static_cast<uint8_t>((dutyStep + 1) & 7);
    }
}

// ---------- Wave ----------
uint32_t APU::WaveChannel::period() const {
    return static_cast<uint32_t>((2048u - (frequency & 0x7FFu)) * 2u);
}

void APU::WaveChannel::trigger() {
    enabled = dacEnabled;
    freqTimer = period();
    if (freqTimer == 0) freqTimer = 2;
    position = 0;
    currentSample = (waveRam[0] >> 4) & 0x0F;
    if (length == 0) length = 256;
}

void APU::WaveChannel::clockLength() {
    if (lengthEnabled && length > 0) {
        --length;
        if (length == 0) enabled = false;
    }
}

uint8_t APU::WaveChannel::digitalOutput() const {
    if (!enabled || !dacEnabled || volumeCode == 0) return 0;
    uint8_t shift = 0;
    switch (volumeCode) {
        case 1: shift = 0; break;
        case 2: shift = 1; break;
        case 3: shift = 2; break;
        default: return 0;
    }
    return static_cast<uint8_t>(currentSample >> shift);
}

void APU::WaveChannel::tickTimer() {
    if (!enabled) return;
    if (freqTimer > 0) --freqTimer;
    if (freqTimer == 0) {
        freqTimer = period();
        if (freqTimer == 0) freqTimer = 2;
        position = static_cast<uint8_t>((position + 1) & 31);
        const uint8_t byte = waveRam[position >> 1];
        currentSample = (position & 1) ? (byte & 0x0F) : static_cast<uint8_t>((byte >> 4) & 0x0F);
    }
}

// ---------- Noise ----------
uint32_t APU::NoiseChannel::period() const {
    const uint32_t div = kNoiseDivisors[divisorCode & 7];
    const uint32_t shift = clockShift & 0x0F;
    return div << shift;
}

void APU::NoiseChannel::trigger() {
    enabled = dacEnabled;
    lfsr = 0x7FFF;
    envelopeTimer = envelopePeriod ? envelopePeriod : 8;
    envelopeRunning = true;
    volume = (nr2 >> 4) & 0x0F;
    if (length == 0) length = 64;
    freqTimer = period();
    if (freqTimer == 0) freqTimer = 8;
}

void APU::NoiseChannel::clockLength() {
    if (lengthEnabled && length > 0) {
        --length;
        if (length == 0) enabled = false;
    }
}

void APU::NoiseChannel::clockEnvelope() {
    if (!envelopeRunning || envelopePeriod == 0) return;
    if (envelopeTimer > 0) --envelopeTimer;
    if (envelopeTimer == 0) {
        envelopeTimer = envelopePeriod ? envelopePeriod : 8;
        if (envelopeIncrease) {
            if (volume < 15) ++volume;
            else envelopeRunning = false;
        } else {
            if (volume > 0) --volume;
            else envelopeRunning = false;
        }
    }
}

uint8_t APU::NoiseChannel::digitalOutput() const {
    if (!enabled || !dacEnabled) return 0;
    return ((~lfsr) & 1) ? volume : 0;
}

void APU::NoiseChannel::tickTimer() {
    if (!enabled) return;
    if (freqTimer > 0) --freqTimer;
    if (freqTimer == 0) {
        freqTimer = period();
        if (freqTimer == 0) freqTimer = 8;
        const uint8_t xorBit = static_cast<uint8_t>((lfsr ^ (lfsr >> 1)) & 1);
        lfsr = static_cast<uint16_t>((lfsr >> 1) | (static_cast<uint16_t>(xorBit) << 14));
        if (widthMode) {
            lfsr = static_cast<uint16_t>((lfsr & ~static_cast<uint16_t>(0x40)) |
                                         (static_cast<uint16_t>(xorBit) << 6));
        }
    }
}

// ---------- APU ----------
APU::APU() {
    m_ch1.hasSweep = true;
    m_ring.assign(kRingCapacity * 2, 0);
    reset();
}

void APU::setVolume(float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);
}

void APU::reset() {
    m_ch1 = SquareChannel{};
    m_ch1.hasSweep = true;
    m_ch2 = SquareChannel{};
    m_ch3 = WaveChannel{};
    m_ch4 = NoiseChannel{};
    m_nr50 = 0x77;
    m_nr51 = 0xF3;
    m_nr52 = 0xF1;
    m_outputEnabled = true;
    m_frameSeqStep = 0;
    m_sampleTimer = 0;
    m_capLeft = 0;
    m_capRight = 0;
    clearSampleBuffer();
}

void APU::clearSampleBuffer() {
    m_ringRead = m_ringWrite = m_ringFrames = 0;
    if (m_ring.size() != kRingCapacity * 2) {
        m_ring.assign(kRingCapacity * 2, 0);
    }
}

size_t APU::samplesAvailable() const {
    return m_ringFrames;
}

void APU::pushSample(int16_t l, int16_t r) {
    if (m_ringFrames >= kRingCapacity) {
        m_ringRead = (m_ringRead + 1) % kRingCapacity;
        --m_ringFrames;
    }
    const size_t i = m_ringWrite * 2;
    m_ring[i] = l;
    m_ring[i + 1] = r;
    m_ringWrite = (m_ringWrite + 1) % kRingCapacity;
    ++m_ringFrames;
}

void APU::powerOff() {
    for (uint16_t a = 0xFF10; a <= 0xFF25; ++a) {
        writeRegister(a, 0);
    }
    m_ch1.enabled = m_ch2.enabled = m_ch3.enabled = m_ch4.enabled = false;
    m_ch1.dacEnabled = m_ch2.dacEnabled = m_ch3.dacEnabled = m_ch4.dacEnabled = false;
}

void APU::updateNr52Status() {
    m_nr52 = static_cast<uint8_t>(m_nr52 & 0x80);
    if (m_ch1.enabled) m_nr52 |= 0x01;
    if (m_ch2.enabled) m_nr52 |= 0x02;
    if (m_ch3.enabled) m_nr52 |= 0x04;
    if (m_ch4.enabled) m_nr52 |= 0x08;
}

void APU::clockFrameSequencer() {
    switch (m_frameSeqStep) {
        case 0:
        case 2:
        case 4:
        case 6:
            m_ch1.clockLength();
            m_ch2.clockLength();
            m_ch3.clockLength();
            m_ch4.clockLength();
            if (m_frameSeqStep == 2 || m_frameSeqStep == 6) m_ch1.clockSweep();
            break;
        case 7:
            m_ch1.clockEnvelope();
            m_ch2.clockEnvelope();
            m_ch4.clockEnvelope();
            break;
        default:
            break;
    }
    m_frameSeqStep = static_cast<uint8_t>((m_frameSeqStep + 1) & 7);
    updateNr52Status();
}

void APU::maybeExtraLengthClock(bool wasLengthEnabled, bool nowLengthEnabled, SquareChannel& ch) {
    if (!wasLengthEnabled && nowLengthEnabled && (m_frameSeqStep & 1) != 0) ch.clockLength();
}
void APU::maybeExtraLengthClock(bool wasLengthEnabled, bool nowLengthEnabled, WaveChannel& ch) {
    if (!wasLengthEnabled && nowLengthEnabled && (m_frameSeqStep & 1) != 0) ch.clockLength();
}
void APU::maybeExtraLengthClock(bool wasLengthEnabled, bool nowLengthEnabled, NoiseChannel& ch) {
    if (!wasLengthEnabled && nowLengthEnabled && (m_frameSeqStep & 1) != 0) ch.clockLength();
}

void APU::writeRegister(uint16_t address, uint8_t value) {
    if (address == 0xFF26) {
        const bool wasOn = (m_nr52 & 0x80) != 0;
        const bool nowOn = (value & 0x80) != 0;
        if (wasOn && !nowOn) {
            powerOff();
            m_nr52 = 0x00;
        } else if (!wasOn && nowOn) {
            m_nr52 = 0x80;
            m_frameSeqStep = 0;
        } else {
            m_nr52 = static_cast<uint8_t>((m_nr52 & 0x7F) | (value & 0x80));
        }
        updateNr52Status();
        return;
    }

    if (address >= 0xFF30 && address <= 0xFF3F) {
        if (m_ch3.enabled) m_ch3.waveRam[m_ch3.position >> 1] = value;
        else m_ch3.waveRam[address - 0xFF30] = value;
        return;
    }

    if (!(m_nr52 & 0x80)) return;

    switch (address) {
        case 0xFF10:
            m_ch1.nr0 = value;
            m_ch1.sweepPeriod = (value >> 4) & 0x07;
            m_ch1.sweepDecrease = (value & 0x08) != 0;
            m_ch1.sweepShift = value & 0x07;
            if (!m_ch1.sweepDecrease && m_ch1.sweepNegateUsed) m_ch1.enabled = false;
            break;
        case 0xFF11:
            m_ch1.nr1 = value;
            m_ch1.duty = (value >> 6) & 0x03;
            m_ch1.length = static_cast<uint8_t>(64 - (value & 0x3F));
            break;
        case 0xFF12:
            m_ch1.nr2 = value;
            m_ch1.envelopeIncrease = (value & 0x08) != 0;
            m_ch1.envelopePeriod = value & 0x07;
            m_ch1.dacEnabled = (value & 0xF8) != 0;
            if (!m_ch1.dacEnabled) m_ch1.enabled = false;
            break;
        case 0xFF13:
            m_ch1.nr3 = value;
            m_ch1.frequency = static_cast<uint16_t>((m_ch1.frequency & 0x700) | value);
            break;
        case 0xFF14: {
            const bool wasLen = m_ch1.lengthEnabled;
            m_ch1.nr4 = value;
            m_ch1.frequency = static_cast<uint16_t>((m_ch1.frequency & 0xFF) | ((value & 0x07) << 8));
            m_ch1.lengthEnabled = (value & 0x40) != 0;
            maybeExtraLengthClock(wasLen, m_ch1.lengthEnabled, m_ch1);
            if (value & 0x80) m_ch1.trigger(true);
            break;
        }
        case 0xFF16:
            m_ch2.nr1 = value;
            m_ch2.duty = (value >> 6) & 0x03;
            m_ch2.length = static_cast<uint8_t>(64 - (value & 0x3F));
            break;
        case 0xFF17:
            m_ch2.nr2 = value;
            m_ch2.envelopeIncrease = (value & 0x08) != 0;
            m_ch2.envelopePeriod = value & 0x07;
            m_ch2.dacEnabled = (value & 0xF8) != 0;
            if (!m_ch2.dacEnabled) m_ch2.enabled = false;
            break;
        case 0xFF18:
            m_ch2.nr3 = value;
            m_ch2.frequency = static_cast<uint16_t>((m_ch2.frequency & 0x700) | value);
            break;
        case 0xFF19: {
            const bool wasLen = m_ch2.lengthEnabled;
            m_ch2.nr4 = value;
            m_ch2.frequency = static_cast<uint16_t>((m_ch2.frequency & 0xFF) | ((value & 0x07) << 8));
            m_ch2.lengthEnabled = (value & 0x40) != 0;
            maybeExtraLengthClock(wasLen, m_ch2.lengthEnabled, m_ch2);
            if (value & 0x80) m_ch2.trigger(false);
            break;
        }
        case 0xFF1A:
            m_ch3.nr0 = value;
            m_ch3.dacEnabled = (value & 0x80) != 0;
            if (!m_ch3.dacEnabled) m_ch3.enabled = false;
            break;
        case 0xFF1B:
            m_ch3.nr1 = value;
            m_ch3.length = static_cast<uint16_t>(256 - value);
            break;
        case 0xFF1C:
            m_ch3.nr2 = value;
            m_ch3.volumeCode = (value >> 5) & 0x03;
            break;
        case 0xFF1D:
            m_ch3.nr3 = value;
            m_ch3.frequency = static_cast<uint16_t>((m_ch3.frequency & 0x700) | value);
            break;
        case 0xFF1E: {
            const bool wasLen = m_ch3.lengthEnabled;
            m_ch3.nr4 = value;
            m_ch3.frequency = static_cast<uint16_t>((m_ch3.frequency & 0xFF) | ((value & 0x07) << 8));
            m_ch3.lengthEnabled = (value & 0x40) != 0;
            maybeExtraLengthClock(wasLen, m_ch3.lengthEnabled, m_ch3);
            if (value & 0x80) m_ch3.trigger();
            break;
        }
        case 0xFF20:
            m_ch4.nr1 = value;
            m_ch4.length = static_cast<uint8_t>(64 - (value & 0x3F));
            break;
        case 0xFF21:
            m_ch4.nr2 = value;
            m_ch4.envelopeIncrease = (value & 0x08) != 0;
            m_ch4.envelopePeriod = value & 0x07;
            m_ch4.dacEnabled = (value & 0xF8) != 0;
            if (!m_ch4.dacEnabled) m_ch4.enabled = false;
            break;
        case 0xFF22:
            m_ch4.nr3 = value;
            m_ch4.clockShift = (value >> 4) & 0x0F;
            m_ch4.widthMode = (value & 0x08) != 0;
            m_ch4.divisorCode = value & 0x07;
            break;
        case 0xFF23: {
            const bool wasLen = m_ch4.lengthEnabled;
            m_ch4.nr4 = value;
            m_ch4.lengthEnabled = (value & 0x40) != 0;
            maybeExtraLengthClock(wasLen, m_ch4.lengthEnabled, m_ch4);
            if (value & 0x80) m_ch4.trigger();
            break;
        }
        case 0xFF24: m_nr50 = value; break;
        case 0xFF25: m_nr51 = value; break;
        default: break;
    }
    updateNr52Status();
}

uint8_t APU::readRegister(uint16_t address) const {
    auto orMask = [](uint8_t v, uint8_t mask) { return static_cast<uint8_t>(v | mask); };
    switch (address) {
        case 0xFF10: return orMask(m_ch1.nr0, 0x80);
        case 0xFF11: return orMask(m_ch1.nr1, 0x3F);
        case 0xFF12: return m_ch1.nr2;
        case 0xFF13: return 0xFF;
        case 0xFF14: return orMask(static_cast<uint8_t>(m_ch1.nr4 & 0x40), 0xBF);
        case 0xFF16: return orMask(m_ch2.nr1, 0x3F);
        case 0xFF17: return m_ch2.nr2;
        case 0xFF18: return 0xFF;
        case 0xFF19: return orMask(static_cast<uint8_t>(m_ch2.nr4 & 0x40), 0xBF);
        case 0xFF1A: return orMask(m_ch3.nr0, 0x7F);
        case 0xFF1B: return 0xFF;
        case 0xFF1C: return orMask(m_ch3.nr2, 0x9F);
        case 0xFF1D: return 0xFF;
        case 0xFF1E: return orMask(static_cast<uint8_t>(m_ch3.nr4 & 0x40), 0xBF);
        case 0xFF20: return 0xFF;
        case 0xFF21: return m_ch4.nr2;
        case 0xFF22: return m_ch4.nr3;
        case 0xFF23: return orMask(static_cast<uint8_t>(m_ch4.nr4 & 0x40), 0xBF);
        case 0xFF24: return m_nr50;
        case 0xFF25: return m_nr51;
        case 0xFF26: {
            uint8_t v = static_cast<uint8_t>((m_nr52 & 0x80) | 0x70);
            if (m_ch1.enabled) v |= 0x01;
            if (m_ch2.enabled) v |= 0x02;
            if (m_ch3.enabled) v |= 0x04;
            if (m_ch4.enabled) v |= 0x08;
            return v;
        }
        default:
            if (address >= 0xFF30 && address <= 0xFF3F) {
                if (m_ch3.enabled) return m_ch3.waveRam[m_ch3.position >> 1];
                return m_ch3.waveRam[address - 0xFF30];
            }
            return 0xFF;
    }
}

void APU::mixSample() {
    if (!m_outputEnabled || !(m_nr52 & 0x80)) {
        const double outL = 0.0 - m_capLeft;
        const double outR = 0.0 - m_capRight;
        m_capLeft = 0.0 - outL * kCapCharge;
        m_capRight = 0.0 - outR * kCapCharge;
        pushSample(0, 0);
        return;
    }

    const int s1 = m_ch1.digitalOutput();
    const int s2 = m_ch2.digitalOutput();
    const int s3 = m_ch3.digitalOutput();
    const int s4 = m_ch4.digitalOutput();

    int left = 0;
    int right = 0;
    if (m_nr51 & 0x10) left += s1;
    if (m_nr51 & 0x01) right += s1;
    if (m_nr51 & 0x20) left += s2;
    if (m_nr51 & 0x02) right += s2;
    if (m_nr51 & 0x40) left += s3;
    if (m_nr51 & 0x04) right += s3;
    if (m_nr51 & 0x80) left += s4;
    if (m_nr51 & 0x08) right += s4;

    const int leftVol = ((m_nr50 >> 4) & 0x07) + 1;
    const int rightVol = (m_nr50 & 0x07) + 1;

    constexpr double kScale = 1.0 / 480.0;
    const double inL = static_cast<double>(left * leftVol) * kScale;
    const double inR = static_cast<double>(right * rightVol) * kScale;

    const double hpL = inL - m_capLeft;
    const double hpR = inR - m_capRight;
    m_capLeft = inL - hpL * kCapCharge;
    m_capRight = inR - hpR * kCapCharge;

    const double g = static_cast<double>(m_volume) * 2.2;
    const double outL = std::clamp(hpL * g, -1.0, 1.0);
    const double outR = std::clamp(hpR * g, -1.0, 1.0);

    pushSample(static_cast<int16_t>(outL * 24000.0),
               static_cast<int16_t>(outR * 24000.0));
}

void APU::tickTCycle(uint16_t divBefore, uint16_t divAfter) {
    if ((m_nr52 & 0x80) != 0) {
        const bool oldBit = (divBefore & kDivFsBit) != 0;
        const bool newBit = (divAfter & kDivFsBit) != 0;
        if (oldBit && !newBit) clockFrameSequencer();

        m_ch1.tickTimer();
        m_ch2.tickTimer();
        m_ch3.tickTimer();
        m_ch4.tickTimer();
    }

    m_sampleTimer += 1.0;
    while (m_sampleTimer >= kCyclesPerSample) {
        m_sampleTimer -= kCyclesPerSample;
        mixSample();
    }
}

void APU::onDivReset(uint16_t divBefore) {
    if ((m_nr52 & 0x80) != 0 && (divBefore & kDivFsBit) != 0) {
        clockFrameSequencer();
    }
}

size_t APU::popSamples(int16_t* out, size_t maxFrames) {
    const size_t n = std::min(maxFrames, m_ringFrames);
    for (size_t f = 0; f < n; ++f) {
        const size_t i = m_ringRead * 2;
        out[f * 2] = m_ring[i];
        out[f * 2 + 1] = m_ring[i + 1];
        m_ringRead = (m_ringRead + 1) % kRingCapacity;
    }
    m_ringFrames -= n;
    return n;
}

void APU::serialize(std::vector<uint8_t>& out) const {
    auto serSq = [&](const SquareChannel& c) {
        pushBool(out, c.enabled);
        pushBool(out, c.dacEnabled);
        push8(out, c.duty);
        push8(out, c.length);
        pushBool(out, c.lengthEnabled);
        push8(out, c.volume);
        push8(out, c.envelopePeriod);
        push8(out, c.envelopeTimer);
        pushBool(out, c.envelopeIncrease);
        pushBool(out, c.envelopeRunning);
        push16(out, c.frequency);
        push32(out, c.freqTimer);
        push8(out, c.dutyStep);
        push8(out, c.nr0); push8(out, c.nr1); push8(out, c.nr2);
        push8(out, c.nr3); push8(out, c.nr4);
        pushBool(out, c.hasSweep);
        push8(out, c.sweepPeriod);
        push8(out, c.sweepTimer);
        pushBool(out, c.sweepDecrease);
        push8(out, c.sweepShift);
        push16(out, c.shadowFreq);
        pushBool(out, c.sweepEnabled);
        pushBool(out, c.sweepNegateUsed);
    };
    serSq(m_ch1);
    serSq(m_ch2);

    pushBool(out, m_ch3.enabled);
    pushBool(out, m_ch3.dacEnabled);
    push16(out, m_ch3.length);
    pushBool(out, m_ch3.lengthEnabled);
    push8(out, m_ch3.volumeCode);
    push16(out, m_ch3.frequency);
    push32(out, m_ch3.freqTimer);
    push8(out, m_ch3.position);
    out.insert(out.end(), m_ch3.waveRam, m_ch3.waveRam + 16);
    push8(out, m_ch3.nr0); push8(out, m_ch3.nr1); push8(out, m_ch3.nr2);
    push8(out, m_ch3.nr3); push8(out, m_ch3.nr4);
    push8(out, m_ch3.currentSample);

    pushBool(out, m_ch4.enabled);
    pushBool(out, m_ch4.dacEnabled);
    push8(out, m_ch4.length);
    pushBool(out, m_ch4.lengthEnabled);
    push8(out, m_ch4.volume);
    push8(out, m_ch4.envelopePeriod);
    push8(out, m_ch4.envelopeTimer);
    pushBool(out, m_ch4.envelopeIncrease);
    pushBool(out, m_ch4.envelopeRunning);
    push8(out, m_ch4.clockShift);
    pushBool(out, m_ch4.widthMode);
    push8(out, m_ch4.divisorCode);
    push16(out, m_ch4.lfsr);
    push32(out, m_ch4.freqTimer);
    push8(out, m_ch4.nr1); push8(out, m_ch4.nr2);
    push8(out, m_ch4.nr3); push8(out, m_ch4.nr4);

    push8(out, m_nr50);
    push8(out, m_nr51);
    push8(out, m_nr52);
    pushBool(out, m_outputEnabled);
    push8(out, m_frameSeqStep);

    uint64_t st = 0, hl = 0, hr = 0;
    std::memcpy(&st, &m_sampleTimer, 8);
    std::memcpy(&hl, &m_capLeft, 8);
    std::memcpy(&hr, &m_capRight, 8);
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>((st >> (i * 8)) & 0xFF));
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>((hl >> (i * 8)) & 0xFF));
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<uint8_t>((hr >> (i * 8)) & 0xFF));
}

bool APU::deserialize(const uint8_t*& ptr, const uint8_t* end) {
    auto deSq = [&](SquareChannel& c) -> bool {
        if (!readBool(ptr, end, c.enabled)) return false;
        if (!readBool(ptr, end, c.dacEnabled)) return false;
        if (!read8(ptr, end, c.duty)) return false;
        if (!read8(ptr, end, c.length)) return false;
        if (!readBool(ptr, end, c.lengthEnabled)) return false;
        if (!read8(ptr, end, c.volume)) return false;
        if (!read8(ptr, end, c.envelopePeriod)) return false;
        if (!read8(ptr, end, c.envelopeTimer)) return false;
        if (!readBool(ptr, end, c.envelopeIncrease)) return false;
        if (!readBool(ptr, end, c.envelopeRunning)) return false;
        if (!read16(ptr, end, c.frequency)) return false;
        if (!read32(ptr, end, c.freqTimer)) return false;
        if (!read8(ptr, end, c.dutyStep)) return false;
        if (!read8(ptr, end, c.nr0) || !read8(ptr, end, c.nr1) || !read8(ptr, end, c.nr2)) return false;
        if (!read8(ptr, end, c.nr3) || !read8(ptr, end, c.nr4)) return false;
        if (!readBool(ptr, end, c.hasSweep)) return false;
        if (!read8(ptr, end, c.sweepPeriod)) return false;
        if (!read8(ptr, end, c.sweepTimer)) return false;
        if (!readBool(ptr, end, c.sweepDecrease)) return false;
        if (!read8(ptr, end, c.sweepShift)) return false;
        if (!read16(ptr, end, c.shadowFreq)) return false;
        if (!readBool(ptr, end, c.sweepEnabled)) return false;
        if (!readBool(ptr, end, c.sweepNegateUsed)) return false;
        return true;
    };
    if (!deSq(m_ch1) || !deSq(m_ch2)) return false;
    m_ch1.hasSweep = true;
    m_ch2.hasSweep = false;

    if (!readBool(ptr, end, m_ch3.enabled)) return false;
    if (!readBool(ptr, end, m_ch3.dacEnabled)) return false;
    if (!read16(ptr, end, m_ch3.length)) return false;
    if (!readBool(ptr, end, m_ch3.lengthEnabled)) return false;
    if (!read8(ptr, end, m_ch3.volumeCode)) return false;
    if (!read16(ptr, end, m_ch3.frequency)) return false;
    if (!read32(ptr, end, m_ch3.freqTimer)) return false;
    if (!read8(ptr, end, m_ch3.position)) return false;
    if (end - ptr < 16) return false;
    std::memcpy(m_ch3.waveRam, ptr, 16);
    ptr += 16;
    if (!read8(ptr, end, m_ch3.nr0) || !read8(ptr, end, m_ch3.nr1) || !read8(ptr, end, m_ch3.nr2)) return false;
    if (!read8(ptr, end, m_ch3.nr3) || !read8(ptr, end, m_ch3.nr4)) return false;
    if (!read8(ptr, end, m_ch3.currentSample)) return false;

    if (!readBool(ptr, end, m_ch4.enabled)) return false;
    if (!readBool(ptr, end, m_ch4.dacEnabled)) return false;
    if (!read8(ptr, end, m_ch4.length)) return false;
    if (!readBool(ptr, end, m_ch4.lengthEnabled)) return false;
    if (!read8(ptr, end, m_ch4.volume)) return false;
    if (!read8(ptr, end, m_ch4.envelopePeriod)) return false;
    if (!read8(ptr, end, m_ch4.envelopeTimer)) return false;
    if (!readBool(ptr, end, m_ch4.envelopeIncrease)) return false;
    if (!readBool(ptr, end, m_ch4.envelopeRunning)) return false;
    if (!read8(ptr, end, m_ch4.clockShift)) return false;
    if (!readBool(ptr, end, m_ch4.widthMode)) return false;
    if (!read8(ptr, end, m_ch4.divisorCode)) return false;
    if (!read16(ptr, end, m_ch4.lfsr)) return false;
    if (!read32(ptr, end, m_ch4.freqTimer)) return false;
    if (!read8(ptr, end, m_ch4.nr1) || !read8(ptr, end, m_ch4.nr2)) return false;
    if (!read8(ptr, end, m_ch4.nr3) || !read8(ptr, end, m_ch4.nr4)) return false;

    if (!read8(ptr, end, m_nr50) || !read8(ptr, end, m_nr51) || !read8(ptr, end, m_nr52)) return false;
    if (!readBool(ptr, end, m_outputEnabled)) return false;
    if (!read8(ptr, end, m_frameSeqStep)) return false;

    if (end - ptr < 24) return false;
    uint64_t st = 0, hl = 0, hr = 0;
    for (int i = 0; i < 8; ++i) st |= static_cast<uint64_t>(ptr[i]) << (i * 8);
    ptr += 8;
    for (int i = 0; i < 8; ++i) hl |= static_cast<uint64_t>(ptr[i]) << (i * 8);
    ptr += 8;
    for (int i = 0; i < 8; ++i) hr |= static_cast<uint64_t>(ptr[i]) << (i * 8);
    ptr += 8;
    std::memcpy(&m_sampleTimer, &st, 8);
    std::memcpy(&m_capLeft, &hl, 8);
    std::memcpy(&m_capRight, &hr, 8);

    clearSampleBuffer();
    return true;
}
