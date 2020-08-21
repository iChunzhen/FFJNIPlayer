#include <jni.h>
#include <string>
#include "NEFFmpeg.h"
#include "JavaCallHelper.h"
#include <android/native_window_jni.h>
#include <unistd.h>

extern "C" {
#include "include/libavutil/avutil.h"

//重采样
#include "libswresample/swresample.h"
}
JavaVM *javaVM = 0;
JavaCallHelper *javaCallHelper = 0;
NEFFmpeg *ffmpeg = 0;
ANativeWindow *window = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;//静态初始化mutex
extern "C" JNIEXPORT jstring JNICALL
Java_cn_ichunzhen_ffjniplayer_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    return env->NewStringUTF(av_version_info());
}

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    javaVM = vm;
    return JNI_VERSION_1_4;
}

//1，data;2，linesize；3，width; 4， height
void renderFrame(uint8_t *src_data, int src_lineSize, int width, int height) {
    pthread_mutex_lock(&mutex);
    if (!window) {
        pthread_mutex_unlock(&mutex);
        return;
    }
    ANativeWindow_setBuffersGeometry(window, width,
                                     height,
                                     WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer window_buffer;
    if (ANativeWindow_lock(window, &window_buffer, 0)) {
        ANativeWindow_release(window);
        window = 0;
        pthread_mutex_unlock(&mutex);
        return;
    }
    //把buffer中的数据进行赋值（修改）
    uint8_t *dst_data = static_cast<uint8_t *>(window_buffer.bits);
    int dst_lineSize = window_buffer.stride * 4;//ARGB
    //逐行拷贝
    for (int i = 0; i < window_buffer.height; ++i) {
        memcpy(dst_data + i * dst_lineSize, src_data + i * src_lineSize, dst_lineSize);
    }
    ANativeWindow_unlockAndPost(window);
    pthread_mutex_unlock(&mutex);
}


extern "C"
JNIEXPORT void JNICALL
Java_cn_ichunzhen_ffjniplayer_FFPlayer_prepareNative(JNIEnv *env, jobject thiz,
                                                     jstring dataSource_) {
    const char *dataSource = env->GetStringUTFChars(dataSource_, 0);
    javaCallHelper = new JavaCallHelper(javaVM, env, thiz);
    ffmpeg = new NEFFmpeg(javaCallHelper, const_cast<char *>(dataSource));
    ffmpeg->setRenderCallback(renderFrame);
    ffmpeg->prepare();

    env->ReleaseStringUTFChars(dataSource_, dataSource);
}




extern "C"
JNIEXPORT void JNICALL
Java_cn_ichunzhen_ffjniplayer_FFPlayer_startNative(JNIEnv *env, jobject thiz) {
    if (ffmpeg) {
        ffmpeg->start();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_cn_ichunzhen_ffjniplayer_FFPlayer_setSurfaceNative(JNIEnv *env, jobject thiz,
                                                        jobject surface) {
    pthread_mutex_lock(&mutex);
    //先释放之前的显示窗口
    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }
    //创建新的窗口用于视频显示
    window = ANativeWindow_fromSurface(env, surface);
    pthread_mutex_unlock(&mutex);
}
//视频解码
extern "C"
JNIEXPORT void JNICALL
Java_cn_ichunzhen_ffjniplayer_MainActivity_startPlayJNI(JNIEnv *env, jobject thiz,
                                                        jstring absolute_path, jobject surface) {
    const char *path = env->GetStringUTFChars(absolute_path, 0);
    //初始化
    avformat_network_init();
    AVFormatContext *formatContext = avformat_alloc_context();
    //打开url
    AVDictionary *opts = NULL;
    //设置三秒超时
    av_dict_set_int(&opts, "timeout", 3000000, 0);
    //强制指定AVFormatContext中的AVInputFormat 这个参数一般情况下可以设置为NULL，这样FFmpeg可以自动检测AVInputFormat。
    //输入文件的封装格式
//    av_find_input_format("avi");
    //ret为0表示成功
    //传入文件 内部共享 只是读文件头，并不会填充流信息
    int ret = avformat_open_input(&formatContext, path, NULL, &opts);
    //ff读取文件信息  此函数会读取packet，并确定文件中所有的流信息
    avformat_find_stream_info(formatContext, NULL);
    //视频时长 单位为微秒    遍历它找到第一条视频流 有视频流 音频流 字幕流等
    int video_stream_index = -1;
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    AVCodecParameters *codecParameters = formatContext->streams[video_stream_index]->codecpar;
    //找到解密器
    AVCodec *avCodec = avcodec_find_decoder(codecParameters->codec_id);
    //创建解码器上下文 并传入
    AVCodecContext *codecContext = avcodec_alloc_context3(avCodec);
    avcodec_parameters_to_context(codecContext, codecParameters);
    //版本升级了 打开文件 传入解码器
    avcodec_open2(codecContext, avCodec, NULL);
    //读取数据包
    AVPacket *packet = av_packet_alloc();
    //像素数据 传入解码器中获取的原始文件的大小 和输出文件的大小 和其他参数
    SwsContext *swsContext = sws_getContext(codecContext->width, codecContext->height,
                                            codecContext->pix_fmt,
                                            codecContext->width, codecContext->height,
                                            AV_PIX_FMT_RGBA,
                                            SWS_BILINEAR, 0, 0, 0);
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    //视频缓冲区 缓冲区内存拷贝完成后渲染nativeWindow
    ANativeWindow_Buffer outBuffer;
    //创建新窗口用于显示
    int frameCount = 0;
    ANativeWindow_setBuffersGeometry(nativeWindow, codecContext->width, codecContext->height,
                                     WINDOW_FORMAT_RGBA_8888);
    while (av_read_frame(formatContext, packet) >= 0) {
        avcodec_send_packet(codecContext, packet);
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            //需要更多数据
            continue;
        } else if (ret < 0) {
            break;
        }
        //读取到数据 继续进行
        uint8_t *dst_data[0];
        int dst_linesize[0];
        av_image_alloc(dst_data, dst_linesize,
                       codecContext->width, codecContext->height, AV_PIX_FMT_RGBA, 1);
        if (packet->stream_index == video_stream_index) {
            //非零正在解码
            if (ret == 0) {
                //绘制之前 配置一些信息 比如宽高 格式
                //绘制
                ANativeWindow_lock(nativeWindow, &outBuffer, NULL);
                //     h 264   ----yuv          RGBA
                //转为制定yuv420p
                sws_scale(swsContext, reinterpret_cast<const uint8_t *const *> (frame->data),
                          frame->linesize, 0,
                          frame->height, dst_data, dst_linesize);
                //rgb_frame是有画面数据
                uint8_t *dst = (uint8_t *) outBuffer.bits;
                //拿到一行又多少个字节
                int destStride = outBuffer.stride * 4;
                uint8_t *srcData = dst_data[0];
                int srcLinesize = dst_linesize[0];
                uint8_t *firstWindow = static_cast<uint8_t *>(outBuffer.bits);
                for (int i = 0; i < outBuffer.height; ++i) {
                    memcpy(firstWindow + i * destStride, srcData + i * srcLinesize, destStride);
                }
                ANativeWindow_unlockAndPost(nativeWindow);
                usleep(1000 * 16);
                av_frame_free(&frame);
            }
        }
    }
    ANativeWindow_release(nativeWindow);
    avcodec_close(codecContext);
    avformat_free_context(formatContext);
    env->ReleaseStringUTFChars(absolute_path, path);
}extern "C"
JNIEXPORT void JNICALL
Java_cn_ichunzhen_ffjniplayer_MainActivity_playSound(JNIEnv *env, jobject thiz, jstring input_,
                                                     jstring output_) {
    const char *input = env->GetStringUTFChars(input_, 0);
    const char *output = env->GetStringUTFChars(output_, 0);
    avformat_network_init();
//    总上下文
    AVFormatContext * formatContext = avformat_alloc_context();
    //打开音频文件
    if(avformat_open_input(&formatContext,input,NULL,NULL) != 0){
        LOGE("%s","无法打开音频文件");
        return;
    }

    //获取输入文件信息
    if(avformat_find_stream_info(formatContext,NULL) < 0){
        LOGE("%s","无法获取输入文件信息");
        return;
    }
    //视频时长（单位：微秒us，转换为秒需要除以1000000）
    int audio_stream_idx=-1;
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_idx=i;
            break;
        }
    }

    AVCodecParameters *codecpar = formatContext->streams[audio_stream_idx]->codecpar;
    //找到解码器
    AVCodec *dec = avcodec_find_decoder(codecpar->codec_id);
    //创建上下文
    AVCodecContext *codecContext = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(codecContext, codecpar);
    avcodec_open2(codecContext, dec, NULL);
    SwrContext *swrContext = swr_alloc();

//    输入的这些参数
    AVSampleFormat in_sample =  codecContext->sample_fmt;
    // 输入采样率
    int in_sample_rate = codecContext->sample_rate;
    //    输入声道布局
    uint64_t in_ch_layout=codecContext->channel_layout;
//        输出参数  固定

//    输出采样格式
    AVSampleFormat out_sample=AV_SAMPLE_FMT_S16;
//    输出采样
    int out_sample_rate=44100;
//    输出声道布局
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;
//设置转换器 的输入参数 和输出参数
    swr_alloc_set_opts(swrContext,out_ch_layout,out_sample,out_sample_rate
            ,in_ch_layout,in_sample,in_sample_rate,0,NULL);
//    初始化转换器其他的默认参数
    swr_init(swrContext);
    uint8_t *out_buffer = (uint8_t *)(av_malloc(2 * 44100));
    FILE *fp_pcm = fopen(output, "wb");
    //读取包  压缩数据
    AVPacket *packet = av_packet_alloc();
    int count = 0;
    //    设置音频缓冲区间 16bit   44100  PCM数据
//            输出 值
    while (av_read_frame(formatContext, packet)>=0) {
        avcodec_send_packet(codecContext, packet);
        //解压缩数据  未压缩
        AVFrame *frame = av_frame_alloc();
//        c    指针
        int ret = avcodec_receive_frame(codecContext, frame);
//        frame
        if (ret == AVERROR(EAGAIN))
            continue;
        else if (ret < 0) {
            LOGE("解码完成");
            break;
        }
        if (packet->stream_index!= audio_stream_idx) {
            continue;
        }
        LOGE("正在解码%d",count++);
//frame  ---->统一的格式
        swr_convert(swrContext, &out_buffer, 2 * 44100,
                    (const uint8_t **)frame->data, frame->nb_samples);
        int out_channerl_nb= av_get_channel_layout_nb_channels(out_ch_layout);
//缓冲区的 大小
        int out_buffer_size=  av_samples_get_buffer_size(NULL, out_channerl_nb, frame->nb_samples, out_sample, 1);
        fwrite(out_buffer,out_buffer_size,1,fp_pcm);
    }
    fclose(fp_pcm);
    av_free(out_buffer);
    swr_free(&swrContext);
    avcodec_close(codecContext);
    avformat_close_input(&formatContext);
    env->ReleaseStringUTFChars(input_, input);
    env->ReleaseStringUTFChars(output_, output);
}