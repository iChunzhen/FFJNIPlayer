package cn.ichunzhen.ffjniplayer

import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.Surface
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import kotlinx.android.synthetic.main.activity_main.*
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var player: FFPlayer

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        player = FFPlayer()
        player.setSurfaceView(surfaceView);
        player.setDataSource(
            File(
                Environment.getExternalStorageDirectory(),
                "input.mp4"
            ).getAbsolutePath()
        )
        player.setOnpreparedListener(object : FFPlayer.OnpreparedListener {
            override fun onPrepared() {
                runOnUiThread {
                    Log.e("MainActivityTAG", "开始播放")
                }
                //播放 调用到native去
                player.start()
            }
        })
        player.setOnErrorListener(object : FFPlayer.OnErrorListener {
            override fun onError(errorCode: Int) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "出错了，错误码：$errorCode", Toast.LENGTH_SHORT)
                        .show()
                }
            }
        })
    }

    fun openOne(view: View) {
        startPlayJNI(
            File(Environment.getExternalStorageDirectory(), "input.mp4").getAbsolutePath(),
            surfaceView.holder.surface
        )
    }

    fun openTwo(view: View) {
        val input =
            File(Environment.getExternalStorageDirectory(), "input.mp4")
                .absolutePath
        val output =
            File(Environment.getExternalStorageDirectory(), "output.pcm")
                .absolutePath
        playSound(input,output)

    }

    fun openThree(view: View) {
        player.prepare()

    }

    external fun startPlayJNI(absolutePath: String, surface: Surface)

    external fun playSound(input: String, output: String)
}
