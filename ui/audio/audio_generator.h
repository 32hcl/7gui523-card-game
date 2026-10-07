#ifndef AUDIO_GENERATOR_H
#define AUDIO_GENERATOR_H

#include <QString>
#include <vector>
#include <cstdint>

/**
 * @brief 程序化音频生成器
 *
 * 用代码合成WAV格式的音效和背景音乐，无需外部音频资源：
 * - 振荡器（正弦/方波/三角/锯齿/噪声）
 * - ADSR包络
 * - 简单混响/延迟
 * - 多轨混音
 *
 * 所有生成的音频保存为16bit PCM WAV格式。
 */
class AudioGenerator
{
public:
    static constexpr int SAMPLE_RATE = 44100;

    /**
     * @brief 生成所有游戏音效到指定目录
     * @param outputDir 输出目录
     * @return 成功生成的文件数
     */
    static int generateAll(const QString& outputDir);

private:
    // 振荡器类型
    enum class WaveType { Sine, Square, Triangle, Sawtooth, Noise };

    // ADSR包络参数
    struct AdsrParams {
        float attack  = 0.01f;  // 起音(秒)
        float decay   = 0.05f;  // 衰减(秒)
        float sustain = 0.7f;   // 持续电平
        float release = 0.1f;   // 释放(秒)
    };

    // 生成单音
    static std::vector<float> generateTone(float freq, float duration,
                                            WaveType type, const AdsrParams& adsr,
                                            float amplitude = 0.3f);

    // 生成噪声
    static std::vector<float> generateNoise(float duration, float amplitude = 0.2f);

    // 频率调制（用于爆炸/冲击音效）
    static std::vector<float> generateSweep(float startFreq, float endFreq,
                                             float duration, WaveType type,
                                             const AdsrParams& adsr,
                                             float amplitude = 0.3f);

    // 混合两个音频缓冲区
    static std::vector<float> mix(const std::vector<float>& a,
                                   const std::vector<float>& b,
                                   float gainA = 1.0f, float gainB = 1.0f);

    // 应用简单延迟效果
    static std::vector<float> applyDelay(const std::vector<float>& input,
                                          float delayTime, float feedback);

    // 生成赌场风格BGM（简单的爵士和弦进行+鼓点）
    static std::vector<float> generateCasinoBgm(float durationSec, float bpm = 90.0f);

    // 生成战斗BGM（更有节奏感）
    static std::vector<float> generateBattleBgm(float durationSec, float bpm = 110.0f);

    // 生成胜利BGM（上升音阶）
    static std::vector<float> generateVictoryBgm(float durationSec);

    // 生成失败BGM（下降音阶）
    static std::vector<float> generateDefeatBgm(float durationSec);

    // 保存为WAV文件
    static bool saveWav(const QString& filePath, const std::vector<float>& samples,
                         int sampleRate = SAMPLE_RATE);

    // 工具：音符频率
    static float noteFreq(int midiNote);

    // 写WAV头
    static void writeWavHeader(std::vector<uint8_t>& data, int numSamples,
                                int sampleRate, int numChannels = 1,
                                int bitsPerSample = 16);
};

#endif // AUDIO_GENERATOR_H
