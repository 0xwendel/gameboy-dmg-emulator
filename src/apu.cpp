#include "apu.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr uint8_t kDutyTable[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 0},
};

uint16_t squarePeriod(uint16_t freq) {
    return static_cast<uint16_t>((2048 - (freq & 0x7FF)) * 4);
}
} // namespace

// ---------- Square ----------
void APU::SquareChannel::trigger(bool isCh1) {
    enabled = dacEnabled;
    freqTimer = squarePeriod(frequency);
    dutyStep = 0;
    envelopeTimer = envelopePeriod ? envelopePeriod : 8;
    volume = (nr2 >> 4) & 0x0F;
    if (length == 0) length = 64;

    if (isCh1 && hasSweep) {
        shadowFreq = frequency;
        sweepTimer = sweepPeriod ? sweepPeriod : 8;
        sweepEnabled = sweepPeriod != 0 || sweepShift != 0;
        if (sweepShift != 0) {
            // Overflow check
            uint16_t newFreq = shadowFreq >> sweepShift;
            if (sweepDecrease) {
                // ok
            } else if (shadowFreq + newFreq > 0x7FF) {
                enabled = false;
            }
        }
    }
}

void APU::SquareChannel::clockLength() {
    if (lengthEnabled && length > 0) {
        length--;
        if (length == 0) enabled = false;
    }
}

void APU::SquareChannel::clockEnvelope() {
    if (envelopePeriod == 0) return;
    if (envelopeTimer > 0) envelopeTimer--;
    if (envelopeTimer == 0) {
        envelopeTimer = envelopePeriod;
        if (envelopeIncrease && volume < 15) volume++;
        else if (!envelopeIncrease && volume > 0) volume--;
    }
}

void APU::SquareChannel::clockSweep() {
    if (!hasSweep || !sweepEnabled || sweepPeriod == 0) return;
    if (sweepTimer > 0) sweepTimer--;
    if (sweepTimer == 0) {
        sweepTimer = sweepPeriod ? sweepPeriod : 8;
        uint16_t delta = shadowFreq >> sweepShift;
        uint16_t newFreq = sweepDecrease ? static_cast<uint16_t>(shadowFreq - delta)
                                         : static_cast<uint16_t>(shadowFreq + delta);
        if (newFreq > 0x7FF) {
            enabled = false;
        } else if (sweepShift != 0) {
            shadowFreq = newFreq;
            frequency = newFreq;
            // second overflow check
            delta = shadowFreq >> sweepShift;
            newFreq = sweepDecrease ? static_cast<uint16_t>(shadowFreq - delta)
                                    : static_cast<uint16_t>(shadowFreq + delta);
            if (newFreq > 0x7FF) enabled = false;
        }
    }
}

float APU::SquareChannel::sample() const {
    if (!enabled || !dacEnabled) return 0.0f;
    const uint8_t bit = kDutyTable[duty & 3][dutyStep & 7];
    return bit ? (volume / 15.0f) : 0.0f;
}

void APU::SquareChannel::tickTimer(uint32_t tCycles) {
    if (!enabled) return;
    for (uint32_t i = 0; i < tCycles; ++i) {
        if (freqTimer > 0) freqTimer--;
        if (freqTimer == 0) {
            freqTimer = squarePeriod(frequency);
            dutyStep = (dutyStep + 1) & 7;
        }
    }
}

// ---------- Wave ----------
void APU::WaveChannel::trigger() {
    enabled = dacEnabled;
    freqTimer = static_cast<uint16_t>((2048 - (frequency & 0x7FF)) * 2);
    position = 0;
    if (length == 0) length = 256;
}

void APU::WaveChannel::clockLength() {
    if (lengthEnabled && length > 0) {
        length--;
        if (length == 0) enabled = false;
    }
}

float APU::WaveChannel::sample() const {
    if (!enabled || !dacEnabled || volumeCode == 0) return 0.0f;
    uint8_t shift = 0;
    switch (volumeCode) {
        case 1: shift = 0; break;
        case 2: shift = 1; break;
        case 3: shift = 2; break;
        default: return 0.0f;
    }
    return (currentSample >> shift) / 15.0f;
}

void APU::WaveChannel::tickTimer(uint32_t tCycles) {
    if (!enabled) return;
    for (uint32_t i = 0; i < tCycles; ++i) {
        if (freqTimer > 0) freqTimer--;
        if (freqTimer == 0) {
            freqTimer = static_cast<uint16_t>((2048 - (frequency & 0x7FF)) * 2);
            position = (position + 1) & 31;
            const uint8_t byte = waveRam[position / 2];
            currentSample = (position & 1) ? (byte & 0x0F) : (byte >> 4);
        }
    }
}

// ---------- Noise ----------
void APU::NoiseChannel::trigger() {
    enabled = dacEnabled;
    lfsr = 0x7FFF;
    envelopeTimer = envelopePeriod ? envelopePeriod : 8;
    volume = (nr2 >> 4) & 0x0F;
    if (length == 0) length = 64;
    static const uint8_t divisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};
    freqTimer = static_cast<uint16_t>(divisors[divisorCode & 7] << clockShift);
}

void APU::NoiseChannel::clockLength() {
    if (lengthEnabled && length > 0) {
        length--;
        if (length == 0) enabled = false;
    }
}

void APU::NoiseChannel::clockEnvelope() {
    if (envelopePeriod == 0) return;
    if (envelopeTimer > 0) envelopeTimer--;
    if (envelopeTimer == 0) {
        envelopeTimer = envelopePeriod;
        if (envelopeIncrease && volume < 15) volume++;
        else if (!envelopeIncrease && volume > 0) volume--;
    }
}

float APU::NoiseChannel::sample() const {
    if (!enabled || !dacEnabled) return 0.0f;
    return ((~lfsr) & 1) ? (volume / 15.0f) : 0.0f;
}

void APU::NoiseChannel::tickTimer(uint32_t tCycles) {
    if (!enabled) return;
    for (uint32_t i = 0; i < tCycles; ++i) {
        if (freqTimer > 0) freqTimer--;
        if (freqTimer == 0) {
            static const uint8_t divisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};
            freqTimer = static_cast<uint16_t>(divisors[divisorCode & 7] << clockShift);
            const uint8_t xorBit = (lfsr ^ (lfsr >> 1)) & 1;
            lfsr = static_cast<uint16_t>((lfsr >> 1) | (xorBit << 14));
            if (widthMode) {
                lfsr = static_cast<uint16_t>((lfsr & ~0x40) | (xorBit << 6));
            }
        }
    }
}

// ---------- APU ----------
APU::APU() {
    m_ch1.hasSweep = true;
    reset();
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
    m_masterEnabled = true;
    m_frameSeqTimer = 0;
    m_frameSeqStep = 0;
    m_sampleTimer = 0;
    m_sampleBuffer.clear();
    m_sampleBuffer.reserve(kSampleRate / 10);
}

void APU::writeRegister(uint16_t address, uint8_t value) {
    if (address == 0xFF26) {
        const bool wasOn = (m_nr52 & 0x80) != 0;
        m_nr52 = static_cast<uint8_t>((value & 0x80) | (m_nr52 & 0x7F));
        if (wasOn && !(m_nr52 & 0x80)) {
            // Power off: zera registradores de áudio (exceto wave RAM)
            for (uint16_t a = 0xFF10; a <= 0xFF25; ++a) {
                if (a < 0xFF30) writeRegister(a, 0);
            }
            m_ch1.enabled = m_ch2.enabled = m_ch3.enabled = m_ch4.enabled = false;
        }
        return;
    }

    if (!(m_nr52 & 0x80) && address != 0xFF26) {
        // Com APU off, só NR52 e wave RAM são graváveis
        if (address < 0xFF30 || address > 0xFF3F) return;
    }

    switch (address) {
        case 0xFF10:
            m_ch1.nr0 = value;
            m_ch1.sweepPeriod = (value >> 4) & 0x07;
            m_ch1.sweepDecrease = (value & 0x08) != 0;
            m_ch1.sweepShift = value & 0x07;
            break;
        case 0xFF11:
            m_ch1.nr1 = value;
            m_ch1.duty = (value >> 6) & 0x03;
            m_ch1.length = 64 - (value & 0x3F);
            break;
        case 0xFF12:
            m_ch1.nr2 = value;
            m_ch1.volume = (value >> 4) & 0x0F;
            m_ch1.envelopeIncrease = (value & 0x08) != 0;
            m_ch1.envelopePeriod = value & 0x07;
            m_ch1.dacEnabled = (value & 0xF8) != 0;
            if (!m_ch1.dacEnabled) m_ch1.enabled = false;
            break;
        case 0xFF13:
            m_ch1.nr3 = value;
            m_ch1.frequency = static_cast<uint16_t>((m_ch1.frequency & 0x700) | value);
            break;
        case 0xFF14:
            m_ch1.nr4 = value;
            m_ch1.frequency = static_cast<uint16_t>((m_ch1.frequency & 0xFF) | ((value & 0x07) << 8));
            m_ch1.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) m_ch1.trigger(true);
            break;

        case 0xFF16:
            m_ch2.nr1 = value;
            m_ch2.duty = (value >> 6) & 0x03;
            m_ch2.length = 64 - (value & 0x3F);
            break;
        case 0xFF17:
            m_ch2.nr2 = value;
            m_ch2.volume = (value >> 4) & 0x0F;
            m_ch2.envelopeIncrease = (value & 0x08) != 0;
            m_ch2.envelopePeriod = value & 0x07;
            m_ch2.dacEnabled = (value & 0xF8) != 0;
            if (!m_ch2.dacEnabled) m_ch2.enabled = false;
            break;
        case 0xFF18:
            m_ch2.nr3 = value;
            m_ch2.frequency = static_cast<uint16_t>((m_ch2.frequency & 0x700) | value);
            break;
        case 0xFF19:
            m_ch2.nr4 = value;
            m_ch2.frequency = static_cast<uint16_t>((m_ch2.frequency & 0xFF) | ((value & 0x07) << 8));
            m_ch2.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) m_ch2.trigger(false);
            break;

        case 0xFF1A:
            m_ch3.nr0 = value;
            m_ch3.dacEnabled = (value & 0x80) != 0;
            if (!m_ch3.dacEnabled) m_ch3.enabled = false;
            break;
        case 0xFF1B:
            m_ch3.nr1 = value;
            m_ch3.length = 256 - value;
            break;
        case 0xFF1C:
            m_ch3.nr2 = value;
            m_ch3.volumeCode = (value >> 5) & 0x03;
            break;
        case 0xFF1D:
            m_ch3.nr3 = value;
            m_ch3.frequency = static_cast<uint16_t>((m_ch3.frequency & 0x700) | value);
            break;
        case 0xFF1E:
            m_ch3.nr4 = value;
            m_ch3.frequency = static_cast<uint16_t>((m_ch3.frequency & 0xFF) | ((value & 0x07) << 8));
            m_ch3.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) m_ch3.trigger();
            break;

        case 0xFF20:
            m_ch4.nr1 = value;
            m_ch4.length = 64 - (value & 0x3F);
            break;
        case 0xFF21:
            m_ch4.nr2 = value;
            m_ch4.volume = (value >> 4) & 0x0F;
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
        case 0xFF23:
            m_ch4.nr4 = value;
            m_ch4.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) m_ch4.trigger();
            break;

        case 0xFF24:
            m_nr50 = value;
            break;
        case 0xFF25:
            m_nr51 = value;
            break;

        default:
            if (address >= 0xFF30 && address <= 0xFF3F) {
                m_ch3.waveRam[address - 0xFF30] = value;
            }
            break;
    }
}

uint8_t APU::readRegister(uint16_t address) const {
    auto orMask = [](uint8_t v, uint8_t mask) { return static_cast<uint8_t>(v | mask); };

    switch (address) {
        case 0xFF10: return orMask(m_ch1.nr0, 0x80);
        case 0xFF11: return orMask(m_ch1.nr1, 0x3F);
        case 0xFF12: return m_ch1.nr2;
        case 0xFF13: return 0xFF;
        case 0xFF14: return orMask(m_ch1.nr4, 0xBF);
        case 0xFF16: return orMask(m_ch2.nr1, 0x3F);
        case 0xFF17: return m_ch2.nr2;
        case 0xFF18: return 0xFF;
        case 0xFF19: return orMask(m_ch2.nr4, 0xBF);
        case 0xFF1A: return orMask(m_ch3.nr0, 0x7F);
        case 0xFF1B: return 0xFF;
        case 0xFF1C: return orMask(m_ch3.nr2, 0x9F);
        case 0xFF1D: return 0xFF;
        case 0xFF1E: return orMask(m_ch3.nr4, 0xBF);
        case 0xFF20: return 0xFF;
        case 0xFF21: return m_ch4.nr2;
        case 0xFF22: return m_ch4.nr3;
        case 0xFF23: return orMask(m_ch4.nr4, 0xBF);
        case 0xFF24: return m_nr50;
        case 0xFF25: return m_nr51;
        case 0xFF26: {
            uint8_t v = static_cast<uint8_t>(m_nr52 & 0x80);
            if (m_ch1.enabled) v |= 0x01;
            if (m_ch2.enabled) v |= 0x02;
            if (m_ch3.enabled) v |= 0x04;
            if (m_ch4.enabled) v |= 0x08;
            return static_cast<uint8_t>(v | 0x70);
        }
        default:
            if (address >= 0xFF30 && address <= 0xFF3F) {
                return m_ch3.waveRam[address - 0xFF30];
            }
            return 0xFF;
    }
}

void APU::clockFrameSequencer() {
    // 512 Hz steps: length em 0,2,4,6; sweep em 2,6; envelope em 7
    switch (m_frameSeqStep) {
        case 0:
        case 2:
        case 4:
        case 6:
            m_ch1.clockLength();
            m_ch2.clockLength();
            m_ch3.clockLength();
            m_ch4.clockLength();
            if (m_frameSeqStep == 2 || m_frameSeqStep == 6) {
                m_ch1.clockSweep();
            }
            break;
        case 7:
            m_ch1.clockEnvelope();
            m_ch2.clockEnvelope();
            m_ch4.clockEnvelope();
            break;
        default:
            break;
    }
    m_frameSeqStep = (m_frameSeqStep + 1) & 7;
}

void APU::mixSample() {
    if (!m_masterEnabled || !(m_nr52 & 0x80)) {
        m_sampleBuffer.push_back(0);
        m_sampleBuffer.push_back(0);
        return;
    }

    const float s1 = m_ch1.sample();
    const float s2 = m_ch2.sample();
    const float s3 = m_ch3.sample();
    const float s4 = m_ch4.sample();

    float left = 0.0f;
    float right = 0.0f;

    if (m_nr51 & 0x10) left += s1;
    if (m_nr51 & 0x01) right += s1;
    if (m_nr51 & 0x20) left += s2;
    if (m_nr51 & 0x02) right += s2;
    if (m_nr51 & 0x40) left += s3;
    if (m_nr51 & 0x04) right += s3;
    if (m_nr51 & 0x80) left += s4;
    if (m_nr51 & 0x08) right += s4;

    const float leftVol = (((m_nr50 >> 4) & 0x07) + 1) / 8.0f;
    const float rightVol = ((m_nr50 & 0x07) + 1) / 8.0f;

    left = std::clamp(left * leftVol * m_volume * 0.25f, -1.0f, 1.0f);
    right = std::clamp(right * rightVol * m_volume * 0.25f, -1.0f, 1.0f);

    m_sampleBuffer.push_back(static_cast<int16_t>(left * 30000.0f));
    m_sampleBuffer.push_back(static_cast<int16_t>(right * 30000.0f));
}

void APU::tick(uint32_t tCycles) {
    // Frame sequencer: 512 Hz = a cada 8192 T-cycles
    for (uint32_t i = 0; i < tCycles; ++i) {
        m_frameSeqTimer++;
        if (m_frameSeqTimer >= 8192) {
            m_frameSeqTimer = 0;
            if (m_nr52 & 0x80) clockFrameSequencer();
        }
    }

    if (m_nr52 & 0x80) {
        m_ch1.tickTimer(tCycles);
        m_ch2.tickTimer(tCycles);
        m_ch3.tickTimer(tCycles);
        m_ch4.tickTimer(tCycles);
    }

    m_sampleTimer += tCycles;
    while (m_sampleTimer >= kCyclesPerSample) {
        m_sampleTimer -= kCyclesPerSample;
        mixSample();
        // Evita buffer infinito se o host não consumir
        if (m_sampleBuffer.size() > static_cast<size_t>(kSampleRate * 2)) {
            m_sampleBuffer.erase(m_sampleBuffer.begin(),
                                 m_sampleBuffer.begin() + static_cast<std::ptrdiff_t>(kSampleRate));
        }
    }
}

size_t APU::popSamples(int16_t* out, size_t maxFrames) {
    const size_t availableFrames = m_sampleBuffer.size() / 2;
    const size_t frames = std::min(availableFrames, maxFrames);
    if (frames == 0) return 0;
    std::memcpy(out, m_sampleBuffer.data(), frames * 2 * sizeof(int16_t));
    m_sampleBuffer.erase(m_sampleBuffer.begin(),
                         m_sampleBuffer.begin() + static_cast<std::ptrdiff_t>(frames * 2));
    return frames;
}
