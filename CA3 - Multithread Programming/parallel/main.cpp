#include <iostream>
#include <sndfile.h>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <pthread.h>

using namespace std;
using namespace std::chrono;

struct BandpassFilterArgs {
    vector<float>* data;
    int sampleRate;
    float lowCutoff;
    float highCutoff;
    size_t start;
    size_t end;
};

struct NotchFilterArgs {
    vector<float>* data;
    int sampleRate;
    float notchFrequency;
    int n;
    size_t start;
    size_t end;
};

struct FIRFilterArgs {
    vector<float>* data;
    const vector<float>* coefficients;
    size_t start;
    size_t end;
};

struct IIRFilterArgs {
    vector<float>* data;
    const vector<float>* b;
    const vector<float>* a;
    size_t start;
    size_t end;
};

struct ReadArgs {
    const std::string* inputFile;
    vector<float>* data;
    size_t start;
    size_t end;
    SF_INFO fileInfo;
};

struct WriteArgs {
    const std::string* outputFile;
    const vector<float>* data;
    size_t start;
    size_t end;
    SF_INFO fileInfo;
};




void* applyBandpassFilterSegment(void* args) {
    BandpassFilterArgs* filterArgs = (BandpassFilterArgs*)args;
    vector<float>* data = filterArgs->data;
    int sampleRate = filterArgs->sampleRate;
    float lowCutoff = filterArgs->lowCutoff;
    float highCutoff = filterArgs->highCutoff;
    size_t start = filterArgs->start;
    size_t end = filterArgs->end;

    float deltaF = highCutoff - lowCutoff;

    for (size_t i = start; i < end; ++i) {
        float f = static_cast<float>(i) / data->size() * sampleRate;
        if (f < lowCutoff || f > highCutoff) {
            (*data)[i] = 0.0f;
        } else {
            (*data)[i] *= (f * f) / (f * f + deltaF * deltaF);
        }
    }

    return NULL;
}

void applyBandpassFilter(vector<float>& data, int sampleRate, float lowCutoff, float highCutoff) {
    int numThreads = 4;
    pthread_t threads[numThreads];
    BandpassFilterArgs args[numThreads];
    size_t segmentSize = data.size() / numThreads;

    for (int i = 0; i < numThreads; ++i) {
        args[i] = {&data, sampleRate, lowCutoff, highCutoff, i * segmentSize, (i + 1) * segmentSize};
        if (i == numThreads - 1) {
            args[i].end = data.size();
        }
        pthread_create(&threads[i], NULL, applyBandpassFilterSegment, &args[i]);
    }

    for (int i = 0; i < numThreads; ++i) {
        pthread_join(threads[i], NULL);
    }
}

void* applyNotchFilterSegment(void* args) {
    NotchFilterArgs* filterArgs = (NotchFilterArgs*)args;
    vector<float>* data = filterArgs->data;
    int sampleRate = filterArgs->sampleRate;
    float notchFrequency = filterArgs->notchFrequency;
    int n = filterArgs->n;
    size_t start = filterArgs->start;
    size_t end = filterArgs->end;

    for (size_t i = start; i < end; ++i) {
        float f = static_cast<float>(i) / data->size() * sampleRate;
        float H_f = 1.0f / (1.0f + std::pow((notchFrequency / f), 2 * n));
        (*data)[i] *= H_f;
    }

    return NULL;
}

void applyNotchFilter(vector<float>& data, int sampleRate, float notchFrequency, int n) {
    int numThreads = 8;
    pthread_t threads[numThreads];
    NotchFilterArgs args[numThreads];
    size_t segmentSize = data.size() / numThreads;

    for (int i = 0; i < numThreads; ++i) {
        args[i] = {&data, sampleRate, notchFrequency, n, i * segmentSize, (i + 1) * segmentSize};
        if (i == numThreads - 1) {
            args[i].end = data.size();
        }
        pthread_create(&threads[i], NULL, applyNotchFilterSegment, &args[i]);
    }

    for (int i = 0; i < numThreads; ++i) {
        pthread_join(threads[i], NULL);
    }
}

void* readWavFileSegment(void* args) {
    ReadArgs* readArgs = (ReadArgs*)args;
    const std::string* inputFile = readArgs->inputFile;
    vector<float>* data = readArgs->data;
    size_t start = readArgs->start;
    size_t end = readArgs->end;
    SF_INFO fileInfo = readArgs->fileInfo;

    SNDFILE* inFile = sf_open(inputFile->c_str(), SFM_READ, &fileInfo);
    if (!inFile) {
        std::cerr << "Error opening input file: " << sf_strerror(NULL) << std::endl;
        exit(1);
    }

    sf_seek(inFile, start / fileInfo.channels, SEEK_SET);
    sf_readf_float(inFile, data->data() + start, (end - start) / fileInfo.channels);
    sf_close(inFile);

    return NULL;
}


void readWavFile(const std::string& inputFile, std::vector<float>& data, SF_INFO& fileInfo) {
    SNDFILE* inFile = sf_open(inputFile.c_str(), SFM_READ, &fileInfo);
    if (!inFile) {
        std::cerr << "Error opening input file: " << sf_strerror(NULL) << std::endl;
        exit(1);
    }

    data.resize(fileInfo.frames * fileInfo.channels);
    sf_close(inFile);

    int numThreads = 8;
    pthread_t threads[numThreads];
    ReadArgs args[numThreads];
    size_t segmentSize = data.size() / numThreads;

    for (int i = 0; i < numThreads; ++i) {
        args[i] = {&inputFile, &data, i * segmentSize, (i + 1) * segmentSize, fileInfo};
        if (i == numThreads - 1) {
            args[i].end = data.size();
        }
        pthread_create(&threads[i], NULL, readWavFileSegment, &args[i]);
    }

    for (int i = 0; i < numThreads; ++i) {
        pthread_join(threads[i], NULL);
    }
}


void* writeWavFileSegment(void* args) {
    WriteArgs* writeArgs = (WriteArgs*)args;
    const std::string* outputFile = writeArgs->outputFile;
    const vector<float>* data = writeArgs->data;
    size_t start = writeArgs->start;
    size_t end = writeArgs->end;
    SF_INFO fileInfo = writeArgs->fileInfo;

    SNDFILE* outFile = sf_open(outputFile->c_str(), SFM_WRITE, &fileInfo);
    if (!outFile) {
        std::cerr << "Error opening output file: " << sf_strerror(NULL) << std::endl;
        exit(1);
    }

    sf_seek(outFile, start / fileInfo.channels, SEEK_SET);
    sf_writef_float(outFile, data->data() + start, (end - start) / fileInfo.channels);
    sf_close(outFile);

    return NULL;
}


void writeWavFile(const std::string& outputFile, const std::vector<float>& data, SF_INFO& fileInfo) {
    int numThreads = 8;
    pthread_t threads[numThreads];
    WriteArgs args[numThreads];
    size_t segmentSize = data.size() / numThreads;

    for (int i = 0; i < numThreads; ++i) {
        args[i] = {&outputFile, &data, i * segmentSize, (i + 1) * segmentSize, fileInfo};
        if (i == numThreads - 1) {
            args[i].end = data.size();
        }
        pthread_create(&threads[i], NULL, writeWavFileSegment, &args[i]);
    }

    for (int i = 0; i < numThreads; ++i) {
        pthread_join(threads[i], NULL);
    }
}


void normalizeAudio(std::vector<float>& data) {
    float maxAmplitude = *std::max_element(data.begin(), data.end(), [](float a, float b) { return std::abs(a) < std::abs(b); });
    if (maxAmplitude > 0) {
        for (auto& sample : data) {
            sample /= maxAmplitude;
        }
    }
}
void* applyFIRFilterSegment(void* args) {
    FIRFilterArgs* filterArgs = (FIRFilterArgs*)args;
    vector<float>* data = filterArgs->data;
    const vector<float>* coefficients = filterArgs->coefficients;
    size_t start = filterArgs->start;
    size_t end = filterArgs->end;
    int M = coefficients->size();
    
    vector<float> filteredData(data->size(), 0.0f);

    for (size_t n = start; n < end; ++n) {
        for (int k = 0; k < M; ++k) {
            if (n >= k) {
                filteredData[n] += (*coefficients)[k] * (*data)[n - k];
            }
        }
    }

    for (size_t n = start; n < end; ++n) {
        (*data)[n] = filteredData[n];
    }

    return NULL;
}

void applyFIRFilter(vector<float>& data, const vector<float>& coefficients) {
    int numThreads = 8;
    pthread_t threads[numThreads];
    FIRFilterArgs args[numThreads];
    size_t segmentSize = data.size() / numThreads;

    for (int i = 0; i < numThreads; ++i) {
        args[i] = {&data, &coefficients, i * segmentSize, (i + 1) * segmentSize};
        if (i == numThreads - 1) {
            args[i].end = data.size();
        }
        pthread_create(&threads[i], NULL, applyFIRFilterSegment, &args[i]);
    }

    for (int i = 0; i < numThreads; ++i) {
        pthread_join(threads[i], NULL);
    }
}

void* applyIIRFilterSegment(void* args) {
    IIRFilterArgs* filterArgs = (IIRFilterArgs*)args;
    vector<float>* data = filterArgs->data;
    const vector<float>* b = filterArgs->b;
    const vector<float>* a = filterArgs->a;
    size_t start = filterArgs->start;
    size_t end = filterArgs->end;
    int P = b->size();
    int Q = a->size();

    vector<float> filteredData(data->size(), 0.0f);

    for (size_t n = start; n < end; ++n) {
        for (int i = 0; i < P; ++i) {
            if (n >= i) {
                filteredData[n] += (*b)[i] * (*data)[n - i];
            }
        }
        for (int j = 1; j < Q; ++j) {
            if (n >= j) {
                filteredData[n] -= (*a)[j] * filteredData[n - j];
            }
        }
    }

    for (size_t n = start; n < end; ++n) {
        (*data)[n] = filteredData[n];
    }

    return NULL;
}

void applyIIRFilter(vector<float>& data, const vector<float>& b, const vector<float>& a) {
    int numThreads = 4;
    pthread_t threads[numThreads];
    IIRFilterArgs args[numThreads];
    size_t segmentSize = data.size() / numThreads;

    for (int i = 0; i < numThreads; ++i) {
        args[i] = {&data, &b, &a, i * segmentSize, (i + 1) * segmentSize};
        if (i == numThreads - 1) {
            args[i].end = data.size();
        }
        pthread_create(&threads[i], NULL, applyIIRFilterSegment, &args[i]);
    }

    for (int i = 0; i < numThreads; ++i) {
        pthread_join(threads[i], NULL);
    }
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <../input.wav>" << std::endl;
        return 1;
    }

    auto start = high_resolution_clock::now();

    std::string inputFile = argv[1];
    std::string outputFile1 = "outputBandpassParallel.wav";
    std::string outputFile2 = "outputNotchParallel.wav";
    std::string outputFile3 = "outputFIRParallel.wav";
    std::string outputFile4 = "outputIIRParallel.wav";

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
