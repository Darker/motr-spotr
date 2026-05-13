#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <array>
#include <iostream>
#include <fstream>

#include <implot/implot.h>
#include <nucleo_shared/adc_stream_constants.h>
#include <nucleo_shared/byte_stuff.h>
#include <reader_lib/CDCReaderWindows.h>

#include <GLFW/glfw3.h>

#include "fft_stuff.h"
#include "FixedCirdularQueue.h"

using namespace hertz;

constexpr size_t FFT_SIZE = 4096;
constexpr size_t FFT_LINES = 800;
constexpr Hz FREQ_SAMPLE = 48_kHz;
constexpr Hz FREQ_FFT_MIN = 9_kHz;
constexpr Hz FREQ_FFT_MAX = 35_kHz;
constexpr size_t FFT_BATCH_SIZE = 1<<14;
std::mutex lockFFTData;

std::condition_variable cvNewFFTData;
std::vector<std::array<double, FFT_SIZE>> g_plotData;

std::atomic<bool> g_running{true};
std::atomic<bool> g_newData{false};

FixedCircularQueue<uint8_t[ADC_PACKED_CHUNK_LEN], 10> packedQueue;

GLFWwindow* CreateGuiWindow(int width, int height, const char* title)
{
    // Initialize GLFW once per process
    if (!glfwInit())
    {
        return nullptr;
    }

    // Request OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
    GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return nullptr;
    }

    // Activate OpenGL context
    glfwMakeContextCurrent(window);

    // Enable vsync (optional)
    glfwSwapInterval(1);

    return window;
}

static uint64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

void guiThreadFunc()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ImGuiContext* gctx = ImGui::CreateContext();
    ImPlotContext* pctx = ImPlot::CreateContext();

    ImGui::SetCurrentContext(gctx);
    ImPlot::SetCurrentContext(pctx);
    GLFWwindow* window = CreateGuiWindow(1024, 512, "ADC FFT");

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    // Custom colormap: 0=black, 0.5=red, 1=yellow
    static constexpr ImVec4 colors[3] = {
        ImVec4(0, 0, 0, 1),      // black
        ImVec4(1, 0, 0, 1),      // red
        ImVec4(1, 1, 0, 1)       // yellow
    };
    const int customRGBMap = ImPlot::AddColormap("RED_YELLOW_MAP", colors, 3, false);

    std::vector<std::array<double, FFT_SIZE>> fftRows;
    fftRows.resize(FFT_LINES);
    for(auto& row : fftRows)
    {
        row.fill(0.0);
    }
    size_t fftRowIndex = 0;
    bool rowsChanged = false;

    const auto predNewDataCv = [window]() 
    {
        return g_newData.load() || glfwWindowShouldClose(window) || !g_running.load();
    };

    constexpr int RENDER_SKIP = 5;


    while (!glfwWindowShouldClose(window) && g_running.load())
    {
        rowsChanged = false;
        
        {
            std::unique_lock<std::mutex> lock(lockFFTData);
            // Wait up to 5 ms for new FFT data
            cvNewFFTData.wait_for(lock, std::chrono::milliseconds(5), predNewDataCv);

            if(g_plotData.size() > 0)
            {
                for(size_t i=0; i<g_plotData.size(); ++i)
                {
                    auto& row = fftRows[fftRowIndex];
                    std::copy_n(g_plotData[i].begin(), FFT_SIZE, row.begin());
                    ++fftRowIndex;
                    if (fftRowIndex >= FFT_LINES)
                    {
                        fftRowIndex = 0;
                    }
                }

                g_plotData.resize(0);

                g_newData.store(false);
                rowsChanged = true;
            }
        }

        glfwPollEvents();
        if(!rowsChanged)
        {
            continue;
        }

        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("Spectrogram"))
        {
            if (ImPlot::BeginPlot("FFT Heatmap",
                ImVec2(-1, -1),                                     // expand to fit window
                ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText |
                ImPlotFlags_NoFrame | ImPlotFlags_NoMenus))
            {
                ImPlotSpec spec{};
                ImPlot::PushColormap(customRGBMap);
                ImPlot::PlotHeatmap(
                    "FFT",
                    reinterpret_cast<const double*>(fftRows.data()),
                    FFT_LINES,
                    (int)FFT_SIZE,
                    0.0f,
                    1.0f,
                    nullptr,
                    ImPlotPoint(FREQ_FFT_MIN, 0),
                    ImPlotPoint(FREQ_FFT_MAX, FFT_LINES)
                );

                ImPlot::PopColormap();
                ImPlot::EndPlot();
            }

        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::Shutdown();
    ImPlot::DestroyContext(pctx);
    ImGui::DestroyContext(gctx);
    
    g_running.store(false);
    packedQueue.notifyWaiters();
}

void unpack3to12(const uint8_t *in, size_t byte_count, uint16_t *out)
{
    size_t o = 0;

    for (size_t i = 0; i + 2 < byte_count; i += 3)
    {
        uint8_t b0 = in[i + 0];
        uint8_t b1 = in[i + 1];
        uint8_t b2 = in[i + 2];

        // A = [A11:A8 from b1 low nibble] [A7:A0 from b0]
        uint16_t a = (uint16_t)b0 | ((uint16_t)(b1 & 0x0F) << 8);

        // B = [B11:B4 from b2] [B3:B0 from b1 high nibble]
        uint16_t b = ((uint16_t)b2 << 4) | ((uint16_t)(b1 >> 4) & 0x0F);

        out[o++] = a;
        out[o++] = b;
    }
}

std::mutex lockIncomingSamples;
std::condition_variable cvIncomingSamples;
uint16_t currentSamples[ADC_BUFFER_HALF_SIZE_SAMPLES] = {0};
std::atomic<bool> newSamples(false);



void comReaderThreadFunc()
{
    CDCReaderWindows reader("COM8");

    uint8_t internalBuffer[ADC_PACKED_CHUNK_LEN];
    uint8_t magicBeginBuffer[sizeof(MAGIC_BEGIN)];
    uint8_t magicEndBuffer[sizeof(MAGIC_END)];
    size_t magicRead = 0;
    bool started = false;
    bool synced = false;

    while(g_running.load())
    {
        if(!synced)
        {
            while(!started && g_running.load())
            {
                // wait for magic start
                memmove(magicBeginBuffer, magicBeginBuffer+1, sizeof(MAGIC_BEGIN) - 1);
                reader.readBytesExact(magicBeginBuffer + sizeof(MAGIC_BEGIN) - 1, 1);
                ++magicRead;
                if(memcmp(magicBeginBuffer, MAGIC_BEGIN, sizeof(MAGIC_BEGIN)) == 0) {
                    started = true;
                    synced = true;
                    magicRead = 0;
                    break;
                }
                // else if(synced && magicRead >= sizeof(MAGIC_BEGIN)){
                //     std::cout << "Desynced!\n";
                //     synced = false;
                // }
            }
        }
        else 
        {
            reader.readBytesExact(magicBeginBuffer, sizeof(MAGIC_BEGIN));
            if (memcmp(magicBeginBuffer, MAGIC_BEGIN, sizeof(MAGIC_BEGIN)) != 0)
            {
                std::cout << "Desynced! Re-scanning...\n";
                synced = false;
                continue; // go back to sliding-window search
            }
            else {
                started = true;
                magicRead = 0;
            }
        }

        if(!started) {
            continue;
        }
        started = false;
        magicRead = 0;
        memset(magicBeginBuffer, 0, sizeof(MAGIC_BEGIN));
        // drain the unused data len header
        reader.readBytesExact(internalBuffer, 4);
        // raw data, collapsed to 2 shorts in 3 bytes
        reader.readBytesExact(internalBuffer, ADC_PACKED_CHUNK_LEN);

        // check if final magic is correct
        reader.readBytesExact(magicEndBuffer, sizeof(MAGIC_END));
        if(memcmp(magicEndBuffer, MAGIC_END, sizeof(MAGIC_END)) != 0) {
            std::cout << "Corrupted buffer, final magic does not fit!\n";
            continue;
        }
        {
            packedQueue.pushNextWaitSpace([internalBuffer](auto* val){
                memcpy(val, internalBuffer, ADC_PACKED_CHUNK_LEN);
            }, g_running);
        }
    }
    packedQueue.notifyWaiters();
}

void unpackerThread()
{
    uint8_t packedBuffer[ADC_PACKED_CHUNK_LEN];
    while(g_running.load())
    {
        bool gotSample = false;
        {
            auto lock = packedQueue.waitSize(1, g_running);
            if(packedQueue.sizeUnlocked() > 0)
            {
                memcpy(packedBuffer, packedQueue.atUnlocked(0), ADC_PACKED_CHUNK_LEN);
                packedQueue.popOldestUnlocked();
                gotSample = true;
                if(packedQueue.sizeUnlocked() > 5)
                {
                    std::cout << "WARNING: Unpacker cannot keep up with data!\n";
                }
            }
        }
        if(gotSample)
        {
            {
                std::lock_guard<std::mutex> lock(lockIncomingSamples);
                if(newSamples.load())
                {
                    std::cout << "WARNING: Waveform computer cannot keep up with data!\n";
                }
                unpack3to12(packedBuffer, ADC_PACKED_CHUNK_LEN, currentSamples);
                newSamples.store(true);
            }
            cvIncomingSamples.notify_all();
        }
    }
    cvIncomingSamples.notify_all();
}

struct FFTSample
{
    uint64_t timestampMs;
    std::array<double, FFT_SIZE> data;
};

FixedCircularQueue<FFTSample, 32> saveQueue;
void recordingThreadFn()
{
    std::ofstream out("output.bin", std::ios::binary);
    if (!out) {
        std::cout << "Failed to open recording file!\n";
        g_running = false;
        return;
    }

    // Temporary buffer for endian‑swapped doubles
    std::array<uint64_t, FFT_SIZE> be;

    uint64_t fftSize = FFT_SIZE;


    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        constexpr bool littleEndian = true;
    #else
        constexpr bool littleEndian = false;
    #endif

    if constexpr (littleEndian)
    {
        fftSize = bswap64(fftSize);
    }

    out.write(reinterpret_cast<const char*>(&fftSize), sizeof(fftSize));

    while (g_running.load())
    {
        // ---- 1) Wait for next sample (this is the ONLY lock) ----
        FFTSample sample;
        {
            auto l = saveQueue.waitSize(1, g_running);
            if (!g_running.load())
                break;
            const auto size = saveQueue.sizeUnlocked();
            if(size > 0)
            {
                if(size > 20)
                {
                    std::cout << "WARNING: save queue getting full " << size << "/" << 32 << "\n";
                }
                sample = saveQueue.atUnlocked(0);
                saveQueue.popOldestUnlocked();
            }
            else {
                continue;
            }
        }

        // ---- 2) Write timestamp (big‑endian uint64_t) ----
        uint64_t ts = sample.timestampMs;
        // std::cout << "TS: " << bswap64(bswap64(ts)) << "\n";
        if constexpr (littleEndian)
        {
            ts = bswap64(ts);
        }

        out.write(reinterpret_cast<const char*>(&ts), sizeof(ts));

        // ---- 3) Convert all doubles to big‑endian uint64_t ----
        for (size_t i = 0; i < FFT_SIZE; i++)
        {
            uint64_t bits;
            static_assert(sizeof(bits) == sizeof(sample.data[i]));
            std::memcpy(&bits, &sample.data[i], sizeof(bits));

            if constexpr (littleEndian)
            {
                bits = bswap64(bits);
            }

            be[i] = bits;
        }

        // ---- 4) Write entire FFT row in one go ----
        out.write(reinterpret_cast<const char*>(be.data()),
                  be.size() * sizeof(uint64_t));
    }
}


std::mutex mutexWaveform;
std::condition_variable cvWaveform;
std::vector<double> waveform;

void waveformConsumerThread()
{
    uint16_t currentSamplesCopy[ADC_BUFFER_HALF_SIZE_SAMPLES] = {0};
    constexpr uint16_t sampleMax = 0xFFF;
    constexpr double sampleMaxDouble = (double)sampleMax;
    bool gotNewSample = false;

    while(g_running.load())
    {
        {
            std::unique_lock lock(lockIncomingSamples);
            cvIncomingSamples.wait(lock, [](){return newSamples.load() || !g_running.load();});
            if(newSamples.load()) {
                newSamples.store(false);
                memcpy(currentSamplesCopy, currentSamples, ADC_BUFFER_HALF_SIZE_BYTES);
                memset(currentSamples, 0, ADC_BUFFER_HALF_SIZE_BYTES);
                gotNewSample = true;
            }
        }

        if(gotNewSample && g_running.load()) 
        {
            std::lock_guard lockWave{mutexWaveform};
            const size_t start = waveform.size();
            waveform.resize(waveform.size() + ADC_BUFFER_HALF_SIZE_SAMPLES);
            for(size_t i=0; i<ADC_BUFFER_HALF_SIZE_SAMPLES; ++i)
            {
                waveform[i+start] = ((double)(currentSamplesCopy[i]*2))/sampleMaxDouble - 1.0;
            }

            if(waveform.size() > 100000)
            {
                std::cout << "WARNING: Too much waveform samples, FFT is slow!\n";
                waveform.erase(waveform.begin(), waveform.begin() + FFT_BATCH_SIZE/2);
            }
            
            gotNewSample = false;
        }
        cvWaveform.notify_all();
    }
    cvWaveform.notify_all();
}

template <typename TValue, size_t VSize>
void addArray(const std::array<TValue, VSize>& from, std::array<TValue, VSize>& to, const bool substract = false)
{
    for(size_t i=0; i<VSize; ++i)
    {
        to[i] += substract ? -1*from[i] : from[i];
    }
}

template <typename TValue, size_t VSize>
void divideArray(std::array<TValue, VSize>& tgt, double divider)
{
    for(size_t i=0; i<VSize; ++i)
    {
        tgt[i] = tgt[i] / divider;
    }
}

template <typename TValue, size_t VSize>
void divideArrayTo(const std::array<TValue, VSize>& from, std::array<TValue, VSize>& to, double divider)
{
    for(size_t i=0; i<VSize; ++i)
    {
        to[i] = from[i] / divider;
    }
}


constexpr size_t numAverage = 9;
FixedCircularQueue<std::array<double, FFT_SIZE>, numAverage+1> fftQueue;
void fftAveragerThread()
{
    std::array<double, FFT_SIZE> average = {0.0};
    std::array<double, FFT_SIZE> dividedAverage = {0.0};
    constexpr int guiFrameSkip = 13;
    constexpr int recordingFrameSkip = 17;
    while(g_running.load())
    {
        {
            const auto lock = fftQueue.waitNext(g_running);
            if(numAverage > 1)
            {
                addArray(fftQueue.headUnlocked(), average);
                bool wasFull = false;
                if(fftQueue.isFullUnlocked())
                {
                    addArray(fftQueue.atUnlocked(0), average, true);
                    wasFull = true;
                    fftQueue.popOldestUnlocked();
                }

                if(!wasFull)
                {
                    continue;
                }
            }
            else
            {   if(fftQueue.sizeUnlocked() > 0 && fftQueue.start % guiFrameSkip == 0)
                {
                    std::copy_n(fftQueue.headUnlocked().data(), FFT_SIZE, dividedAverage.data());
                }
                
                fftQueue.popOldestUnlocked();
                // if(fftQueue.start % guiFrameSkip != 0)
                // {
                //     continue;
                // }
            }
        }
        const bool doGui = fftQueue.start % guiFrameSkip == 0;
        const bool doStore = fftQueue.start % recordingFrameSkip == 0;

        if(!doGui && !doStore)
        {
            continue;
        }
        if(numAverage > 1)
        {
            divideArrayTo(average, dividedAverage, (double)numAverage);
        }

        bool shouldNotifyGui = false;
        if(doGui)
        {
            std::lock_guard<std::mutex> lock(lockFFTData);
            g_plotData.push_back(dividedAverage);

            if(g_plotData.size() > 12)
            {
                std::cout << "WARNING: gui can't keep up with FFT samples!\n";
            }
            if(g_plotData.size() > 7)
            {
                g_newData.store(true);
                shouldNotifyGui = true;
            }
        }
        if(shouldNotifyGui)
        {
            cvNewFFTData.notify_all();
        }

        if(doStore)
        {
            const auto curTimestamp = nowMs();
            saveQueue.pushNextWaitSpace([&dividedAverage, curTimestamp](auto data) {
                std::copy_n(dividedAverage.data(), FFT_SIZE, data->data.data());
                data->timestampMs = curTimestamp;
            }, g_running);
        }
    }
    cvNewFFTData.notify_all();
    saveQueue.notifyWaiters();
}

int main(int argc, const char** argv)
{
    waveform.reserve(50000);

    std::thread guiThread(guiThreadFunc);
    std::thread nucleoThread(comReaderThreadFunc);
    std::thread unpackThread(unpackerThread);
    std::thread waveformThread(waveformConsumerThread);
    std::thread fftAvgThread(fftAveragerThread);
    std::thread recordingThread(recordingThreadFn);
    // Later: reader thread will push data and notify
    // For now, simulate notifications
    std::vector<double> curWaveform;
    curWaveform.reserve(FFT_BATCH_SIZE);
    size_t waveformOffset = 0;
    std::array<double, FFT_SIZE> fftBuffer = {0};
    std::vector<std::complex<double>> fftInternalBuffer;
    while(true)
    {
        {
            std::unique_lock lockWave{mutexWaveform};
            cvWaveform.wait(lockWave, [&waveformOffset](){ 
                return waveform.size() < waveformOffset || waveform.size() - waveformOffset >= FFT_BATCH_SIZE || !g_running.load(); 
            });
            if(waveformOffset >= waveform.size())
            {
                waveformOffset = 0;
            }
            if(waveform.size() - waveformOffset >= FFT_BATCH_SIZE)
            {
                curWaveform.insert(curWaveform.begin(), waveform.begin() + waveformOffset, waveform.begin()+FFT_BATCH_SIZE + waveformOffset);
                // waveform.erase(waveform.begin(), waveform.begin() + 5000);
                waveformOffset += FFT_BATCH_SIZE/2;
                if(waveformOffset > (FFT_BATCH_SIZE*3))
                {
                    if(waveform.size() - waveformOffset > FFT_BATCH_SIZE*2)
                    {
                        std::cout << "WARNING: FFT calculator had to drop samples!\n";
                        waveform.erase(waveform.begin(), waveform.begin() + FFT_BATCH_SIZE/2);
                    }
                    else
                    {
                        waveform.erase(waveform.begin(), waveform.begin() + waveformOffset);
                    }
                    
                    waveformOffset = 0;
                }
            }
        }

        if(!g_running.load()) {
            break;
        }

        if(curWaveform.size() >= FFT_BATCH_SIZE)
        {
            computeSpectrumFaster(
                curWaveform,
                fftInternalBuffer,
                500_kHz,     // sampling rate
                FREQ_FFT_MIN,         // fmin
                FREQ_FFT_MAX,     // fmax
                fftBuffer
            );

            fftQueue.pushNextWaitSpace([fftBuffer](std::array<double, FFT_SIZE>* value){
                std::copy_n(fftBuffer.data(), FFT_SIZE, value->data());
            }, g_running);

            curWaveform.resize(0);
        }

        if(!g_running.load()) {
            fftQueue.notifyWaiters();
            saveQueue.notifyWaiters();
            break;
        }
    }
    g_running.store(false);

    packedQueue.notifyWaiters();
    fftQueue.notifyWaiters();
    
    cvNewFFTData.notify_one();
    nucleoThread.join();
    waveformThread.join();
    fftAvgThread.join();
    unpackThread.join();
    recordingThread.join();

    guiThread.join();
    return 0;
}