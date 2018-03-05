#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

#include <thread> // for sleep

#include <math.h>
#ifndef M_PI
#define M_PI 3.141592653589793
#endif

#define max(x,y) (((x)>(y))?(x):(y))
#define min(x,y) (((x)<(y))?(x):(y))

double freq = 100;
double volume = 0.2;

float mixbuffer[4096*256];
int16_t outbuffer[4096*256];
void mix_sine(float * buffer, uint64_t count, uint64_t samplerate, uint64_t channels, uint64_t time)
{
    double nyquist = samplerate/2.0;
    for(uint64_t i = 0; i < count; i++)
    {
        for(uint64_t c = 0; c < channels; c++)
        {
            buffer[i*channels + c] = sin((i+time)*M_PI/nyquist*freq)*volume;
        }
    }
}

int main()
{
    snd_pcm_t * device;
    snd_pcm_hw_params_t * parameters;

//    if(snd_pcm_open(&device, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0)
    if(snd_pcm_open(&device, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
        return puts("Failed to open ALSA device"), 0;
       
    if(snd_pcm_hw_params_malloc(&parameters) < 0)
        return puts("Failed to allocate device parameter structure"), 0;
             
    if(snd_pcm_hw_params_any(device, parameters) < 0)
        return puts("Failed to init device parameter structure"), 0;

    if(snd_pcm_hw_params_set_access(device, parameters, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
        return puts("Failed to set access type"), 0;

    if(snd_pcm_hw_params_set_format(device, parameters, SND_PCM_FORMAT_S16_LE) < 0)
        return puts("Failed to set sample format"), 0;

    unsigned int samplerate = 44100;
    if(snd_pcm_hw_params_set_rate_near(device, parameters, &samplerate, 0) < 0)
        return puts("Failed to set sample rate"), 0;

    int channels = 2;
    if(snd_pcm_hw_params_set_channels(device, parameters, channels) < 0)
        return puts("Failed to set channel count"), 0;
   
    printf("actual sample rate %d\n", samplerate);
   
    snd_pcm_uframes_t period;
    int dir;
   
    if(snd_pcm_hw_params_get_period_size_min(parameters, &period, &dir) < 0)
        return puts("Failed to get period size"), 0;
   
    printf("min period size: %lu\n", period);
   
    if(snd_pcm_hw_params_set_period_size(device, parameters, period, dir) < 0)
        return puts("Failed to set period size to the minimum provided by it"), 0;
   
    snd_pcm_uframes_t samples = max(period, 1024);
    printf("pretend buffer size is %lu\n", samples);
   
    if(snd_pcm_hw_params_set_buffer_size_near(device, parameters, &samples) < 0)
        return puts("Failed to set buffer size to something near our sample window"), 0;

    if(snd_pcm_hw_params(device, parameters) < 0)
        return puts("Failed to set device parameters"), 0;

    snd_pcm_hw_params_free(parameters);

    if (snd_pcm_prepare(device) < 0)
        return puts("Failed to prepare device"), 0;
   
    auto buffer_size = snd_pcm_avail_update(device);
    if(buffer_size < 0)
        return puts("Error when measuring approximate buffer size"), 0;
   
    printf("apparent buffer size is %lu\n", buffer_size);
   
    uint64_t t = 0;
    while(t < 100000)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto avail = snd_pcm_avail_update(device);
        while(avail == -EPIPE or avail == -ESTRPIPE)
        {
            snd_pcm_recover(device, avail, 0);
            avail = snd_pcm_avail_update(device);
        }
        int padding = buffer_size - avail;
       
        int render = min(uint64_t(buffer_size), samples) - padding;
       
        if(render > 0)
        {
            mix_sine(mixbuffer, render, samplerate, 2, t);
            t += render;
           
            for(int i = 0; i < render*2; i++)
                outbuffer[i] = round(mixbuffer[i]*0x7FFF);
           
            auto written = snd_pcm_writei(device, outbuffer, render);
           
            while(written == -EPIPE or written == -ESTRPIPE)
            {
                snd_pcm_recover(device, written, 0);
                written = snd_pcm_writei(device, outbuffer, render);
            }
        }
    }

    snd_pcm_close(device);
    return 0;
}
