package org.hftext.android

import android.Manifest
import android.annotation.SuppressLint
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Handler
import android.os.Looper
import androidx.annotation.RequiresPermission
import java.io.File
import java.io.BufferedOutputStream
import java.io.FileOutputStream
import java.io.OutputStream
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.math.abs
import kotlin.math.min

enum class HFTextAudioInputMode(val label: String) {
    VoiceRecognition("Voice"),
    Unprocessed("Raw"),
    Microphone("Mic")
}

class HFTextAudioRecorder(
    private val analyzeAudioSamples: (FloatArray, Int) -> HFTextAudioStats,
    private val createReceiver: (HFTextSpeedProfile) -> HFTextReceiverSession
) {
    private val evidenceLock = Any()
    private val mainHandler = Handler(Looper.getMainLooper())

    @Volatile
    private var activeRecorder: AudioRecord? = null

    @Volatile
    private var cancelled = false

    private var rawEvidence = FloatRingBuffer(1)
    private var modemEvidence = FloatRingBuffer(1)
    private var evidenceSampleRate = 0

    @SuppressLint("MissingPermission")
    @RequiresPermission(Manifest.permission.RECORD_AUDIO)
    fun start(
        profile: HFTextSpeedProfile,
        inputMode: HFTextAudioInputMode,
        sampleRate: Int,
        onStarted: (String) -> Unit,
        onStats: (HFTextAudioStats, HFTextAudioStats, Float, Double) -> Unit,
        onReceiverUpdate: (HFTextReceiverUpdate) -> Unit,
        onError: (String) -> Unit
    ) {
        stop()

        if (sampleRate <= 0) {
            onError("invalid RX sample rate")
            return
        }

        cancelled = false
        resetEvidence(sampleRate)
        val receiverQueue = ArrayBlockingQueue<FloatArray>(1024)
        val errorReported = AtomicBoolean(false)

        fun reportError(error: Throwable) {
            if (cancelled || !errorReported.compareAndSet(false, true)) {
                return
            }
            cancelled = true
            mainHandler.post {
                onError(error.message ?: error::class.java.simpleName)
            }
        }

        Thread {
            var receiver: HFTextReceiverSession? = null
            var lastReceiverStatusMs = 0L
            try {
                receiver = createReceiver(profile)
                while (!cancelled) {
                    val receiverSamples = receiverQueue.poll(100L, TimeUnit.MILLISECONDS) ?: continue
                    val nowMs = System.currentTimeMillis()
                    val receiverUpdate = receiver.pushSamples(receiverSamples)
                    val shouldPostReceiverUpdate = receiverUpdate.hasActivity ||
                        (receiverUpdate.eventCount > 0L && nowMs - lastReceiverStatusMs >= 1000L)
                    if (!cancelled && shouldPostReceiverUpdate) {
                        lastReceiverStatusMs = nowMs
                        mainHandler.post {
                            if (!cancelled) {
                                onReceiverUpdate(receiverUpdate)
                            }
                        }
                    }
                }
            } catch (error: Throwable) {
                reportError(error)
            } finally {
                try {
                    receiver?.close()
                } catch (_: Throwable) {
                }
            }
        }.apply {
            name = "HFTextAudioDecode"
            isDaemon = true
            start()
        }

        Thread {
            var recorder: AudioRecord? = null
            try {
                val minBufferSize = AudioRecord.getMinBufferSize(
                    sampleRate,
                    AudioFormat.CHANNEL_IN_MONO,
                    AudioFormat.ENCODING_PCM_FLOAT
                )
                if (minBufferSize <= 0) {
                    throw IllegalStateException("AudioRecord does not support $sampleRate Hz float mono capture")
                }

                val bufferSizeBytes = maxOf(minBufferSize, 4096 * Float.SIZE_BYTES)
                val bufferFloats = maxOf(bufferSizeBytes / Float.SIZE_BYTES, 1024)
                val recorderConfig = createAudioRecord(inputMode, sampleRate, bufferSizeBytes)
                recorder = recorderConfig.recorder

                activeRecorder = recorder
                recorder.startRecording()
                mainHandler.post { onStarted(recorderConfig.sourceLabel) }

                val buffer = FloatArray(bufferFloats)
                var lastStatsMs = 0L
                while (!cancelled) {
                    val read = recorder.read(
                        buffer,
                        0,
                        buffer.size,
                        AudioRecord.READ_BLOCKING
                    )
                    if (read < 0) {
                        throw IllegalStateException("AudioRecord read failed: $read")
                    }
                    if (read == 0) {
                        Thread.sleep(5)
                        continue
                    }

                    val nowMs = System.currentTimeMillis()
                    val samples = buffer.copyOf(read)
                    val receiverSamples = amplifiedReceiverSamples(samples)
                    appendEvidence(samples, receiverSamples.samples)
                    if (!receiverQueue.offer(receiverSamples.samples)) {
                        receiverQueue.poll()
                        receiverQueue.offer(receiverSamples.samples)
                    }

                    if (nowMs - lastStatsMs >= 250L) {
                        lastStatsMs = nowMs
                        val rawStats = analyzeAudioSamples(samples, sampleRate)
                        val receiverStats = analyzeAudioSamples(receiverSamples.samples, sampleRate)
                        if (!cancelled) {
                            mainHandler.post {
                                if (!cancelled) {
                                    onStats(
                                        rawStats,
                                        receiverStats,
                                        receiverSamples.gain,
                                        evidenceDurationSeconds()
                                    )
                                }
                            }
                        }
                    }
                }
            } catch (error: Throwable) {
                reportError(error)
            } finally {
                if (activeRecorder === recorder) {
                    activeRecorder = null
                }
                releaseRecorder(recorder)
            }
        }.apply {
            name = "HFTextAudioRecord"
            isDaemon = true
            start()
        }
    }

    fun saveDebugAudio(directory: File): HFTextSavedRxAudio {
        val snapshot = synchronized(evidenceLock) {
            EvidenceSnapshot(
                sampleRate = evidenceSampleRate,
                raw = rawEvidence.snapshot(),
                modem = modemEvidence.snapshot()
            )
        }

        if (snapshot.sampleRate <= 0 || snapshot.raw.isEmpty() || snapshot.modem.isEmpty()) {
            throw IllegalStateException("no RX audio has been captured yet")
        }

        if (!directory.exists() && !directory.mkdirs()) {
            throw IllegalStateException("could not create ${directory.absolutePath}")
        }

        val timestamp = System.currentTimeMillis()
        val rawFile = File(directory, "hftext-android-rx-$timestamp-raw.wav")
        val modemFile = File(directory, "hftext-android-rx-$timestamp-modem.wav")
        writePcm16Wav(rawFile, snapshot.raw, snapshot.sampleRate)
        writePcm16Wav(modemFile, snapshot.modem, snapshot.sampleRate)

        return HFTextSavedRxAudio(
            rawPath = rawFile.absolutePath,
            modemPath = modemFile.absolutePath,
            sampleRate = snapshot.sampleRate,
            sampleCount = min(snapshot.raw.size, snapshot.modem.size)
        )
    }

    fun evidenceDurationSeconds(): Double {
        val snapshot = synchronized(evidenceLock) {
            Pair(evidenceSampleRate, min(rawEvidence.size(), modemEvidence.size()))
        }
        return if (snapshot.first > 0) {
            snapshot.second.toDouble() / snapshot.first.toDouble()
        } else {
            0.0
        }
    }

    private data class EvidenceSnapshot(
        val sampleRate: Int,
        val raw: FloatArray,
        val modem: FloatArray
    )

    fun stop() {
        cancelled = true
        val recorder = activeRecorder
        activeRecorder = null
        releaseRecorder(recorder)
    }

    private fun releaseRecorder(recorder: AudioRecord?) {
        if (recorder == null) {
            return
        }

        try {
            recorder.stop()
        } catch (_: Throwable) {
        }
        try {
            recorder.release()
        } catch (_: Throwable) {
        }
    }

    private data class AudioRecordConfig(
        val recorder: AudioRecord,
        val sourceLabel: String
    )

    private data class ReceiverSamples(
        val samples: FloatArray,
        val gain: Float
    )

    private data class AudioSourceCandidate(
        val source: Int,
        val label: String
    )

    private fun audioSourceCandidates(inputMode: HFTextAudioInputMode): List<AudioSourceCandidate> {
        val selected = when (inputMode) {
            HFTextAudioInputMode.VoiceRecognition ->
                AudioSourceCandidate(MediaRecorder.AudioSource.VOICE_RECOGNITION, "voice recognition")
            HFTextAudioInputMode.Unprocessed ->
                AudioSourceCandidate(MediaRecorder.AudioSource.UNPROCESSED, "unprocessed")
            HFTextAudioInputMode.Microphone ->
                AudioSourceCandidate(MediaRecorder.AudioSource.MIC, "microphone")
        }
        val fallbacks = listOf(
            AudioSourceCandidate(MediaRecorder.AudioSource.VOICE_RECOGNITION, "voice recognition"),
            AudioSourceCandidate(MediaRecorder.AudioSource.UNPROCESSED, "unprocessed"),
            AudioSourceCandidate(MediaRecorder.AudioSource.MIC, "microphone")
        )
        return (listOf(selected) + fallbacks)
            .distinctBy { it.source }
    }

    @SuppressLint("MissingPermission")
    private fun createAudioRecord(
        inputMode: HFTextAudioInputMode,
        sampleRate: Int,
        bufferSizeBytes: Int
    ): AudioRecordConfig {
        val errors = mutableListOf<String>()
        for (candidate in audioSourceCandidates(inputMode)) {
            val recorder = try {
                AudioRecord.Builder()
                    .setAudioSource(candidate.source)
                    .setAudioFormat(
                        AudioFormat.Builder()
                            .setSampleRate(sampleRate)
                            .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                            .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                            .build()
                    )
                    .setBufferSizeInBytes(bufferSizeBytes)
                    .build()
            } catch (error: Throwable) {
                errors += "${candidate.label}: ${error.message ?: error::class.java.simpleName}"
                null
            }

            if (recorder == null) {
                continue
            }

            if (recorder.state == AudioRecord.STATE_INITIALIZED) {
                return AudioRecordConfig(recorder, candidate.label)
            }

            releaseRecorder(recorder)
            errors += "${candidate.label}: not initialized"
        }

        throw IllegalStateException(
            "AudioRecord initialization failed: ${errors.joinToString("; ")}"
        )
    }

    private fun amplifiedReceiverSamples(samples: FloatArray): ReceiverSamples {
        var peak = 0.0f
        for (sample in samples) {
            peak = maxOf(peak, abs(sample))
        }

        val targetPeak = 0.25f
        val minimumActivePeak = 0.001f
        val maximumGain = 80.0f
        val gain = if (peak >= minimumActivePeak && peak < targetPeak) {
            min(targetPeak / peak, maximumGain)
        } else {
            1.0f
        }

        if (gain <= 1.01f) {
            return ReceiverSamples(samples, 1.0f)
        }

        val amplified = FloatArray(samples.size)
        for (index in samples.indices) {
            amplified[index] = (samples[index] * gain).coerceIn(-0.95f, 0.95f)
        }
        return ReceiverSamples(amplified, gain)
    }

    private fun resetEvidence(sampleRate: Int) {
        val capacity = sampleRate * 120
        synchronized(evidenceLock) {
            rawEvidence = FloatRingBuffer(capacity)
            modemEvidence = FloatRingBuffer(capacity)
            evidenceSampleRate = sampleRate
        }
    }

    private fun appendEvidence(rawSamples: FloatArray, modemSamples: FloatArray) {
        synchronized(evidenceLock) {
            rawEvidence.append(rawSamples)
            modemEvidence.append(modemSamples)
        }
    }
}

data class HFTextSavedRxAudio(
    val rawPath: String,
    val modemPath: String,
    val sampleRate: Int,
    val sampleCount: Int
) {
    val durationSeconds: Double
        get() = if (sampleRate > 0) sampleCount.toDouble() / sampleRate.toDouble() else 0.0
}

private class FloatRingBuffer(
    private val capacity: Int
) {
    private val values = FloatArray(capacity.coerceAtLeast(1))
    private var nextIndex = 0
    private var size = 0

    fun append(samples: FloatArray) {
        if (samples.isEmpty()) {
            return
        }

        for (sample in samples) {
            values[nextIndex] = sample
            nextIndex = (nextIndex + 1) % values.size
            if (size < values.size) {
                ++size
            }
        }
    }

    fun snapshot(): FloatArray {
        val out = FloatArray(size)
        if (size == 0) {
            return out
        }

        val start = if (size == values.size) nextIndex else 0
        for (index in 0 until size) {
            out[index] = values[(start + index) % values.size]
        }
        return out
    }

    fun size(): Int {
        return size
    }
}

private fun writePcm16Wav(file: File, samples: FloatArray, sampleRate: Int) {
    val dataBytes = samples.size * 2
    BufferedOutputStream(FileOutputStream(file)).use { out ->
        out.writeAscii("RIFF")
        out.writeInt32Le(36 + dataBytes)
        out.writeAscii("WAVE")
        out.writeAscii("fmt ")
        out.writeInt32Le(16)
        out.writeInt16Le(1)
        out.writeInt16Le(1)
        out.writeInt32Le(sampleRate)
        out.writeInt32Le(sampleRate * 2)
        out.writeInt16Le(2)
        out.writeInt16Le(16)
        out.writeAscii("data")
        out.writeInt32Le(dataBytes)

        val buffer = ByteArray(8192 * 2)
        var sampleIndex = 0
        while (sampleIndex < samples.size) {
            val sampleCount = min(8192, samples.size - sampleIndex)
            var byteIndex = 0
            for (offset in 0 until sampleCount) {
                val value = (samples[sampleIndex + offset].coerceIn(-1.0f, 1.0f) * 32767.0f).toInt()
                buffer[byteIndex++] = (value and 0xFF).toByte()
                buffer[byteIndex++] = ((value shr 8) and 0xFF).toByte()
            }
            out.write(buffer, 0, byteIndex)
            sampleIndex += sampleCount
        }
    }
}

private fun OutputStream.writeAscii(value: String) {
    write(value.toByteArray(Charsets.US_ASCII))
}

private fun OutputStream.writeInt16Le(value: Int) {
    write(value and 0xFF)
    write((value shr 8) and 0xFF)
}

private fun OutputStream.writeInt32Le(value: Int) {
    write(value and 0xFF)
    write((value shr 8) and 0xFF)
    write((value shr 16) and 0xFF)
    write((value shr 24) and 0xFF)
}
