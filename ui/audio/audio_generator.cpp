#include "ui/audio/audio_generator.h"

#include <QFile>
#include <QDir>
#include <QDebug>
#include <cmath>
#include <cstring>
#include <random>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── 音符频率（A4 = 69 = 440Hz） ──
float AudioGenerator::noteFreq(int midiNote)
{
    return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
}

// ── 单音生成 ──
std::vector<float> AudioGenerator::generateTone(float freq, float duration,
                                                  WaveType type,
                                                  const AdsrParams& adsr,
                                                  float amplitude)
{
    int numSamples = static_cast<int>(duration * SAMPLE_RATE);
    std::vector<float> samples(numSamples, 0.0f);

    float phase = 0.0f;
    float phaseInc = 2.0f * static_cast<float>(M_PI) * freq / SAMPLE_RATE;

    int attackSamples  = static_cast<int>(adsr.attack * SAMPLE_RATE);
    int decaySamples   = static_cast<int>(adsr.decay * SAMPLE_RATE);
    int releaseSamples = static_cast<int>(adsr.release * SAMPLE_RATE);
    int sustainStart   = attackSamples + decaySamples;
    int sustainEnd     = numSamples - releaseSamples;
    sustainEnd = std::max(sustainEnd, sustainStart);

    for (int i = 0; i < numSamples; ++i) {
        float wave = 0.0f;
        switch (type) {
        case WaveType::Sine:
            wave = std::sin(phase);
            break;
        case WaveType::Square:
            wave = std::sin(phase) >= 0 ? 1.0f : -1.0f;
            break;
        case WaveType::Triangle: {
            float t = std::fmod(phase, 2.0f * static_cast<float>(M_PI)) / (2.0f * static_cast<float>(M_PI));
            wave = (t < 0.5f) ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
            break;
        }
        case WaveType::Sawtooth: {
            float t = std::fmod(phase, 2.0f * static_cast<float>(M_PI)) / (2.0f * static_cast<float>(M_PI));
            wave = 2.0f * t - 1.0f;
            break;
        }
        case WaveType::Noise:
            wave = (std::rand() / float(RAND_MAX)) * 2.0f - 1.0f;
            break;
        }

        // ADSR envelope
        float env = 0.0f;
        if (i < attackSamples) {
            env = static_cast<float>(i) / attackSamples;
        } else if (i < sustainStart) {
            float t = static_cast<float>(i - attackSamples) / decaySamples;
            env = 1.0f - t * (1.0f - adsr.sustain);
        } else if (i < sustainEnd) {
            env = adsr.sustain;
        } else if (releaseSamples > 0) {
            float t = static_cast<float>(i - sustainEnd) / releaseSamples;
            env = adsr.sustain * (1.0f - t);
        }

        samples[i] = wave * env * amplitude;
        phase += phaseInc;
    }

    return samples;
}

std::vector<float> AudioGenerator::generateNoise(float duration, float amplitude)
{
    int numSamples = static_cast<int>(duration * SAMPLE_RATE);
    std::vector<float> samples(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        samples[i] = ((std::rand() / float(RAND_MAX)) * 2.0f - 1.0f) * amplitude;
    }
    return samples;
}

std::vector<float> AudioGenerator::generateSweep(float startFreq, float endFreq,
                                                   float duration, WaveType type,
                                                   const AdsrParams& adsr,
                                                   float amplitude)
{
    int numSamples = static_cast<int>(duration * SAMPLE_RATE);
    std::vector<float> samples(numSamples, 0.0f);

    float phase = 0.0f;

    int attackSamples  = static_cast<int>(adsr.attack * SAMPLE_RATE);
    int decaySamples   = static_cast<int>(adsr.decay * SAMPLE_RATE);
    int releaseSamples = static_cast<int>(adsr.release * SAMPLE_RATE);
    int sustainStart   = attackSamples + decaySamples;
    int sustainEnd     = numSamples - releaseSamples;
    sustainEnd = std::max(sustainEnd, sustainStart);

    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / numSamples;
        float freq = startFreq + (endFreq - startFreq) * t;
        float phaseInc = 2.0f * static_cast<float>(M_PI) * freq / SAMPLE_RATE;

        float wave = 0.0f;
        switch (type) {
        case WaveType::Sine:
            wave = std::sin(phase);
            break;
        case WaveType::Square:
            wave = std::sin(phase) >= 0 ? 1.0f : -1.0f;
            break;
        case WaveType::Triangle: {
            float tp = std::fmod(phase, 2.0f * static_cast<float>(M_PI)) / (2.0f * static_cast<float>(M_PI));
            wave = (tp < 0.5f) ? (4.0f * tp - 1.0f) : (3.0f - 4.0f * tp);
            break;
        }
        case WaveType::Sawtooth: {
            float tp = std::fmod(phase, 2.0f * static_cast<float>(M_PI)) / (2.0f * static_cast<float>(M_PI));
            wave = 2.0f * tp - 1.0f;
            break;
        }
        case WaveType::Noise:
            wave = (std::rand() / float(RAND_MAX)) * 2.0f - 1.0f;
            break;
        }

        float env = 0.0f;
        if (i < attackSamples) {
            env = static_cast<float>(i) / attackSamples;
        } else if (i < sustainStart) {
            float tp = static_cast<float>(i - attackSamples) / decaySamples;
            env = 1.0f - tp * (1.0f - adsr.sustain);
        } else if (i < sustainEnd) {
            env = adsr.sustain;
        } else if (releaseSamples > 0) {
            float tp = static_cast<float>(i - sustainEnd) / releaseSamples;
            env = adsr.sustain * (1.0f - tp);
        }

        samples[i] = wave * env * amplitude;
        phase += phaseInc;
    }

    return samples;
}

std::vector<float> AudioGenerator::mix(const std::vector<float>& a,
                                        const std::vector<float>& b,
                                        float gainA, float gainB)
{
    size_t maxLen = std::max(a.size(), b.size());
    std::vector<float> result(maxLen, 0.0f);
    for (size_t i = 0; i < maxLen; ++i) {
        float sa = (i < a.size()) ? a[i] * gainA : 0.0f;
        float sb = (i < b.size()) ? b[i] * gainB : 0.0f;
        result[i] = sa + sb;
    }
    return result;
}

std::vector<float> AudioGenerator::applyDelay(const std::vector<float>& input,
                                               float delayTime, float feedback)
{
    int delaySamples = static_cast<int>(delayTime * SAMPLE_RATE);
    std::vector<float> result(input.size() + delaySamples, 0.0f);

    for (size_t i = 0; i < input.size(); ++i) {
        result[i] += input[i];
        if (i + delaySamples < result.size()) {
            result[i + delaySamples] += input[i] * feedback;
        }
    }

    // 归一化
    float peak = 0.0f;
    for (float s : result) peak = std::max(peak, std::abs(s));
    if (peak > 0.0f) {
        for (float& s : result) s /= peak * 0.8f;
    }

    return result;
}

// ── 赌场BGM：爵士风格 + 轻鼓点 ──
std::vector<float> AudioGenerator::generateCasinoBgm(float durationSec, float bpm)
{
    float beatDur = 60.0f / bpm;
    int totalSamples = static_cast<int>(durationSec * SAMPLE_RATE);
    std::vector<float> result(totalSamples, 0.0f);

    // 和弦进行：ii-V-I 爵士经典进行 (Dm7-G7-Cmaj7)
    // 每个和弦2拍，4小节循环
    int chords[][4] = {
        {62, 65, 69, 72},   // Dm7: D F A C
        {67, 71, 74, 77},   // G7:  G B D F
        {72, 76, 79, 83},   // Cmaj7: C E G B
        {71, 74, 78, 81},   // Bm7b5: B D F A (替代)
    };
    int numChords = 4;
    float chordDur = beatDur * 2;  // 每个和弦2拍

    // 贝斯线（每拍根音）
    AdsrParams bassAdsr = {0.01f, 0.05f, 0.6f, 0.15f};
    for (int bar = 0; bar < static_cast<int>(durationSec / (chordDur * numChords)) + 1; ++bar) {
        for (int c = 0; c < numChords; ++c) {
            float startTime = (bar * numChords + c) * chordDur;
            if (startTime >= durationSec) break;

            // 贝斯根音（低八度）
            int rootNote = chords[c][0] - 12;
            auto bass = generateTone(noteFreq(rootNote), chordDur * 0.9f,
                                      WaveType::Sine, bassAdsr, 0.25f);

            int startIdx = static_cast<int>(startTime * SAMPLE_RATE);
            for (size_t i = 0; i < bass.size() && startIdx + i < result.size(); ++i) {
                result[startIdx + i] += bass[i];
            }
        }
    }

    // 和弦垫（钢琴音色感：三角波+正弦混合）
    AdsrParams padAdsr = {0.05f, 0.1f, 0.4f, 0.2f};
    for (int bar = 0; bar < static_cast<int>(durationSec / (chordDur * numChords)) + 1; ++bar) {
        for (int c = 0; c < numChords; ++c) {
            float startTime = (bar * numChords + c) * chordDur;
            if (startTime >= durationSec) break;

            std::vector<float> chord;
            for (int n = 0; n < 4; ++n) {
                auto note = generateTone(noteFreq(chords[c][n]), chordDur * 0.95f,
                                          WaveType::Triangle, padAdsr, 0.08f);
                chord = mix(chord, note, 1.0f, 1.0f);
            }

            int startIdx = static_cast<int>(startTime * SAMPLE_RATE);
            for (size_t i = 0; i < chord.size() && startIdx + i < result.size(); ++i) {
                result[startIdx + i] += chord[i];
            }
        }
    }

    // 鼓点：弱拍踩镲（噪声短促）
    AdsrParams hatAdsr = {0.001f, 0.03f, 0.1f, 0.02f};
    for (int beat = 0; beat < static_cast<int>(durationSec / beatDur); ++beat) {
        float startTime = beat * beatDur;
        auto hat = generateNoise(0.05f, 0.06f);
        // 包络
        int startIdx = static_cast<int>(startTime * SAMPLE_RATE);
        for (size_t i = 0; i < hat.size() && startIdx + i < result.size(); ++i) {
            float env = std::exp(-static_cast<float>(i) / (0.02f * SAMPLE_RATE));
            result[startIdx + i] += hat[i] * env;
        }
    }

    // 底鼓（低频sweep）
    AdsrParams kickAdsr = {0.001f, 0.08f, 0.0f, 0.05f};
    for (int beat = 0; beat < static_cast<int>(durationSec / beatDur); ++beat) {
        if (beat % 2 != 0) continue;  // 1、3拍
        float startTime = beat * beatDur;
        auto kick = generateSweep(120.0f, 40.0f, 0.15f,
                                   WaveType::Sine, kickAdsr, 0.4f);
        int startIdx = static_cast<int>(startTime * SAMPLE_RATE);
        for (size_t i = 0; i < kick.size() && startIdx + i < result.size(); ++i) {
            result[startIdx + i] += kick[i];
        }
    }

    // 归一化
    float peak = 0.0f;
    for (float s : result) peak = std::max(peak, std::abs(s));
    if (peak > 0.0f) {
        for (float& s : result) s /= peak * 0.85f;
    }

    return result;
}

// ── 战斗BGM：更有节奏感 ──
std::vector<float> AudioGenerator::generateBattleBgm(float durationSec, float bpm)
{
    float beatDur = 60.0f / bpm;
    int totalSamples = static_cast<int>(durationSec * SAMPLE_RATE);
    std::vector<float> result(totalSamples, 0.0f);

    // 小调进行（紧张感）
    int chords[][3] = {
        {57, 60, 64},  // Am: A C E
        {55, 59, 62},  // G:  G B D
        {53, 57, 60},  // F:  F A C
        {52, 55, 59},  // Em: E G B
    };
    int numChords = 4;
    float chordDur = beatDur * 2;

    // 贝斯（每个半拍都有，更紧凑）
    AdsrParams bassAdsr = {0.005f, 0.03f, 0.5f, 0.05f};
    for (int bar = 0; bar < static_cast<int>(durationSec / (chordDur * numChords)) + 1; ++bar) {
        for (int c = 0; c < numChords; ++c) {
            float startTime = (bar * numChords + c) * chordDur;
            if (startTime >= durationSec) break;

            // 每半拍一个贝斯音
            for (int h = 0; h < 4; ++h) {
                float hStart = startTime + h * (beatDur / 2.0f);
                int note = chords[c][0] - 12 + (h % 2 == 0 ? 0 : 7);  // 根音+五度交替
                auto bass = generateTone(noteFreq(note), beatDur * 0.4f,
                                          WaveType::Sine, bassAdsr, 0.2f);
                int startIdx = static_cast<int>(hStart * SAMPLE_RATE);
                for (size_t i = 0; i < bass.size() && startIdx + i < result.size(); ++i) {
                    result[startIdx + i] += bass[i];
                }
            }
        }
    }

    // 弦乐和弦垫（方波+低通感）
    AdsrParams padAdsr = {0.1f, 0.2f, 0.3f, 0.3f};
    for (int bar = 0; bar < static_cast<int>(durationSec / (chordDur * numChords)) + 1; ++bar) {
        for (int c = 0; c < numChords; ++c) {
            float startTime = (bar * numChords + c) * chordDur;
            if (startTime >= durationSec) break;

            std::vector<float> chord;
            for (int n = 0; n < 3; ++n) {
                auto note = generateTone(noteFreq(chords[c][n] + 12), chordDur * 0.95f,
                                          WaveType::Sawtooth, padAdsr, 0.05f);
                chord = mix(chord, note, 1.0f, 1.0f);
            }
            int startIdx = static_cast<int>(startTime * SAMPLE_RATE);
            for (size_t i = 0; i < chord.size() && startIdx + i < result.size(); ++i) {
                result[startIdx + i] += chord[i];
            }
        }
    }

    // 军鼓（2、4拍）
    AdsrParams snareAdsr = {0.001f, 0.1f, 0.0f, 0.05f};
    for (int beat = 0; beat < static_cast<int>(durationSec / beatDur); ++beat) {
        if (beat % 2 != 1) continue;  // 2、4拍
        float startTime = beat * beatDur;
        auto snare = generateSweep(200.0f, 100.0f, 0.15f,
                                    WaveType::Noise, snareAdsr, 0.3f);
        int startIdx = static_cast<int>(startTime * SAMPLE_RATE);
        for (size_t i = 0; i < snare.size() && startIdx + i < result.size(); ++i) {
            result[startIdx + i] += snare[i];
        }
    }

    // 底鼓（1、3拍）
    AdsrParams kickAdsr = {0.001f, 0.1f, 0.0f, 0.05f};
    for (int beat = 0; beat < static_cast<int>(durationSec / beatDur); ++beat) {
        if (beat % 2 != 0) continue;
        float startTime = beat * beatDur;
        auto kick = generateSweep(150.0f, 50.0f, 0.2f,
                                   WaveType::Sine, kickAdsr, 0.5f);
        int startIdx = static_cast<int>(startTime * SAMPLE_RATE);
        for (size_t i = 0; i < kick.size() && startIdx + i < result.size(); ++i) {
            result[startIdx + i] += kick[i];
        }
    }

    // 踩镲八分音符
    for (int h = 0; h < static_cast<int>(durationSec / (beatDur / 2.0f)); ++h) {
        float startTime = h * (beatDur / 2.0f);
        auto hat = generateNoise(0.03f, 0.04f);
        int startIdx = static_cast<int>(startTime * SAMPLE_RATE);
        for (size_t i = 0; i < hat.size() && startIdx + i < result.size(); ++i) {
            float env = std::exp(-static_cast<float>(i) / (0.015f * SAMPLE_RATE));
            result[startIdx + i] += hat[i] * env;
        }
    }

    // 归一化
    float peak = 0.0f;
    for (float s : result) peak = std::max(peak, std::abs(s));
    if (peak > 0.0f) {
        for (float& s : result) s /= peak * 0.85f;
    }

    return result;
}

// ── 胜利BGM：上升琶音 ──
std::vector<float> AudioGenerator::generateVictoryBgm(float durationSec)
{
    int totalSamples = static_cast<int>(durationSec * SAMPLE_RATE);
    std::vector<float> result(totalSamples, 0.0f);

    // C大调音阶上行：C D E G C5（分解和弦）
    int notes[] = {72, 76, 79, 84, 79, 84, 88, 91};
    int numNotes = 8;
    float noteDur = durationSec / numNotes;

    AdsrParams adsr = {0.02f, 0.05f, 0.5f, 0.3f};
    for (int i = 0; i < numNotes; ++i) {
        float startTime = i * noteDur;
        // 主音
        auto tone = generateTone(noteFreq(notes[i]), noteDur * 0.9f,
                                  WaveType::Triangle, adsr, 0.25f);
        // 高八度和声
        auto harmony = generateTone(noteFreq(notes[i] + 12), noteDur * 0.9f,
                                     WaveType::Sine, adsr, 0.1f);
        auto mixed = mix(tone, harmony);

        int startIdx = static_cast<int>(startTime * SAMPLE_RATE);
        for (size_t j = 0; j < mixed.size() && startIdx + j < result.size(); ++j) {
            result[startIdx + j] += mixed[j];
        }
    }

    // 结尾延音
    int lastNoteStart = static_cast<int>((numNotes - 1) * noteDur * SAMPLE_RATE);
    for (int i = lastNoteStart; i < totalSamples; ++i) {
        float t = static_cast<float>(i - lastNoteStart) / (totalSamples - lastNoteStart);
        result[i] *= std::exp(-t * 3.0f);
    }

    // 归一化
    float peak = 0.0f;
    for (float s : result) peak = std::max(peak, std::abs(s));
    if (peak > 0.0f) {
        for (float& s : result) s /= peak * 0.85f;
    }

    return result;
}

// ── 失败BGM：下行音阶 ──
std::vector<float> AudioGenerator::generateDefeatBgm(float durationSec)
{
    int totalSamples = static_cast<int>(durationSec * SAMPLE_RATE);
    std::vector<float> result(totalSamples, 0.0f);

    // 小调下行：C Bb Ab G F Eb D C
    int notes[] = {84, 82, 80, 79, 77, 75, 74, 72};
    int numNotes = 8;
    float noteDur = durationSec / numNotes;

    AdsrParams adsr = {0.03f, 0.1f, 0.4f, 0.4f};
    for (int i = 0; i < numNotes; ++i) {
        float startTime = i * noteDur;
        auto tone = generateTone(noteFreq(notes[i]), noteDur * 0.85f,
                                  WaveType::Sawtooth, adsr, 0.18f);
        auto low = generateTone(noteFreq(notes[i] - 12), noteDur * 0.85f,
                                 WaveType::Sine, adsr, 0.12f);
        auto mixed = mix(tone, low);

        int startIdx = static_cast<int>(startTime * SAMPLE_RATE);
        for (size_t j = 0; j < mixed.size() && startIdx + j < result.size(); ++j) {
            result[startIdx + j] += mixed[j];
        }
    }

    // 结尾渐弱
    for (int i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / totalSamples;
        if (t > 0.6f) {
            result[i] *= 1.0f - (t - 0.6f) / 0.4f;
        }
    }

    // 归一化
    float peak = 0.0f;
    for (float s : result) peak = std::max(peak, std::abs(s));
    if (peak > 0.0f) {
        for (float& s : result) s /= peak * 0.85f;
    }

    return result;
}

// ── WAV文件写入 ──
void AudioGenerator::writeWavHeader(std::vector<uint8_t>& data, int numSamples,
                                      int sampleRate, int numChannels,
                                      int bitsPerSample)
{
    int byteRate = sampleRate * numChannels * bitsPerSample / 8;
    int blockAlign = numChannels * bitsPerSample / 8;
    int dataSize = numSamples * numChannels * bitsPerSample / 8;
    int chunkSize = 36 + dataSize;

    // RIFF header
    data.push_back('R'); data.push_back('I'); data.push_back('F'); data.push_back('F');
    data.push_back(chunkSize & 0xFF);
    data.push_back((chunkSize >> 8) & 0xFF);
    data.push_back((chunkSize >> 16) & 0xFF);
    data.push_back((chunkSize >> 24) & 0xFF);
    data.push_back('W'); data.push_back('A'); data.push_back('V'); data.push_back('E');

    // fmt subchunk
    data.push_back('f'); data.push_back('m'); data.push_back('t'); data.push_back(' ');
    data.push_back(16); data.push_back(0); data.push_back(0); data.push_back(0);  // PCM = 16
    data.push_back(1); data.push_back(0);  // AudioFormat = PCM
    data.push_back(numChannels & 0xFF);
    data.push_back((numChannels >> 8) & 0xFF);
    data.push_back(sampleRate & 0xFF);
    data.push_back((sampleRate >> 8) & 0xFF);
    data.push_back((sampleRate >> 16) & 0xFF);
    data.push_back((sampleRate >> 24) & 0xFF);
    data.push_back(byteRate & 0xFF);
    data.push_back((byteRate >> 8) & 0xFF);
    data.push_back((byteRate >> 16) & 0xFF);
    data.push_back((byteRate >> 24) & 0xFF);
    data.push_back(blockAlign & 0xFF);
    data.push_back((blockAlign >> 8) & 0xFF);
    data.push_back(bitsPerSample & 0xFF);
    data.push_back((bitsPerSample >> 8) & 0xFF);

    // data subchunk
    data.push_back('d'); data.push_back('a'); data.push_back('t'); data.push_back('a');
    data.push_back(dataSize & 0xFF);
    data.push_back((dataSize >> 8) & 0xFF);
    data.push_back((dataSize >> 16) & 0xFF);
    data.push_back((dataSize >> 24) & 0xFF);
}

bool AudioGenerator::saveWav(const QString& filePath, const std::vector<float>& samples,
                               int sampleRate)
{
    std::vector<uint8_t> wavData;
    writeWavHeader(wavData, static_cast<int>(samples.size()), sampleRate, 1, 16);

    // 写入16bit PCM数据
    for (float s : samples) {
        int16_t val = static_cast<int16_t>(s * 32767.0f);
        val = std::max<int16_t>(-32767, std::min<int16_t>(32767, val));
        wavData.push_back(val & 0xFF);
        wavData.push_back((val >> 8) & 0xFF);
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "[AudioGenerator] Failed to open:" << filePath;
        return false;
    }

    qint64 written = file.write(reinterpret_cast<const char*>(wavData.data()), wavData.size());
    file.close();
    return written == static_cast<qint64>(wavData.size());
}

// ── 生成所有音效 ──
int AudioGenerator::generateAll(const QString& outputDir)
{
    QDir().mkpath(outputDir);
    int count = 0;

    // ===== BGM =====
    // 主菜单BGM：约30秒循环
    {
        auto bgm = generateCasinoBgm(30.0f, 90.0f);
        // 开头和结尾交叉淡入淡出，使其可无缝循环
        int fadeSamples = static_cast<int>(1.0f * SAMPLE_RATE);
        for (int i = 0; i < fadeSamples && i < static_cast<int>(bgm.size()); ++i) {
            float fadeIn = static_cast<float>(i) / fadeSamples;
            bgm[i] *= fadeIn;
            if (static_cast<int>(bgm.size()) - 1 - i >= 0) {
                bgm[bgm.size() - 1 - i] *= fadeIn;
            }
        }
        if (saveWav(outputDir + "/bgm_menu.wav", bgm)) count++;
    }

    // 战斗BGM：约20秒循环
    {
        auto bgm = generateBattleBgm(20.0f, 110.0f);
        int fadeSamples = static_cast<int>(1.0f * SAMPLE_RATE);
        for (int i = 0; i < fadeSamples && i < static_cast<int>(bgm.size()); ++i) {
            float fadeIn = static_cast<float>(i) / fadeSamples;
            bgm[i] *= fadeIn;
            if (static_cast<int>(bgm.size()) - 1 - i >= 0) {
                bgm[bgm.size() - 1 - i] *= fadeIn;
            }
        }
        if (saveWav(outputDir + "/bgm_battle.wav", bgm)) count++;
    }

    // 胜利BGM
    {
        auto bgm = generateVictoryBgm(4.0f);
        if (saveWav(outputDir + "/bgm_victory.wav", bgm)) count++;
    }

    // 失败BGM
    {
        auto bgm = generateDefeatBgm(4.0f);
        if (saveWav(outputDir + "/bgm_defeat.wav", bgm)) count++;
    }

    // ===== SFX =====
    AdsrParams shortAdsr = {0.001f, 0.02f, 0.5f, 0.05f};
    AdsrParams midAdsr   = {0.005f, 0.05f, 0.6f, 0.1f};

    // 点击音：短促高频点击
    {
        auto click = generateTone(1200.0f, 0.04f, WaveType::Sine, shortAdsr, 0.3f);
        if (saveWav(outputDir + "/sfx_click.wav", click)) count++;
    }

    // 出牌音：纸牌滑过桌面的声音（噪声+扫频）
    {
        auto swipe = generateSweep(800.0f, 300.0f, 0.12f, WaveType::Noise, shortAdsr, 0.2f);
        if (saveWav(outputDir + "/sfx_playcard.wav", swipe)) count++;
    }

    // Pass音：低调下降
    {
        auto pass = generateSweep(400.0f, 200.0f, 0.15f, WaveType::Triangle, midAdsr, 0.2f);
        if (saveWav(outputDir + "/sfx_pass.wav", pass)) count++;
    }

    // 炸弹音：低频爆炸（噪声+sweep）
    {
        auto boom = generateSweep(200.0f, 40.0f, 0.5f, WaveType::Noise,
                                   {0.001f, 0.1f, 0.3f, 0.4f}, 0.4f);
        auto sub = generateSweep(100.0f, 30.0f, 0.6f, WaveType::Sine,
                                  {0.001f, 0.15f, 0.2f, 0.5f}, 0.5f);
        auto mixed = mix(boom, sub);
        mixed = applyDelay(mixed, 0.15f, 0.3f);
        if (saveWav(outputDir + "/sfx_bomb.wav", mixed)) count++;
    }

    // 王炸音：金色辉煌的和弦
    {
        // 大三和弦+C5，三角波+正弦混合
        AdsrParams rocketAdsr = {0.01f, 0.1f, 0.7f, 0.5f};
        auto c5 = generateTone(noteFreq(84), 0.8f, WaveType::Triangle, rocketAdsr, 0.2f);
        auto e5 = generateTone(noteFreq(88), 0.8f, WaveType::Sine, rocketAdsr, 0.15f);
        auto g5 = generateTone(noteFreq(91), 0.8f, WaveType::Triangle, rocketAdsr, 0.15f);
        auto c6 = generateTone(noteFreq(96), 0.8f, WaveType::Sine, rocketAdsr, 0.1f);
        auto chord = mix(c5, e5);
        chord = mix(chord, g5);
        chord = mix(chord, c6);
        // 加一点闪亮的高频
        auto shine = generateSweep(2000.0f, 4000.0f, 0.3f,
                                    WaveType::Sine, {0.001f, 0.05f, 0.3f, 0.25f}, 0.1f);
        chord = mix(chord, shine);
        chord = applyDelay(chord, 0.2f, 0.25f);
        if (saveWav(outputDir + "/sfx_rocket.wav", chord)) count++;
    }

    // 七鬼523：无敌特效音（上升琶音+闪烁）
    {
        std::vector<float> special;
        int notes[] = {72, 76, 79, 84, 88, 91, 96};
        float totalDur = 1.0f;
        float noteDur = totalDur / 7;
        AdsrParams specAdsr = {0.005f, 0.02f, 0.6f, 0.08f};
        for (int i = 0; i < 7; ++i) {
            auto note = generateTone(noteFreq(notes[i]), noteDur * 1.5f,
                                      WaveType::Square, specAdsr, 0.12f);
            auto sine = generateTone(noteFreq(notes[i] + 12), noteDur * 1.5f,
                                      WaveType::Sine, specAdsr, 0.08f);
            auto mixed = mix(note, sine);
            int offset = static_cast<int>(i * noteDur * SAMPLE_RATE);
            if (static_cast<int>(special.size()) < offset + static_cast<int>(mixed.size())) {
                special.resize(offset + mixed.size(), 0.0f);
            }
            for (size_t j = 0; j < mixed.size(); ++j) {
                special[offset + j] += mixed[j];
            }
        }
        if (saveWav(outputDir + "/sfx_special523.wav", special)) count++;
    }

    // 回合胜利：上升三音
    {
        AdsrParams winAdsr = {0.01f, 0.05f, 0.5f, 0.15f};
        auto n1 = generateTone(noteFreq(72), 0.15f, WaveType::Triangle, winAdsr, 0.2f);
        auto n2 = generateTone(noteFreq(76), 0.15f, WaveType::Triangle, winAdsr, 0.2f);
        auto n3 = generateTone(noteFreq(79), 0.3f, WaveType::Triangle, winAdsr, 0.25f);
        std::vector<float> win;
        auto append = [&](const std::vector<float>& s, float delay) {
            int offset = static_cast<int>(delay * SAMPLE_RATE);
            if (static_cast<int>(win.size()) < offset + static_cast<int>(s.size())) {
                win.resize(offset + s.size(), 0.0f);
            }
            for (size_t j = 0; j < s.size(); ++j) win[offset + j] += s[j];
        };
        append(n1, 0.0f);
        append(n2, 0.1f);
        append(n3, 0.2f);
        if (saveWav(outputDir + "/sfx_win.wav", win)) count++;
    }

    // 回合失败：下降两音
    {
        AdsrParams loseAdsr = {0.01f, 0.08f, 0.4f, 0.2f};
        auto n1 = generateTone(noteFreq(67), 0.2f, WaveType::Sawtooth, loseAdsr, 0.2f);
        auto n2 = generateTone(noteFreq(64), 0.3f, WaveType::Sawtooth, loseAdsr, 0.2f);
        std::vector<float> lose;
        auto append = [&](const std::vector<float>& s, float delay) {
            int offset = static_cast<int>(delay * SAMPLE_RATE);
            if (static_cast<int>(lose.size()) < offset + static_cast<int>(s.size())) {
                lose.resize(offset + s.size(), 0.0f);
            }
            for (size_t j = 0; j < s.size(); ++j) lose[offset + j] += s[j];
        };
        append(n1, 0.0f);
        append(n2, 0.15f);
        if (saveWav(outputDir + "/sfx_lose.wav", lose)) count++;
    }

    // 筹码增加：金币叮咚声
    {
        AdsrParams chipAdsr = {0.001f, 0.05f, 0.3f, 0.2f};
        auto c1 = generateTone(noteFreq(88), 0.2f, WaveType::Sine, chipAdsr, 0.3f);
        auto c2 = generateTone(noteFreq(91), 0.2f, WaveType::Sine, chipAdsr, 0.25f);
        auto c3 = generateTone(noteFreq(96), 0.3f, WaveType::Triangle, chipAdsr, 0.2f);
        std::vector<float> chip;
        auto append = [&](const std::vector<float>& s, float d) {
            int o = static_cast<int>(d * SAMPLE_RATE);
            if (static_cast<int>(chip.size()) < o + static_cast<int>(s.size())) {
                chip.resize(o + s.size(), 0.0f);
            }
            for (size_t j = 0; j < s.size(); ++j) chip[o + j] += s[j];
        };
        append(c1, 0.0f);
        append(c2, 0.06f);
        append(c3, 0.12f);
        if (saveWav(outputDir + "/sfx_chipgain.wav", chip)) count++;
    }

    // 筹码减少：低沉金属声
    {
        AdsrParams loseAdsr = {0.005f, 0.1f, 0.3f, 0.25f};
        auto c1 = generateTone(noteFreq(55), 0.25f, WaveType::Triangle, loseAdsr, 0.25f);
        auto c2 = generateTone(noteFreq(52), 0.3f, WaveType::Sine, loseAdsr, 0.2f);
        auto mixed = mix(c1, c2);
        if (saveWav(outputDir + "/sfx_chiplose.wav", mixed)) count++;
    }

    // 卡牌选中：轻微咔哒声
    {
        auto sel = generateTone(900.0f, 0.03f, WaveType::Sine, shortAdsr, 0.2f);
        if (saveWav(outputDir + "/sfx_select.wav", sel)) count++;
    }

    // 卡牌取消选中：更低的咔哒
    {
        auto desel = generateTone(700.0f, 0.03f, WaveType::Sine, shortAdsr, 0.15f);
        if (saveWav(outputDir + "/sfx_deselect.wav", desel)) count++;
    }

    // HP扣减：伤害音效
    {
        auto dmg = generateSweep(300.0f, 80.0f, 0.2f, WaveType::Sawtooth,
                                  {0.001f, 0.05f, 0.3f, 0.15f}, 0.25f);
        if (saveWav(outputDir + "/sfx_damage.wav", dmg)) count++;
    }

    // HP恢复：柔和上升音
    {
        auto heal = generateSweep(400.0f, 800.0f, 0.3f, WaveType::Sine,
                                   {0.02f, 0.05f, 0.4f, 0.25f}, 0.2f);
        if (saveWav(outputDir + "/sfx_heal.wav", heal)) count++;
    }

    qDebug() << "[AudioGenerator] Generated" << count << "audio files to" << outputDir;
    return count;
}
