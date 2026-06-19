package org.hftext.android

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            HFTextApp()
        }
    }
}

@Composable
private fun HFTextApp() {
    val nativeSnapshot = remember { HFTextNativeBridge.snapshot() }
    HFTextScreen(
        nativeSnapshot = nativeSnapshot,
        analyzeText = HFTextNativeBridge::analyzeText,
        generateTransmitAudio = HFTextNativeBridge::generateTransmitAudio,
        receiveSampleRate = HFTextNativeBridge::receiveSampleRate,
        analyzeAudioSamples = HFTextNativeBridge::analyzeAudioSamples,
        createReceiver = HFTextNativeBridge::createReceiver
    )
}

@Composable
private fun HFTextScreen(
    nativeSnapshot: HFTextNativeSnapshot,
    analyzeText: (String, String) -> HFTextTextAnalysis,
    generateTransmitAudio: (String, String, HFTextSpeedProfile) -> HFTextGeneratedAudio,
    receiveSampleRate: (HFTextSpeedProfile) -> Int,
    analyzeAudioSamples: (FloatArray, Int) -> HFTextAudioStats,
    createReceiver: (HFTextSpeedProfile) -> HFTextReceiverSession
) {
    val context = LocalContext.current
    var callsign by remember { mutableStateOf("pu5lrk") }
    var message by remember { mutableStateOf("Hello HFText!") }
    var selectedProfile by remember { mutableStateOf(HFTextSpeedProfile.Fast) }
    var isTransmitting by remember { mutableStateOf(false) }
    var isReceiving by remember { mutableStateOf(false) }
    var rxInputMode by remember { mutableStateOf(HFTextAudioInputMode.VoiceRecognition) }
    var txStatus by remember { mutableStateOf("ready") }
    var rxStatus by remember { mutableStateOf("stopped") }
    var rxStats by remember { mutableStateOf(emptyAudioStats()) }
    var rxReceiverStats by remember { mutableStateOf(emptyAudioStats()) }
    var rxReceiverGain by remember { mutableStateOf(1.0f) }
    var rxBufferSeconds by remember { mutableStateOf(0.0) }
    var rxDecodeStatus by remember { mutableStateOf("decoder idle") }
    var rxEvidenceStatus by remember { mutableStateOf("not saved") }
    var rxAccepted by remember { mutableStateOf(0L) }
    var rxRejected by remember { mutableStateOf(0L) }
    var rxSync by remember { mutableStateOf(0L) }
    var rxEvents by remember { mutableStateOf(0L) }
    var receivedMessages by remember { mutableStateOf(emptyList<String>()) }
    var hasRecordPermission by remember {
        mutableStateOf(
            context.checkSelfPermission(Manifest.permission.RECORD_AUDIO) ==
                PackageManager.PERMISSION_GRANTED
        )
    }
    val audioPlayer = remember { HFTextAudioPlayer() }
    val audioRecorder = remember(analyzeAudioSamples, createReceiver) {
        HFTextAudioRecorder(analyzeAudioSamples, createReceiver)
    }
    val mainHandler = remember { Handler(Looper.getMainLooper()) }
    val analysis = remember(callsign, message) {
        analyzeText(callsign, message)
    }
    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { granted ->
        hasRecordPermission = granted
        rxStatus = if (granted) {
            "microphone permission granted; press Start RX"
        } else {
            "microphone permission denied"
        }
    }

    DisposableEffect(audioPlayer, audioRecorder) {
        onDispose {
            audioPlayer.stop()
            audioRecorder.stop()
        }
    }

    fun startOrStopTx() {
        if (isTransmitting) {
            audioPlayer.stop()
            isTransmitting = false
            txStatus = "TX cancelled"
            return
        }

        if (!nativeSnapshot.nativeAvailable || !analysis.nativeAvailable) {
            txStatus = "native bridge unavailable"
            return
        }
        if (analysis.messageEmpty) {
            txStatus = "type a message first"
            return
        }
        if (analysis.payloadTooLong || estimateTooLong(analysis, selectedProfile)) {
            txStatus = "payload too long"
            return
        }

        val txCallsign = callsign
        val txMessage = message
        val txProfile = selectedProfile
        isTransmitting = true
        txStatus = "preparing ${txProfile.label} TX audio"

        Thread {
            val generatedAudio = generateTransmitAudio(txCallsign, txMessage, txProfile)
            mainHandler.post {
                if (!isTransmitting) {
                    return@post
                }
                if (!generatedAudio.ok) {
                    isTransmitting = false
                    txStatus = "TX failed: ${generatedAudio.error}"
                    return@post
                }

                txStatus = "playing ${txProfile.label} TX (${formatSeconds(generatedAudio.durationSeconds)})"
                audioPlayer.play(
                    samples = generatedAudio.samples,
                    sampleRate = generatedAudio.sampleRate,
                    onFinished = {
                        isTransmitting = false
                        txStatus = "TX complete"
                    },
                    onError = { error ->
                        isTransmitting = false
                        txStatus = "TX failed: $error"
                    }
                )
            }
        }.apply {
            name = "HFTextTxGenerate"
            isDaemon = true
            start()
        }
    }

    fun startOrStopRx() {
        if (isReceiving) {
            audioRecorder.stop()
            isReceiving = false
            rxStatus = "stopped"
            rxStats = emptyAudioStats()
            rxReceiverStats = emptyAudioStats()
            rxReceiverGain = 1.0f
            rxBufferSeconds = 0.0
            rxDecodeStatus = "decoder idle"
            return
        }

        if (!hasRecordPermission) {
            rxStatus = "requesting microphone permission"
            permissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
            return
        }

        val rxProfile = selectedProfile
        val sampleRate = receiveSampleRate(rxProfile)
        if (sampleRate <= 0) {
            rxStatus = "invalid native RX sample rate"
            return
        }

        rxStatus = "starting ${rxProfile.label} RX capture"
        rxAccepted = 0L
        rxRejected = 0L
        rxSync = 0L
        rxEvents = 0L
        rxBufferSeconds = 0.0
        audioRecorder.start(
            profile = rxProfile,
            inputMode = rxInputMode,
            sampleRate = sampleRate,
            onStarted = { sourceLabel ->
                isReceiving = true
                rxStatus = "capturing ${rxProfile.label} audio at $sampleRate Hz ($sourceLabel)"
                rxDecodeStatus = "listening"
            },
            onStats = { rawStats, receiverStats, gain, evidenceSeconds ->
                rxStats = rawStats
                rxReceiverStats = receiverStats
                rxReceiverGain = gain
                rxBufferSeconds = evidenceSeconds
                if (!rawStats.ok) {
                    rxStatus = "RX stats failed: ${rawStats.error}"
                } else if (!receiverStats.ok) {
                    rxStatus = "RX receiver stats failed: ${receiverStats.error}"
                }
            },
            onReceiverUpdate = { update ->
                if (!update.ok) {
                    rxDecodeStatus = update.error.ifBlank { "decoder update failed" }
                } else {
                    if (update.eventCount > 0) {
                        rxEvents += update.eventCount
                    }
                    if (update.state.isNotBlank()) {
                        rxDecodeStatus = receiverStatusText(update)
                    } else if (update.eventCount > 0) {
                        rxDecodeStatus = "low-confidence receiver activity (${update.eventCount} events)"
                    }
                    if (update.accepted > 0) {
                        rxAccepted += update.accepted
                    }
                    if (update.rejected > 0) {
                        rxRejected += update.rejected
                    }
                    if (update.sync > 0) {
                        rxSync += update.sync
                    }
                    if (update.messages.isNotEmpty()) {
                        val timestamp = currentClockText()
                        receivedMessages = (receivedMessages + update.messages.map {
                            "[$timestamp] $it"
                        }).takeLast(20)
                        rxDecodeStatus = "message accepted"
                    }
                }
            },
            onError = { error ->
                isReceiving = false
                rxStatus = "RX failed: $error"
            }
        )
    }

    fun saveRxEvidence() {
        rxEvidenceStatus = "saving RX audio..."
        val expectedTxSeconds = estimateSeconds(analysis, selectedProfile)
        Thread {
            try {
                val directory = context.getExternalFilesDir("rx-evidence") ?: context.filesDir
                val savedAudio = audioRecorder.saveDebugAudio(directory)
                mainHandler.post {
                    val duration = savedAudio.durationSeconds
                    val durationText = if (expectedTxSeconds > 0.0 && duration + 0.5 < expectedTxSeconds) {
                        "saved ${formatSeconds(duration)}; shorter than local TX estimate ${formatSeconds(expectedTxSeconds)}"
                    } else {
                        "saved ${formatSeconds(duration)}"
                    }
                    rxEvidenceStatus = "$durationText | ${savedAudio.modemPath}"
                }
            } catch (error: Throwable) {
                mainHandler.post {
                    rxEvidenceStatus = "save failed: ${error.message ?: error::class.java.simpleName}"
                }
            }
        }.apply {
            name = "HFTextSaveRxAudio"
            isDaemon = true
            start()
        }
    }

    MaterialTheme {
        Surface(
            color = Color(0xFF14181D),
            modifier = Modifier.fillMaxSize()
        ) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .verticalScroll(rememberScrollState())
                    .padding(20.dp),
                verticalArrangement = Arrangement.spacedBy(18.dp)
            ) {
                Column {
                    Text(
                        text = "HFText",
                        color = Color.White,
                        style = MaterialTheme.typography.headlineMedium,
                        fontWeight = FontWeight.SemiBold
                    )
                    Text(
                        text = "Android JNI shell",
                        color = Color(0xFF9FB3C8),
                        style = MaterialTheme.typography.bodyMedium
                    )
                }

                Column(
                    modifier = Modifier.fillMaxWidth(),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    StatusRow(label = "Bridge", value = nativeSnapshot.bridgeStatus)
                    StatusRow(label = "Core", value = nativeSnapshot.core)
                    StatusRow(label = "Protocol", value = nativeSnapshot.protocol)
                    StatusRow(label = "Slow", value = nativeSnapshot.slowProfile)
                    StatusRow(label = "Fast", value = nativeSnapshot.fastProfile)
                }

                Column(
                    modifier = Modifier.fillMaxWidth(),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    HFTextField(
                        label = "Callsign",
                        value = callsign,
                        onValueChange = { callsign = it },
                        singleLine = true
                    )
                    HFTextField(
                        label = "Message",
                        value = message,
                        onValueChange = { message = it },
                        singleLine = false
                    )
                }

                Column(
                    modifier = Modifier.fillMaxWidth(),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(10.dp)
                    ) {
                        ProfileButton(
                            label = "Fast",
                            selected = selectedProfile == HFTextSpeedProfile.Fast,
                            onClick = { selectedProfile = HFTextSpeedProfile.Fast },
                            modifier = Modifier.weight(1f)
                        )
                        ProfileButton(
                            label = "Slow",
                            selected = selectedProfile == HFTextSpeedProfile.Slow,
                            onClick = { selectedProfile = HFTextSpeedProfile.Slow },
                            modifier = Modifier.weight(1f)
                        )
                    }
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(10.dp)
                    ) {
                        AudioInputButton(
                            label = "Voice",
                            selected = rxInputMode == HFTextAudioInputMode.VoiceRecognition,
                            enabled = !isReceiving,
                            onClick = { rxInputMode = HFTextAudioInputMode.VoiceRecognition },
                            modifier = Modifier.weight(1f)
                        )
                        AudioInputButton(
                            label = "Raw",
                            selected = rxInputMode == HFTextAudioInputMode.Unprocessed,
                            enabled = !isReceiving,
                            onClick = { rxInputMode = HFTextAudioInputMode.Unprocessed },
                            modifier = Modifier.weight(1f)
                        )
                        AudioInputButton(
                            label = "Mic",
                            selected = rxInputMode == HFTextAudioInputMode.Microphone,
                            enabled = !isReceiving,
                            onClick = { rxInputMode = HFTextAudioInputMode.Microphone },
                            modifier = Modifier.weight(1f)
                        )
                    }
                    Button(
                        onClick = ::startOrStopTx,
                        modifier = Modifier.fillMaxWidth(),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color(0xFF3C6F9F),
                            contentColor = Color.White
                        )
                    ) {
                        Text(if (isTransmitting) "Stop TX" else "Send audio")
                    }
                    OutlinedButton(
                        onClick = ::startOrStopRx,
                        modifier = Modifier.fillMaxWidth(),
                        colors = ButtonDefaults.outlinedButtonColors(
                            contentColor = Color(0xFFE6EDF3)
                        )
                    ) {
                        Text(if (isReceiving) "Stop RX capture" else "Start RX capture")
                    }
                    OutlinedButton(
                        onClick = ::saveRxEvidence,
                        modifier = Modifier.fillMaxWidth(),
                        colors = ButtonDefaults.outlinedButtonColors(
                            contentColor = Color(0xFFE6EDF3)
                        )
                    ) {
                        Text("Save RX audio")
                    }
                }

                Column(
                    modifier = Modifier.fillMaxWidth(),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    StatusRow(label = "Received", value = receivedMessagesText(receivedMessages))
                    StatusRow(label = "Text", value = textStatus(analysis))
                    StatusRow(label = "TX", value = txStatus)
                    StatusRow(label = "RX", value = rxStatus)
                    StatusRow(label = "RX level", value = rxLevelText(rxStats, rxReceiverStats, rxReceiverGain))
                    StatusRow(label = "RX buffer", value = rxBufferText(rxBufferSeconds, analysis, selectedProfile))
                    StatusRow(label = "Decoder", value = rxDecodeStatus)
                    StatusRow(label = "RX session", value = "accepted $rxAccepted | rejected $rxRejected | sync $rxSync | events $rxEvents")
                    StatusRow(label = "RX evidence", value = rxEvidenceStatus)
                    StatusRow(label = "Sanitized", value = displayText(analysis.sanitizedMessage))
                    StatusRow(label = "Payload", value = displayText(analysis.payload))
                    StatusRow(
                        label = "Symbols",
                        value = "${analysis.payloadSymbols}/${analysis.maxPayloadSymbols} payload | ${analysis.messageSymbols} message"
                    )
                    StatusRow(label = "Slow TX", value = estimateText(analysis, slow = true))
                    StatusRow(label = "Fast TX", value = estimateText(analysis, slow = false))
                }

                Column {
                    Text(
                        text = if (nativeSnapshot.nativeAvailable) {
                            "Kotlin is reading metadata, text preparation, and estimates through JNI and the C ABI."
                        } else {
                            "Native bridge is not available in this runtime."
                        },
                        color = Color(0xFFE6EDF3),
                        style = MaterialTheme.typography.bodyMedium
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        text = "Audio TX is generated by the core. Android RX feeds captured audio to the native streaming receiver.",
                        color = Color(0xFF9FB3C8),
                        style = MaterialTheme.typography.bodySmall
                    )
                }
            }
        }
    }
}

@Composable
private fun HFTextField(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    singleLine: Boolean
) {
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        label = { Text(label) },
        singleLine = singleLine,
        minLines = if (singleLine) 1 else 3,
        maxLines = if (singleLine) 1 else 5,
        modifier = Modifier.fillMaxWidth(),
        colors = OutlinedTextFieldDefaults.colors(
            focusedTextColor = Color.White,
            unfocusedTextColor = Color.White,
            focusedLabelColor = Color(0xFF9FB3C8),
            unfocusedLabelColor = Color(0xFF9FB3C8),
            cursorColor = Color(0xFFE6EDF3),
            focusedBorderColor = Color(0xFF7EA7D8),
            unfocusedBorderColor = Color(0xFF455263)
        )
    )
}

@Composable
private fun ProfileButton(
    label: String,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    if (selected) {
        Button(
            onClick = onClick,
            modifier = modifier,
            colors = ButtonDefaults.buttonColors(
                containerColor = Color(0xFF3C6F9F),
                contentColor = Color.White
            )
        ) {
            Text(label)
        }
    } else {
        OutlinedButton(
            onClick = onClick,
            modifier = modifier,
            colors = ButtonDefaults.outlinedButtonColors(
                contentColor = Color(0xFFE6EDF3)
            )
        ) {
            Text(label)
        }
    }
}

@Composable
private fun AudioInputButton(
    label: String,
    selected: Boolean,
    enabled: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    if (selected) {
        Button(
            onClick = onClick,
            enabled = enabled,
            modifier = modifier,
            colors = ButtonDefaults.buttonColors(
                containerColor = Color(0xFF2F5B7E),
                contentColor = Color.White
            )
        ) {
            Text(label)
        }
    } else {
        OutlinedButton(
            onClick = onClick,
            enabled = enabled,
            modifier = modifier,
            colors = ButtonDefaults.outlinedButtonColors(
                contentColor = Color(0xFFE6EDF3)
            )
        ) {
            Text(label)
        }
    }
}

@Composable
private fun StatusRow(label: String, value: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(Color(0xFF202832), RoundedCornerShape(8.dp))
            .padding(horizontal = 14.dp, vertical = 12.dp),
        verticalAlignment = Alignment.Top
    ) {
        Text(
            text = label,
            color = Color(0xFF9FB3C8),
            style = MaterialTheme.typography.labelLarge,
            modifier = Modifier.width(100.dp)
        )
        Text(
            text = value,
            color = Color.White,
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.weight(1f)
        )
    }
}

private fun displayText(value: String): String {
    return value.ifBlank { "--" }
}

private fun receivedMessagesText(messages: List<String>): String {
    return if (messages.isEmpty()) {
        "--"
    } else {
        messages.joinToString("\n")
    }
}

private fun textStatus(analysis: HFTextTextAnalysis): String {
    return when {
        !analysis.nativeAvailable -> analysis.error
        analysis.status != "ok" -> analysis.error.ifBlank { "native analysis failed" }
        analysis.messageEmpty -> "message empty"
        analysis.payloadTooLong -> "payload too long"
        else -> "ready"
    }
}

private fun estimateTooLong(analysis: HFTextTextAnalysis, profile: HFTextSpeedProfile): Boolean {
    return when (profile) {
        HFTextSpeedProfile.Slow -> analysis.slowPayloadTooLong
        HFTextSpeedProfile.Fast -> analysis.fastPayloadTooLong
    }
}

private fun estimateText(analysis: HFTextTextAnalysis, slow: Boolean): String {
    if (analysis.messageEmpty) {
        return "--"
    }

    val tooLong = if (slow) analysis.slowPayloadTooLong else analysis.fastPayloadTooLong
    if (analysis.payloadTooLong || tooLong) {
        return "payload too long"
    }

    val seconds = if (slow) analysis.slowDurationSeconds else analysis.fastDurationSeconds
    val bits = if (slow) analysis.slowTransmissionBits else analysis.fastTransmissionBits
    return "${formatSeconds(seconds)} | $bits bits"
}

private fun estimateSeconds(analysis: HFTextTextAnalysis, profile: HFTextSpeedProfile): Double {
    if (analysis.messageEmpty || analysis.payloadTooLong || estimateTooLong(analysis, profile)) {
        return 0.0
    }
    return when (profile) {
        HFTextSpeedProfile.Slow -> analysis.slowDurationSeconds
        HFTextSpeedProfile.Fast -> analysis.fastDurationSeconds
    }
}

private fun rxBufferText(
    seconds: Double,
    analysis: HFTextTextAnalysis,
    profile: HFTextSpeedProfile
): String {
    val expected = estimateSeconds(analysis, profile)
    return if (expected > 0.0) {
        "${formatSeconds(seconds)} captured | local TX estimate ${formatSeconds(expected)}"
    } else {
        "${formatSeconds(seconds)} captured"
    }
}

private fun emptyAudioStats(): HFTextAudioStats {
    return HFTextAudioStats(
        ok = true,
        error = "",
        sampleCount = 0L,
        peak = 0.0f,
        clippedSamples = 0L,
        clippingPercent = 0.0,
        durationSeconds = 0.0
    )
}

private fun rxLevelText(
    rawStats: HFTextAudioStats,
    receiverStats: HFTextAudioStats,
    receiverGain: Float
): String {
    if (!rawStats.ok) {
        return rawStats.error.ifBlank { "unavailable" }
    }
    if (!receiverStats.ok) {
        return receiverStats.error.ifBlank { "unavailable" }
    }
    if (rawStats.sampleCount <= 0) {
        return "--"
    }

    val levelHint = when {
        rawStats.peak > 0.90f -> " high"
        rawStats.peak >= 0.001f && rawStats.peak < 0.08f -> " low"
        else -> ""
    }
    return "raw ${formatPercent(rawStats.peak.toDouble())}$levelHint | modem ${
        formatPercent(receiverStats.peak.toDouble())
    } | gain x${formatGain(receiverGain)} | clip ${formatPercent(rawStats.clippingPercent / 100.0)}"
}

private fun receiverStatusText(update: HFTextReceiverUpdate): String {
    val parts = mutableListOf(update.state.ifBlank { "listening" })
    if (update.progress >= 0.0) {
        parts += "progress ${formatPercent(update.progress)}"
    }
    if (update.quality >= 0.0) {
        parts += "quality ${formatPercent(update.quality)}"
    }
    if (update.eventCount > 0) {
        parts += "events ${update.eventCount}"
    }
    return parts.joinToString(" | ")
}

private fun currentClockText(): String {
    return SimpleDateFormat("HH:mm:ss", Locale.US).format(Date())
}

private fun formatSeconds(seconds: Double): String {
    return String.format(Locale.US, "%.2f s", seconds)
}

private fun formatPercent(value: Double): String {
    return String.format(Locale.US, "%.1f%%", value.coerceIn(0.0, 1.0) * 100.0)
}

private fun formatGain(value: Float): String {
    return String.format(Locale.US, "%.1f", value.coerceAtLeast(1.0f))
}

@Preview
@Composable
private fun HFTextAppPreview() {
    HFTextScreen(
        nativeSnapshot = previewSnapshot(),
        analyzeText = { _, _ -> previewAnalysis() },
        generateTransmitAudio = { _, _, _ ->
            HFTextGeneratedAudio(
                ok = true,
                error = "",
                sampleRate = 48000,
                samples = FloatArray(0)
            )
        },
        receiveSampleRate = { 48000 },
        analyzeAudioSamples = { _, _ -> previewAudioStats() },
        createReceiver = { previewReceiverSession() }
    )
}

private fun previewSnapshot(): HFTextNativeSnapshot {
    return HFTextNativeSnapshot(
        nativeAvailable = true,
        bridgeStatus = "JNI OK via C ABI",
        core = "HFText 0.3.0 (experimental)",
        protocol = "HFText Basic v0.1",
        slowProfile = "8-FSK, 0.300 s/symbol, 1050-1960 Hz",
        fastProfile = "8-FSK, 0.100 s/symbol, 1050-1960 Hz"
    )
}

private fun previewAnalysis(): HFTextTextAnalysis {
    return HFTextTextAnalysis(
        nativeAvailable = true,
        status = "ok",
        error = "",
        sanitizedMessage = "Hello HFText!",
        payload = "pu5lrk Hello HFText!",
        messageSymbols = 13,
        payloadSymbols = 20,
        maxPayloadSymbols = 127,
        payloadTooLong = false,
        messageEmpty = false,
        slowDurationSeconds = 147.6,
        slowTransmissionBits = 492,
        slowPayloadTooLong = false,
        fastDurationSeconds = 49.2,
        fastTransmissionBits = 492,
        fastPayloadTooLong = false
    )
}

private fun previewAudioStats(): HFTextAudioStats {
    return HFTextAudioStats(
        ok = true,
        error = "",
        sampleCount = 4096,
        peak = 0.24f,
        clippedSamples = 0L,
        clippingPercent = 0.0,
        durationSeconds = 0.085
    )
}

private fun previewReceiverSession(): HFTextReceiverSession {
    return object : HFTextReceiverSession {
        override fun pushSamples(samples: FloatArray): HFTextReceiverUpdate {
            return HFTextReceiverUpdate(
                ok = true,
                error = "",
                messages = emptyList(),
                state = "listening",
                progress = -1.0,
                quality = -1.0,
                accepted = 0L,
                rejected = 0L,
                sync = 0L,
                eventCount = 0L
            )
        }

        override fun close() {
        }
    }
}
