#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <array>
#include <iostream>

#include <implot/implot.h>
#include <nucleo_shared/adc_stream_constants.h>
#include <reader_lib/CDCReaderWindows.h>

#include <GLFW/glfw3.h>

#include "fft_stuff.h"

using namespace hertz;

constexpr size_t FFT_SIZE = 4096;
constexpr size_t FFT_LINES = 256;
constexpr Hz FREQ_SAMPLE = 48_kHz;
constexpr Hz FREQ_FFT_MAX = 10_kHz;
std::mutex lockFFTData;

std::condition_variable cvNewFFTData;



std::vector<double> g_plotData;
std::atomic<bool> g_running{true};
std::atomic<bool> g_newData{false};

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

    while (!glfwWindowShouldClose(window) && g_running.load())
    {
        {
            std::unique_lock<std::mutex> lock(lockFFTData);
            // Wait up to 5 ms for new FFT data
            cvNewFFTData.wait_for(lock, std::chrono::milliseconds(5));

            if(g_newData.load())
            {
                auto& row = fftRows[fftRowIndex];
                std::copy_n(g_plotData.begin(), FFT_SIZE, row.begin());
                ++fftRowIndex;
                if (fftRowIndex >= FFT_LINES)
                {
                    fftRowIndex = 0;
                }
                g_newData.store(false);
            }
        }

        glfwPollEvents();

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


                // Build contiguous buffer
                std::vector<double> heatmap;
                heatmap.resize(fftRows.size() * FFT_SIZE);

                for (size_t row = 0; row < fftRows.size(); row++) {
                    for (int col = 0; col < FFT_SIZE; col++) {
                        heatmap[row * FFT_SIZE + col] = fftRows[row][col];
                    }
                }

                ImPlotSpec spec{};
                ImPlot::PushColormap(customRGBMap);
                ImPlot::PlotHeatmap(
                    "FFT",
                    heatmap.data(),
                    FFT_LINES,
                    (int)FFT_SIZE,
                    0.0f,
                    1.0f,
                    nullptr,
                    ImPlotPoint(0, 0),
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
                reader.readBytes(magicBeginBuffer + sizeof(MAGIC_BEGIN) - 1, 1);
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
            reader.readBytes(magicBeginBuffer, sizeof(MAGIC_BEGIN));
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
        reader.readBytes(internalBuffer, 4);
        // raw data, collapsed to 2 shorts in 3 bytes
        reader.readBytes(internalBuffer, ADC_PACKED_CHUNK_LEN);

        // check if final magic is correct
        reader.readBytes(magicEndBuffer, sizeof(MAGIC_END));
        if(memcmp(magicEndBuffer, MAGIC_END, sizeof(MAGIC_END)) != 0) {
            std::cout << "Corrupted buffer, final magic does not fit!\n";
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(lockIncomingSamples);
            if(newSamples.load())
            {
                std::cout << "WARNING: FFT sampler cannot keep up with data!\n";
            }
            unpack3to12(internalBuffer, ADC_PACKED_CHUNK_LEN, currentSamples);
            newSamples.store(true);
            cvIncomingSamples.notify_all();
        }
    }
    cvIncomingSamples.notify_all();
}

std::mutex mutexWaveform;
std::condition_variable cvWaveform;
std::vector<double> waveform;
std::atomic<size_t> waveformSize{0};

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

            waveform.reserve(waveform.size() + ADC_BUFFER_HALF_SIZE_SAMPLES);
            waveformSize.store(waveform.size() + ADC_BUFFER_HALF_SIZE_SAMPLES);
            for(size_t i=0; i<ADC_BUFFER_HALF_SIZE_SAMPLES; ++i)
            {
                waveform.push_back(((double)(currentSamplesCopy[i]*2))/sampleMaxDouble - 1.0);
            }
            
            gotNewSample = false;
        }
        cvWaveform.notify_all();
    }
    cvWaveform.notify_all();
}


int main(int argc, const char** argv)
{
    std::thread guiThread(guiThreadFunc);
    std::thread nucleoThread(comReaderThreadFunc);
    std::thread waveformThread(waveformConsumerThread);
    // Later: reader thread will push data and notify
    // For now, simulate notifications
    std::vector<double> curWaveform;
    curWaveform.reserve(10000);
    while(true)
    {
        // 1) Genrate mock waveform (time-domain)
        // std::vector<double> samples(2*4096);
        // std::vector<FakeTone> tones = {
        //      {440.0, 0.8},  // A4
        //      {1200.0, 0.4}, // mid tone
        //      {5000.0, 0.2}, // high tone
        //      {9000.0, 0.1, 1600_Hz}  // very high tone
        // };
        // generateMockSignal(samples, FREQ_SAMPLE, tones);   // fs = 48 kHz

        // convert samples to +- 1
        // constexpr uint16_t sampleMax = 0xFFF;
        // constexpr double sampleMaxDouble = (double)sampleMax;
        // waveform.reserve(waveform.size() + ADC_BUFFER_HALF_SIZE_SAMPLES);
        // {
        //     std::unique_lock lock(lockIncomingSamples);
        //     cvIncomingSamples.wait(lock, [](){return newSamples.load() || !g_running.load();});
        //     if(newSamples.load()) {
        //         newSamples.store(false);
        //         for(size_t i=0; i<ADC_BUFFER_HALF_SIZE_SAMPLES; ++i)
        //         {
        //             waveform.push_back(((double)(currentSamples[i]*2))/sampleMaxDouble - 1.0);
        //         }
        //         memset(currentSamples, 0, ADC_BUFFER_HALF_SIZE_BYTES);
        //     }
        // }
        
        {
            std::unique_lock lockWave{mutexWaveform};
            cvWaveform.wait(lockWave, [](){return waveformSize.load() >= 10000 || !g_running.load();});
            if(waveform.size() >= 10000)
            {
                curWaveform.insert(curWaveform.begin(), waveform.begin(), waveform.begin()+10000);
                waveform.erase(waveform.begin(), waveform.begin() + 5000);
                waveformSize.store(waveform.size());
            }
        }

        if(!g_running.load()) {
            break;
        }

        if(curWaveform.size() >= 10000)
        {
            {
                std::lock_guard<std::mutex> lock(lockFFTData);
                g_plotData = computeSpectrum(
                    curWaveform,
                    500_kHz,     // sampling rate
                    0.0,         // fmin
                    FREQ_FFT_MAX,     // fmax
                    FFT_SIZE         // output buckets
                );
                g_newData.store(true);
                cvNewFFTData.notify_one();
            }
            curWaveform.resize(0);
        }            


        if(!g_running.load()) {
            break;
        }
    }

    g_running.store(false);
    cvNewFFTData.notify_one();
    nucleoThread.join();
    waveformThread.join();

    guiThread.join();
    return 0;
}