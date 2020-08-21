//
// Created by Administrator on 2019/8/9.
//

#ifndef FFJNIPLAYER_VIDEOCHANNEL_H
#define FFJNIPLAYER_VIDEOCHANNEL_H


#include "BaseChannel.h"

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
};

typedef void (*RenderCallback)(uint8_t *, int, int, int);

class VideoChannel : public BaseChannel {
public:
    VideoChannel(int id, AVCodecContext *codecContext, int fps);

    ~VideoChannel();

    void start();

    void stop();

    void video_decode();

    void video_play();

    void setRenderCallback(RenderCallback renderCallback);

private:
    pthread_t pid_video_decode;
    pthread_t pid_video_play;
    RenderCallback renderCallback;
    int fps;
};


#endif //FFJNIPLAYER_VIDEOCHANNEL_H
