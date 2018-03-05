#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <stdio.h>

#include <thread> // for sleeping

#include <math.h>
#ifndef M_PI
#define M_PI 3.141592653589793
#endif

#define max(x,y) (((x)>(y))?(x):(y))
#define min(x,y) (((x)<(y))?(x):(y))

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
    auto device = open("/dev/dsp", O_WRONLY, O_NONBLOCK);
    if(device < 0) return puts("Failed to open device"), 0;
   
    int channels = 2;
    int format = AFMT_S16_LE;
    int frequency = 48000;
   
    int samples = 1024;
   
    ioctl(device, SNDCTL_DSP_CHANNELS, &channels);
    ioctl(device, SNDCTL_DSP_SETFMT, &format);
    ioctl(device, SNDCTL_DSP_SPEED, &frequency);
   
    audio_buf_info space;
    ioctl(device, SNDCTL_DSP_GETOSPACE, &space);
    printf("Space: %u\n", space.bytes);
   
    int buffer_size = space.bytes;
   
    uint64_t t = 0;
   
    while(t < 100000)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ioctl(device, SNDCTL_DSP_GETOSPACE, &space);
        int padding = buffer_size - space.bytes;
        int overhead = samples - padding/4;
       
        uint64_t render = max(0, overhead);
        if(render > 0)
        {
            mix_sine(mixbuffer, render, frequency, channels, t);
            t += render;
           
            for(uint64_t i = 0; i < render; i++)
            {
                int16_t left  = round(mixbuffer[i*channels + 0]*0x7FFF);
                int16_t right = round(mixbuffer[i*channels + 1]*0x7FFF);
                uint32_t frame = (uint16_t(right)<<16) | uint16_t(left);
               
                write(device, &frame, 4);
            }
        }
    }
   
    #ifdef SNDCTL_DSP_HALT
    ioctl(device, SNDCTL_DSP_HALT, nullptr);
    #else
    ioctl(device, SNDCTL_DSP_RESET, nullptr);
    #endif
    close(device);
    return 0;
}
