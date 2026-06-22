package org.hftext.android

data class HFTextNativeSnapshot(
    val nativeAvailable: Boolean,
    val bridgeStatus: String,
    val core: String,
    val protocol: String,
    val slowProfile: String,
    val fastProfile: String
)

data class HFTextTextAnalysis(
    val nativeAvailable: Boolean,
    val status: String,
    val error: String,
    val sanitizedMessage: String,
    val payload: String,
    val messageSymbols: Int,
    val payloadSymbols: Int,
    val maxPayloadSymbols: Int,
    val payloadTooLong: Boolean,
    val messageEmpty: Boolean,
    val slowDurationSeconds: Double,
    val slowTransmissionBits: Int,
    val slowPayloadTooLong: Boolean,
    val fastDurationSeconds: Double,
    val fastTransmissionBits: Int,
    val fastPayloadTooLong: Boolean
)

enum class HFTextSpeedProfile(val nativeValue: Int, val label: String) {
    Slow(0, "Slow"),
    Fast(1, "Fast")
}

data class HFTextGeneratedAudio(
    val ok: Boolean,
    val error: String,
    val sampleRate: Int,
    val samples: FloatArray
) {
    val durationSeconds: Double
        get() = if (sampleRate > 0) samples.size.toDouble() / sampleRate.toDouble() else 0.0
}

data class HFTextAudioStats(
    val ok: Boolean,
    val error: String,
    val sampleCount: Long,
    val peak: Float,
    val clippedSamples: Long,
    val clippingPercent: Double,
    val durationSeconds: Double
)

data class HFTextReceiverUpdate(
    val ok: Boolean,
    val error: String,
    val messages: List<String>,
    val state: String,
    val progress: Double,
    val quality: Double,
    val accepted: Long,
    val rejected: Long,
    val sync: Long,
    val eventCount: Long
) {
    val hasActivity: Boolean
        get() = !ok || messages.isNotEmpty() || state.isNotBlank()
}

interface HFTextReceiverSession : AutoCloseable {
    fun pushSamples(samples: FloatArray): HFTextReceiverUpdate
}

object HFTextNativeBridge {
    @Volatile
    private var loadAttempted = false

    @Volatile
    private var loadFailure: Throwable? = null

    fun snapshot(): HFTextNativeSnapshot {
        val failure = ensureLoaded()
        if (failure != null) {
            return HFTextNativeSnapshot(
                nativeAvailable = false,
                bridgeStatus = "JNI unavailable: ${failure.message ?: failure::class.java.simpleName}",
                core = "--",
                protocol = "--",
                slowProfile = "--",
                fastProfile = "--"
            )
        }

        return try {
            val applicationName = nativeApplicationName()
            val version = nativeVersionLabel()
            val track = nativeReleaseTrack()
            val coreLabel = if (version.startsWith(applicationName)) {
                "$version ($track)"
            } else {
                "$applicationName $version ($track)"
            }
            HFTextNativeSnapshot(
                nativeAvailable = true,
                bridgeStatus = "JNI OK via C ABI",
                core = coreLabel,
                protocol = nativeProtocolVersion(),
                slowProfile = nativeSlowProfileSummary(),
                fastProfile = nativeFastProfileSummary()
            )
        } catch (error: Throwable) {
            HFTextNativeSnapshot(
                nativeAvailable = false,
                bridgeStatus = "JNI call failed: ${error.message ?: error::class.java.simpleName}",
                core = "--",
                protocol = "--",
                slowProfile = "--",
                fastProfile = "--"
            )
        }
    }

    fun analyzeText(callsign: String, message: String): HFTextTextAnalysis {
        val failure = ensureLoaded()
        if (failure != null) {
            return unavailableAnalysis("JNI unavailable: ${failure.message ?: failure::class.java.simpleName}")
        }

        return try {
            val fields = nativeTextAnalysis(callsign, message)
            HFTextTextAnalysis(
                nativeAvailable = true,
                status = fields.field(0, "error"),
                error = fields.field(1, ""),
                sanitizedMessage = fields.field(2, ""),
                payload = fields.field(3, ""),
                messageSymbols = fields.field(4, "0").toIntOrNull() ?: 0,
                payloadSymbols = fields.field(5, "0").toIntOrNull() ?: 0,
                maxPayloadSymbols = fields.field(6, "127").toIntOrNull() ?: 127,
                payloadTooLong = fields.field(7, "0") == "1",
                messageEmpty = fields.field(8, "1") == "1",
                slowDurationSeconds = fields.field(9, "0.0").toDoubleOrNull() ?: 0.0,
                slowTransmissionBits = fields.field(10, "0").toIntOrNull() ?: 0,
                slowPayloadTooLong = fields.field(11, "1") == "1",
                fastDurationSeconds = fields.field(12, "0.0").toDoubleOrNull() ?: 0.0,
                fastTransmissionBits = fields.field(13, "0").toIntOrNull() ?: 0,
                fastPayloadTooLong = fields.field(14, "1") == "1"
            )
        } catch (error: Throwable) {
            unavailableAnalysis("JNI call failed: ${error.message ?: error::class.java.simpleName}")
        }
    }

    fun generateTransmitAudio(
        callsign: String,
        message: String,
        profile: HFTextSpeedProfile
    ): HFTextGeneratedAudio {
        val failure = ensureLoaded()
        if (failure != null) {
            return HFTextGeneratedAudio(
                ok = false,
                error = "JNI unavailable: ${failure.message ?: failure::class.java.simpleName}",
                sampleRate = 0,
                samples = FloatArray(0)
            )
        }

        return try {
            val sampleRate = nativeTransmitSampleRate(profile.nativeValue)
            val samples = nativeTransmitAudioSamples(callsign, message, profile.nativeValue)
            HFTextGeneratedAudio(
                ok = true,
                error = "",
                sampleRate = sampleRate,
                samples = samples
            )
        } catch (error: Throwable) {
            HFTextGeneratedAudio(
                ok = false,
                error = error.message ?: error::class.java.simpleName,
                sampleRate = 0,
                samples = FloatArray(0)
            )
        }
    }

    fun receiveSampleRate(profile: HFTextSpeedProfile): Int {
        val failure = ensureLoaded()
        if (failure != null) {
            return 0
        }

        return try {
            nativeReceiveSampleRate(profile.nativeValue)
        } catch (_: Throwable) {
            0
        }
    }

    fun toneFrequencies(profile: HFTextSpeedProfile): FloatArray {
        val failure = ensureLoaded()
        if (failure != null) {
            return FloatArray(0)
        }

        return try {
            nativeToneFrequencies(profile.nativeValue)
        } catch (_: Throwable) {
            FloatArray(0)
        }
    }

    fun analyzeAudioSamples(samples: FloatArray, sampleRate: Int): HFTextAudioStats {
        val failure = ensureLoaded()
        if (failure != null) {
            return unavailableAudioStats(
                "JNI unavailable: ${failure.message ?: failure::class.java.simpleName}"
            )
        }

        return try {
            val fields = nativeAnalyzeAudioSamples(samples, sampleRate)
            HFTextAudioStats(
                ok = fields.field(0, "error") == "ok",
                error = fields.field(1, ""),
                sampleCount = fields.field(2, "0").toLongOrNull() ?: 0L,
                peak = fields.field(3, "0.0").toFloatOrNull() ?: 0.0f,
                clippedSamples = fields.field(4, "0").toLongOrNull() ?: 0L,
                clippingPercent = fields.field(5, "0.0").toDoubleOrNull() ?: 0.0,
                durationSeconds = fields.field(6, "0.0").toDoubleOrNull() ?: 0.0
            )
        } catch (error: Throwable) {
            unavailableAudioStats("JNI call failed: ${error.message ?: error::class.java.simpleName}")
        }
    }

    fun createReceiver(profile: HFTextSpeedProfile): HFTextReceiverSession {
        val failure = ensureLoaded()
        if (failure != null) {
            throw IllegalStateException(
                "JNI unavailable: ${failure.message ?: failure::class.java.simpleName}"
            )
        }

        val handle = nativeReceiverCreate(profile.nativeValue)
        if (handle == 0L) {
            throw IllegalStateException("native receiver was not created")
        }
        return NativeReceiverSession(handle)
    }

    private class NativeReceiverSession(
        private var handle: Long
    ) : HFTextReceiverSession {
        override fun pushSamples(samples: FloatArray): HFTextReceiverUpdate {
            val activeHandle = handle
            if (activeHandle == 0L) {
                return unavailableReceiverUpdate("receiver is not active")
            }

            return try {
                parseReceiverUpdate(nativeReceiverPushSamples(activeHandle, samples))
            } catch (error: Throwable) {
                unavailableReceiverUpdate(
                    "JNI call failed: ${error.message ?: error::class.java.simpleName}"
                )
            }
        }

        override fun close() {
            val activeHandle = handle
            if (activeHandle != 0L) {
                handle = 0L
                nativeReceiverFree(activeHandle)
            }
        }
    }

    private fun parseReceiverUpdate(fields: Array<String>): HFTextReceiverUpdate {
        val messageText = fields.field(2, "")
        val messages = messageText
            .lineSequence()
            .map { it.trim() }
            .filter { it.isNotEmpty() }
            .toList()
        return HFTextReceiverUpdate(
            ok = fields.field(0, "error") == "ok",
            error = fields.field(1, ""),
            messages = messages,
            state = fields.field(3, ""),
            progress = fields.field(4, "-1.0").toDoubleOrNull() ?: -1.0,
            quality = fields.field(5, "-1.0").toDoubleOrNull() ?: -1.0,
            accepted = fields.field(6, "0").toLongOrNull() ?: 0L,
            rejected = fields.field(7, "0").toLongOrNull() ?: 0L,
            sync = fields.field(8, "0").toLongOrNull() ?: 0L,
            eventCount = fields.field(9, "0").toLongOrNull() ?: 0L
        )
    }

    private fun unavailableReceiverUpdate(error: String): HFTextReceiverUpdate {
        return HFTextReceiverUpdate(
            ok = false,
            error = error,
            messages = emptyList(),
            state = "",
            progress = -1.0,
            quality = -1.0,
            accepted = 0L,
            rejected = 0L,
            sync = 0L,
            eventCount = 0L
        )
    }

    private fun unavailableAudioStats(error: String): HFTextAudioStats {
        return HFTextAudioStats(
            ok = false,
            error = error,
            sampleCount = 0L,
            peak = 0.0f,
            clippedSamples = 0L,
            clippingPercent = 0.0,
            durationSeconds = 0.0
        )
    }

    private fun unavailableAnalysis(error: String): HFTextTextAnalysis {
        return HFTextTextAnalysis(
            nativeAvailable = false,
            status = "error",
            error = error,
            sanitizedMessage = "",
            payload = "",
            messageSymbols = 0,
            payloadSymbols = 0,
            maxPayloadSymbols = 127,
            payloadTooLong = false,
            messageEmpty = true,
            slowDurationSeconds = 0.0,
            slowTransmissionBits = 0,
            slowPayloadTooLong = true,
            fastDurationSeconds = 0.0,
            fastTransmissionBits = 0,
            fastPayloadTooLong = true
        )
    }

    private fun Array<String>.field(index: Int, default: String): String {
        return getOrNull(index) ?: default
    }

    private fun ensureLoaded(): Throwable? {
        if (loadAttempted) {
            return loadFailure
        }

        synchronized(this) {
            if (!loadAttempted) {
                loadFailure = try {
                    System.loadLibrary("hftext_c_api")
                    System.loadLibrary("hftext_android_jni")
                    null
                } catch (error: Throwable) {
                    error
                }
                loadAttempted = true
            }
        }
        return loadFailure
    }

    @JvmStatic
    private external fun nativeApplicationName(): String

    @JvmStatic
    private external fun nativeVersionLabel(): String

    @JvmStatic
    private external fun nativeReleaseTrack(): String

    @JvmStatic
    private external fun nativeProtocolVersion(): String

    @JvmStatic
    private external fun nativeSlowProfileSummary(): String

    @JvmStatic
    private external fun nativeFastProfileSummary(): String

    @JvmStatic
    private external fun nativeTextAnalysis(callsign: String, message: String): Array<String>

    @JvmStatic
    private external fun nativeTransmitSampleRate(profile: Int): Int

    @JvmStatic
    private external fun nativeTransmitAudioSamples(
        callsign: String,
        message: String,
        profile: Int
    ): FloatArray

    @JvmStatic
    private external fun nativeReceiveSampleRate(profile: Int): Int

    @JvmStatic
    private external fun nativeToneFrequencies(profile: Int): FloatArray

    @JvmStatic
    private external fun nativeAnalyzeAudioSamples(samples: FloatArray, sampleRate: Int): Array<String>

    @JvmStatic
    private external fun nativeReceiverCreate(profile: Int): Long

    @JvmStatic
    private external fun nativeReceiverFree(handle: Long)

    @JvmStatic
    private external fun nativeReceiverPushSamples(handle: Long, samples: FloatArray): Array<String>
}
