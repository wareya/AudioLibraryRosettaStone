// -lavrt -lole32 -lksuser

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <stdint.h>

#include <thread> // for sleeping

IMMDeviceEnumerator* enumerator = nullptr;
IMMDevice* audioDevice = nullptr;
IPropertyStore* propertyStore = nullptr;
IAudioClient* audioClient = nullptr;
IAudioRenderClient* renderClient = nullptr;
WAVEFORMATEX* waveFormat = nullptr;
WAVEFORMATEXTENSIBLE* waveFormatEx = nullptr;
PROPVARIANT propVariant;
HANDLE taskHandle = nullptr;
REFERENCE_TIME devicePeriod = 0;
uint32_t bufferSize;
uint32_t numchannels = 2;

#include <math.h>
#ifndef M_PI
#define M_PI 3.141592653589793
#endif

#define max(x,y) (((x)>(y))?(x):(y))

int type = -1; // 0: 16-bit signed int; 1: 32-bit float; -1: unsupported

double freq = 100;
double volume = 0.2;

float mixbuffer[4096*256];
void mix_sine(float * buffer, uint64_t count, uint64_t samplerate, uint64_t channels, uint64_t time)
{
    double nyquist = samplerate/2.0;
    for(uint64_t i = 0; i < count; i++)
    {
        for(uint64_t c = 0; c < channels; c++)
        {
            buffer[i*channels + c] = sin((i+time)*M_PI*freq/nyquist)*volume;
        }
    }
}

int main()
{
    // initialize COM
    CoInitialize(nullptr);
    // load device enumerator
    if(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&enumerator) != S_OK) return puts("a"), false;
    // get default output
    if(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &audioDevice) != S_OK) return false;
    // activate endpoint
    if(audioDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&audioClient) != S_OK) return puts("b"), false;
   
    // get format of endpoint
    if(audioClient->GetMixFormat(&waveFormat) != S_OK) return puts("c"), false;
    if(waveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        waveFormatEx = (WAVEFORMATEXTENSIBLE*)waveFormat;
    if(waveFormat->wBitsPerSample == 16 and (!waveFormatEx or waveFormatEx->SubFormat == KSDATAFORMAT_SUBTYPE_PCM))
        type = 0;
    if(waveFormatEx and waveFormatEx->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT and waveFormat->wBitsPerSample == 32)
        type = 1;
    if(type == -1)
        return puts("Unsupported format"), 0;
   
    printf("Type: %d\n", type);
    printf("Channels: %d\n", waveFormat->nChannels);
    if(waveFormat->nChannels == 1)
        numchannels = 1;
    else
        numchannels = 2;
    printf("(rendering to %d channels)\n", numchannels);
   
   
    uint32_t samples = 512;
   
    uint32_t latency = double(samples)/waveFormat->nSamplesPerSec*10'000'000; // 10: 100ns -> 1us; 1000: us -> ms; 1000: ms -> s
    printf("asking for %fms buffer size\n", double(samples)/waveFormat->nSamplesPerSec*1000);
   
    // open endpoint
    if(audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, latency, 0, waveFormat, nullptr) != S_OK) return puts("f"), false;
    DWORD taskIndex = 0;
    taskHandle = AvSetMmThreadCharacteristics("Audio", &taskIndex);
   
    // set up render client
    if(audioClient->GetService(IID_IAudioRenderClient, (void**)&renderClient) != S_OK) return puts("g"), false;
    // needed metadata
    if(audioClient->GetBufferSize(&bufferSize) != S_OK) return puts("h"), false;
   
    // info
    REFERENCE_TIME default_period, min_period;
    if(audioClient->GetDevicePeriod(&default_period, &min_period) != S_OK) return puts("e"), false;
    printf("actual buffer size: %fms\n", double(bufferSize)/waveFormat->nSamplesPerSec*1000);
    printf("default period: %fms\n minimum period: %fms\n", double(default_period)/10'000, double(min_period)/10'000);

    // start
    if(audioClient->Start() != S_OK) return puts("d"), false;
   
    uint64_t t = 0;
   
    // reduce latency by memeing (note: terrible idea, just for example)
    //uint32_t padding_offset = bufferSize - double(default_period)/10'000'000*waveFormat->nSamplesPerSec;
    uint32_t padding_offset = 0;
   
    while(t < 100000)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        uint32_t padding = 0;
        auto err = audioClient->GetCurrentPadding(&padding);
        if(err != S_OK)
            printf("GetCurrentPadding threw error: %ld\n", err);
       
        auto render = max(0, int64_t(bufferSize)-int64_t(padding+padding_offset));
        if(render > 0)
        {
            mix_sine(mixbuffer, render, waveFormat->nSamplesPerSec, numchannels, t);
            t += render;
           
            unsigned char * backend_buffer;
            err = renderClient->GetBuffer(render, &backend_buffer);
            if(err != S_OK)
                printf("GetBuffer threw error: %ld\n", err);
           
            uint64_t i = 0;
            for(uint64_t j = 0; j < render; j++)
            {
                for(uint64_t c = 0; c < waveFormat->nChannels; c++)
                {
                    if(c < numchannels)
                    {
                        if(type == 1)
                            ((float*)(backend_buffer))[j*waveFormat->nChannels+c] = mixbuffer[i++];
                        if(type == 0)
                            ((int16_t*)(backend_buffer))[j*waveFormat->nChannels+c] = round(mixbuffer[i++]*0x7FFF);
                    }
                    else
                    {
                        if(type == 1)
                            ((float*)(backend_buffer))[j*waveFormat->nChannels+c] = 0;
                        if(type == 0)
                            ((int16_t*)(backend_buffer))[j*waveFormat->nChannels+c] = 0;
                    }
                }
            }
           
            err = renderClient->ReleaseBuffer(render, 0);
            if(err != S_OK)
                printf("ReleaseBuffer threw error: %ld\n", err);
        }
    }
   
    return 0;
}
