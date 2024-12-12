#include <iostream>
#include <sndfile.h>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>

using namespace std;
using namespace std::chrono;

void readWavFile(const std::string& inputFile, std::vector<float>& data, SF_INFO& fileInfo) {
    SNDFILE* inFile = sf_open(inputFile.c_str(), SFM_READ, &fileInfo);
    if (!inFile) {
        std::cerr << "Error opening input file: " << sf_strerror(NULL) << std::endl;
        exit(1);
    }

    data.resize(fileInfo.frames * fileInfo.channels);
    sf_count_t numFrames = sf_readf_float(inFile, data.data(), fileInfo.frames);
    if (numFrames != fileInfo.frames) {
        std::cerr << "Error reading frames from file." << std::endl;
        sf_close(inFile);
        exit(1);
    }

    sf_close(inFile);
}

void writeWavFile(const std::string& outputFile, const std::vector<float>& data, SF_INFO& fileInfo) {
    sf_count_t originalFrames = fileInfo.frames;
    SNDFILE* outFile = sf_open(outputFile.c_str(), SFM_WRITE, &fileInfo);
    if (!outFile) {
        std::cerr << "Error opening output file: " << sf_strerror(NULL) << std::endl;
        exit(1);
    }

    sf_count_t numFrames = sf_writef_float(outFile, data.data(), originalFrames);
    if (numFrames != originalFrames) {
        std::cerr << "Error writing frames to file." << std::endl;
        sf_close(outFile);
        exit(1);
    }

    sf_close(outFile);
}

void normalizeAudio(std::vector<float>& data) {
    float maxAmplitude = *std::max_element(data.begin(), data.end(), [](float a, float b) { return std::abs(a) < std::abs(b); });
    if (maxAmplitude > 0) {
        for (auto& sample : data) {
            sample /= maxAmplitude;
        }
    }
}

void applyBandpassFilter(std::vector<float>& data, int sampleRate, float lowCutoff, float highCutoff) {
    float deltaF = highCutoff - lowCutoff;

    for (size_t i = 0; i < data.size(); ++i) {
        float f = static_cast<float>(i) / data.size() * sampleRate;
        if (f < lowCutoff || f > highCutoff) {
            data[i] = 0.0f;
        } else {
            data[i] *= (f * f) / (f * f + deltaF * deltaF);
        }
    }
}

void applyNotchFilter(std::vector<float>& data, int sampleRate, float notchFrequency, int n) {
    for (size_t i = 0; i < data.size(); ++i) {
        float f = static_cast<float>(i) / data.size() * sampleRate;
        float H_f = 1.0f / (1.0f + std::pow((notchFrequency / f), 2 * n));
        data[i] *= H_f;
    }
}

void applyFIRFilter(std::vector<float>& data, const std::vector<float>& coefficients) {
    int M = coefficients.size();
    std::vector<float> filteredData(data.size(), 0.0f);

    for (size_t n = 0; n < data.size(); ++n) {
        for (int k = 0; k < M; ++k) {
            if (n >= k) {
                filteredData[n] += coefficients[k] * data[n - k];
            }
        }
    }

    data = filteredData;
}

void applyIIRFilter(std::vector<float>& data, const std::vector<float>& b, const std::vector<float>& a) {
    int P = b.size();
    int Q = a.size();
    std::vector<float> filteredData(data.size(), 0.0f);

    for (size_t n = 0; n < data.size(); ++n) {
        for (int i = 0; i < P; ++i) {
            if (n >= i) {
                filteredData[n] += b[i] * data[n - i];
            }
        }
        for (int j = 1; j < Q; ++j) {
            if (n >= j) {
                filteredData[n] -= a[j] * filteredData[n - j];
            }
        }
    }

    data = filteredData;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <../input.wav>" << std::endl;
        return 1;
    }

    auto start = high_resolution_clock::now();


    std::string inputFile = argv[1];
    std::string outputFile1 = "outputBandpassSerial.wav";
    std::string outputFile2 = "outputNotchSerial.wav";
    std::string outputFile3 = "outputFIRSerial.wav";
    std::string outputFile4 = "outputIIRSerial.wav";

    SF_INFO fileInfo;
    std::vector<float> audioData;
    std::memset(&fileInfo, 0, sizeof(fileInfo));

    auto startRead = high_resolution_clock::now();
    readWavFile(inputFile, audioData, fileInfo);
    auto endRead = high_resolution_clock::now();

    normalizeAudio(audioData);

    auto startBandpass = high_resolution_clock::now();
    applyBandpassFilter(audioData, fileInfo.samplerate, 300.0f, 3000.0f);
    auto endBandpass = high_resolution_clock::now();

    auto startWrite = high_resolution_clock::now();
    writeWavFile(outputFile1, audioData, fileInfo);
    auto endWrite = high_resolution_clock::now();
   
    readWavFile(inputFile, audioData, fileInfo);

    auto startNotch = high_resolution_clock::now();
    applyNotchFilter(audioData, fileInfo.samplerate, 50.0f, 2);
    auto endNotch = high_resolution_clock::now();
    writeWavFile(outputFile2, audioData, fileInfo);

    readWavFile(inputFile, audioData, fileInfo);

    std::vector<float> firCoefficients = {0.1, 0.15, 0.5, 0.15, 0.1};
    auto startFIR = high_resolution_clock::now();
    applyFIRFilter(audioData, firCoefficients);
    auto endFIR = high_resolution_clock::now();
    writeWavFile(outputFile3, audioData, fileInfo);

    readWavFile(inputFile, audioData, fileInfo);

    std::vector<float> b = {0.1, 0.15, 0.5, 0.15, 0.1};
    std::vector<float> a = {1.0, -0.5, 0.25};
    auto startIIR = high_resolution_clock::now();
    applyIIRFilter(audioData, b, a);
    auto endIIR = high_resolution_clock::now();
    writeWavFile(outputFile4, audioData, fileInfo);

    auto end = high_resolution_clock::now();

    auto durationRead = duration_cast<milliseconds>(endRead - startRead).count();
    auto durationWrite = duration_cast<milliseconds>(endWrite - startWrite).count();
    auto durationBandpass = duration_cast<milliseconds>(endBandpass - startBandpass).count();
    auto durationNotch = duration_cast<milliseconds>(endNotch - startNotch).count();
    auto durationFIR = duration_cast<milliseconds>(endFIR - startFIR).count();
    auto durationIIR = duration_cast<milliseconds>(endIIR - startIIR).count();
    auto totalDuration = duration_cast<milliseconds>(end - start).count();

    cout << "Time taken to read data: " << durationRead << " ms" << endl;
    cout << "Time taken to write data: " << durationWrite << " ms" << endl;
    cout << "Time taken to apply Band-pass Filter: " << durationBandpass << " ms" << endl;
    cout << "Time taken to apply Notch Filter: " << durationNotch << " ms" << endl;
    cout << "Time taken to apply FIR Filter: " << durationFIR << " ms" << endl;
    cout << "Time taken to apply IIR Filter: " << durationIIR << " ms" << endl;
    cout << "Total execution time: " << totalDuration << " ms" << endl;

    return 0;
}
