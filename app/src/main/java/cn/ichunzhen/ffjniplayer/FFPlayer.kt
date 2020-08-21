package cn.ichunzhen.ffjniplayer

import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView

/**
 * @Author yuancz
 * @Date 2020/8/19-16:53
 * @Email ichunzhen6@gmail.com
 */
class FFPlayer : SurfaceHolder.Callback {
    companion object {
        init {
            System.loadLibrary("ffplayer")
        }

    }

    //准备过程错误码
    val ERROR_CODE_FFMPEG_PREPARE = 1000

    //播放过程错误码
    val ERROR_CODE_FFMPEG_PLAY = 2000

    //打不开视频
    val FFMPEG_CAN_NOT_OPEN_URL = ERROR_CODE_FFMPEG_PREPARE - 1

    //找不到媒体流信息
    val FFMPEG_CAN_NOT_FIND_STREAMS = ERROR_CODE_FFMPEG_PREPARE - 2

    //找不到解码器
    val FFMPEG_FIND_DECODER_FAIL = ERROR_CODE_FFMPEG_PREPARE - 3

    //无法根据解码器创建上下文
    val FFMPEG_ALLOC_CODEC_CONTEXT_FAIL = ERROR_CODE_FFMPEG_PREPARE - 4

    //根据流信息 配置上下文参数失败
    val FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL = ERROR_CODE_FFMPEG_PREPARE - 5

    //打开解码器失败
    val FFMPEG_OPEN_DECODER_FAIL = ERROR_CODE_FFMPEG_PREPARE - 6

    //没有音视频
    val FFMPEG_NOMEDIA = ERROR_CODE_FFMPEG_PREPARE - 7

    //读取媒体数据包失败
    val FFMPEG_READ_PACKETS_FAIL = ERROR_CODE_FFMPEG_PLAY - 1

    //直播地址或媒体文件路径
    private var dataSource: String? = null
    private var surfaceHolder: SurfaceHolder? = null

    fun setDataSource(dataSource: String?) {
        this.dataSource = dataSource
    }

    /**
     * 播放准备工作
     */
    fun prepare() {
        prepareNative(dataSource)
    }

    /**
     * 开始播放
     */
    fun start() {
        startNative()
    }

    /**
     * 供native反射调用
     * 表示播放器准备好了可以开始播放了
     */
    fun onPrepared() {
        if (onpreparedListener != null) {
            onpreparedListener!!.onPrepared()
        }
    }

    /**
     * 供native反射调用
     * 表示出错了
     */
    fun onError(errorCode: Int) {
        //        stop();
        if (null != onErrorListener) {
            onErrorListener!!.onError(errorCode)
        }
    }

    fun setOnpreparedListener(onpreparedListener: OnpreparedListener?) {
        this.onpreparedListener = onpreparedListener
    }

    fun setOnErrorListener(onErrorListener: OnErrorListener?) {
        this.onErrorListener = onErrorListener
    }

    fun setSurfaceView(surfaceView: SurfaceView) {
        if (null != surfaceHolder) {
            surfaceHolder?.removeCallback(this)
        }
        surfaceHolder = surfaceView.holder
        surfaceHolder?.addCallback(this)
    }

    /**
     * 画布创建回调
     *
     * @param holder
     */
    override fun surfaceCreated(holder: SurfaceHolder?) {}

    /**
     * 画布刷新
     *
     * @param holder
     * @param format
     * @param width
     * @param height
     */
    override fun surfaceChanged(
        holder: SurfaceHolder,
        format: Int,
        width: Int,
        height: Int
    ) {
        setSurfaceNative(holder.surface)
    }

    /**
     * 画布销毁
     *
     * @param holder
     */
    override fun surfaceDestroyed(holder: SurfaceHolder?) {}

    interface OnpreparedListener {
        fun onPrepared()
    }

    interface OnErrorListener {
        fun onError(errorCode: Int)
    }

    private var onErrorListener: OnErrorListener? = null
    private var onpreparedListener: OnpreparedListener? = null

    private external fun prepareNative(dataSource: String?)

    private external fun startNative()

    private external fun setSurfaceNative(surface: Surface)

}