package org.hftext.android

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.os.Handler
import android.os.Looper

class HFTextAudioPlayer {
    private val mainHandler = Handler(Looper.getMainLooper())

    @Volatile
    private var activeTrack: AudioTrack? = null

    @Volatile
    private var cancelled = false

    fun play(
        samples: FloatArray,
        sampleRate: Int,
        onProgress: (Double) -> Unit,
        onFinished: () -> Unit,
        onError: (String) -> Unit
    ) {
        stop()

        if (samples.isEmpty() || sampleRate <= 0) {
            onError("empty TX audio")
            return
        }

        cancelled = false
        Thread {
            var track: AudioTrack? = null
            try {
                val minBufferSize = AudioTrack.getMinBufferSize(
                    sampleRate,
                    AudioFormat.CHANNEL_OUT_MONO,
                    AudioFormat.ENCODING_PCM_FLOAT
                )
                val bufferSizeBytes = maxOf(
                    if (minBufferSize > 0) minBufferSize else 0,
                    4096 * Float.SIZE_BYTES
                )

                track = AudioTrack.Builder()
                    .setAudioAttributes(
                        AudioAttributes.Builder()
                            .setUsage(AudioAttributes.USAGE_MEDIA)
                            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                            .build()
                    )
                    .setAudioFormat(
                        AudioFormat.Builder()
                            .setSampleRate(sampleRate)
                            .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                            .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                            .build()
                    )
                    .setBufferSizeInBytes(bufferSizeBytes)
                    .setTransferMode(AudioTrack.MODE_STREAM)
                    .build()

                activeTrack = track
                track.play()

                var offset = 0
                var lastProgressMs = 0L
                while (offset < samples.size && !cancelled) {
                    val count = minOf(4096, samples.size - offset)
                    val written = track.write(
                        samples,
                        offset,
                        count,
                        AudioTrack.WRITE_BLOCKING
                    )
                    if (written < 0) {
                        throw IllegalStateException("AudioTrack write failed: $written")
                    }
                    if (written == 0) {
                        Thread.sleep(5)
                    } else {
                        offset += written
                        val nowMs = System.currentTimeMillis()
                        if (nowMs - lastProgressMs >= 200L || offset >= samples.size) {
                            lastProgressMs = nowMs
                            val progress = offset.toDouble() / samples.size.toDouble()
                            mainHandler.post {
                                if (!cancelled) {
                                    onProgress(progress.coerceIn(0.0, 1.0))
                                }
                            }
                        }
                    }
                }

                if (!cancelled) {
                    mainHandler.post {
                        onProgress(1.0)
                        onFinished()
                    }
                }
            } catch (error: Throwable) {
                if (!cancelled) {
                    mainHandler.post {
                        onError(error.message ?: error::class.java.simpleName)
                    }
                }
            } finally {
                if (activeTrack === track) {
                    activeTrack = null
                }
                releaseTrack(track)
            }
        }.apply {
            name = "HFTextAudioTrack"
            isDaemon = true
            start()
        }
    }

    fun stop() {
        cancelled = true
        val track = activeTrack
        activeTrack = null
        releaseTrack(track)
    }

    private fun releaseTrack(track: AudioTrack?) {
        if (track == null) {
            return
        }

        try {
            track.pause()
        } catch (_: Throwable) {
        }
        try {
            track.flush()
        } catch (_: Throwable) {
        }
        try {
            track.release()
        } catch (_: Throwable) {
        }
    }
}
