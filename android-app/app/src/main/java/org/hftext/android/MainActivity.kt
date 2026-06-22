package org.hftext.android

import android.Manifest
import android.content.ClipData
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.FileProvider
import androidx.compose.foundation.background
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
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
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.roundToInt

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            HFTextApp()
        }
    }
}

private data class ReceivedMessage(
    val clockText: String,
    val dateTimeText: String,
    val text: String
)

private enum class AndroidPanel {
    Operation,
    Diagnostics
}

@Composable
private fun HFTextApp() {
    val nativeSnapshot = remember { HFTextNativeBridge.snapshot() }
    HFTextScreen(
        nativeSnapshot = nativeSnapshot,
        analyzeText = HFTextNativeBridge::analyzeText,
        generateTransmitAudio = HFTextNativeBridge::generateTransmitAudio,
        receiveSampleRate = HFTextNativeBridge::receiveSampleRate,
        toneFrequencies = HFTextNativeBridge::toneFrequencies,
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
    toneFrequencies: (HFTextSpeedProfile) -> FloatArray,
    analyzeAudioSamples: (FloatArray, Int) -> HFTextAudioStats,
    createReceiver: (HFTextSpeedProfile) -> HFTextReceiverSession
) {
    val context = LocalContext.current
    var callsign by remember { mutableStateOf("pu5lrk") }
    var message by remember { mutableStateOf("Hello HFText!") }
    var selectedProfile by remember { mutableStateOf(HFTextSpeedProfile.Fast) }
    var isTransmitting by remember { mutableStateOf(false) }
    var isReceiving by remember { mutableStateOf(false) }
    var selectedPanel by remember { mutableStateOf(AndroidPanel.Operation) }
    var rxInputMode by remember { mutableStateOf(HFTextAudioInputMode.VoiceRecognition) }
    var txStatus by remember { mutableStateOf("ready") }
    var rxStatus by remember { mutableStateOf("stopped") }
    var rxStats by remember { mutableStateOf(emptyAudioStats()) }
    var rxReceiverStats by remember { mutableStateOf(emptyAudioStats()) }
    var rxReceiverGain by remember { mutableStateOf(1.0f) }
    var rxBufferSeconds by remember { mutableStateOf(0.0) }
    var rxDecodeStatus by remember { mutableStateOf("decoder idle") }
    var rxEvidenceStatus by remember { mutableStateOf("not saved") }
    var isSavingRxEvidence by remember { mutableStateOf(false) }
    var lastRxEvidenceFiles by remember { mutableStateOf(emptyList<File>()) }
    var rxAccepted by remember { mutableStateOf(0L) }
    var rxRejected by remember { mutableStateOf(0L) }
    var rxSync by remember { mutableStateOf(0L) }
    var rxEvents by remember { mutableStateOf(0L) }
    var receivedMessages by remember { mutableStateOf(emptyList<ReceivedMessage>()) }
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
    val selectedToneFrequencies = remember(selectedProfile) {
        toneFrequencies(selectedProfile).toList()
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
                        val now = Date()
                        receivedMessages = (receivedMessages + update.messages.map {
                            ReceivedMessage(
                                clockText = currentClockText(now),
                                dateTimeText = currentDateTimeText(now),
                                text = it
                            )
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
        if (isSavingRxEvidence) {
            return
        }
        isSavingRxEvidence = true
        lastRxEvidenceFiles = emptyList()
        rxEvidenceStatus = "saving RX evidence..."
        val expectedTxSeconds = estimateSeconds(analysis, selectedProfile)
        Thread {
            try {
                val directory = context.getExternalFilesDir("rx-evidence") ?: context.filesDir
                val savedAudio = audioRecorder.saveDebugAudio(directory)
                val reportFile = evidenceReportFile(savedAudio)
                reportFile.writeText(
                    buildRxEvidenceReport(
                        nativeSnapshot = nativeSnapshot,
                        callsign = callsign,
                        selectedProfile = selectedProfile,
                        inputMode = rxInputMode,
                        analysis = analysis,
                        rxStatus = rxStatus,
                        rxDecodeStatus = rxDecodeStatus,
                        rxStats = rxStats,
                        rxReceiverStats = rxReceiverStats,
                        rxReceiverGain = rxReceiverGain,
                        rxBufferSeconds = rxBufferSeconds,
                        rxAccepted = rxAccepted,
                        rxRejected = rxRejected,
                        rxSync = rxSync,
                        rxEvents = rxEvents,
                        receivedMessages = receivedMessages,
                        toneFrequencies = selectedToneFrequencies,
                        savedAudio = savedAudio
                    )
                )
                mainHandler.post {
                    isSavingRxEvidence = false
                    lastRxEvidenceFiles = evidenceFiles(reportFile, savedAudio)
                    val duration = savedAudio.durationSeconds
                    val durationText = if (expectedTxSeconds > 0.0 && duration + 0.5 < expectedTxSeconds) {
                        "saved ${formatSeconds(duration)}; shorter than local TX estimate ${formatSeconds(expectedTxSeconds)}"
                    } else {
                        "saved ${formatSeconds(duration)}"
                    }
                    rxEvidenceStatus = "$durationText | report ${reportFile.name}"
                }
            } catch (error: Throwable) {
                mainHandler.post {
                    isSavingRxEvidence = false
                    rxEvidenceStatus = "save failed: ${error.message ?: error::class.java.simpleName}"
                }
            }
        }.apply {
            name = "HFTextSaveRxAudio"
            isDaemon = true
            start()
        }
    }

    fun shareRxEvidence() {
        val files = lastRxEvidenceFiles.filter { it.isFile }
        if (files.isEmpty()) {
            rxEvidenceStatus = "save RX evidence first"
            return
        }

        try {
            val uris = files.map { file ->
                FileProvider.getUriForFile(
                    context,
                    "${context.packageName}.fileprovider",
                    file
                )
            }
            val clipData = ClipData.newUri(context.contentResolver, "RX evidence", uris.first())
            uris.drop(1).forEach { uri ->
                clipData.addItem(ClipData.Item(uri))
            }
            val shareIntent = Intent(Intent.ACTION_SEND_MULTIPLE).apply {
                type = "*/*"
                putParcelableArrayListExtra(Intent.EXTRA_STREAM, ArrayList(uris))
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                this.clipData = clipData
            }
            context.startActivity(Intent.createChooser(shareIntent, "Share RX evidence"))
        } catch (error: Throwable) {
            rxEvidenceStatus = "share failed: ${error.message ?: error::class.java.simpleName}"
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

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    ProfileButton(
                        label = "Operation",
                        selected = selectedPanel == AndroidPanel.Operation,
                        onClick = { selectedPanel = AndroidPanel.Operation },
                        modifier = Modifier.weight(1f)
                    )
                    ProfileButton(
                        label = "Diagnostics",
                        selected = selectedPanel == AndroidPanel.Diagnostics,
                        onClick = { selectedPanel = AndroidPanel.Diagnostics },
                        modifier = Modifier.weight(1f)
                    )
                }

                if (selectedPanel == AndroidPanel.Operation) {
                    ReceivedMessagesPanel(
                        messages = receivedMessages,
                        onClear = { receivedMessages = emptyList() }
                    )

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
                            enabled = !isSavingRxEvidence,
                            modifier = Modifier.fillMaxWidth(),
                            colors = ButtonDefaults.outlinedButtonColors(
                                contentColor = Color(0xFFE6EDF3)
                            )
                        ) {
                            Text("Save RX evidence")
                        }
                        OutlinedButton(
                            onClick = ::shareRxEvidence,
                            enabled = lastRxEvidenceFiles.isNotEmpty() && !isSavingRxEvidence,
                            modifier = Modifier.fillMaxWidth(),
                            colors = ButtonDefaults.outlinedButtonColors(
                                contentColor = Color(0xFFE6EDF3)
                            )
                        ) {
                            Text("Share RX evidence")
                        }
                    }

                    Column(
                        modifier = Modifier.fillMaxWidth(),
                        verticalArrangement = Arrangement.spacedBy(10.dp)
                    ) {
                        StatusRow(label = "Text", value = textStatus(analysis))
                        StatusRow(label = "TX", value = txStatus)
                        StatusRow(label = "RX", value = rxStatus)
                        StatusRow(label = "RX level", value = rxLevelText(rxStats, rxReceiverStats, rxReceiverGain))
                        StatusRow(label = "RX evidence", value = rxEvidenceStatus)
                        StatusRow(
                            label = "Symbols",
                            value = "${analysis.payloadSymbols}/${analysis.maxPayloadSymbols} payload | ${analysis.messageSymbols} message"
                        )
                        StatusRow(
                            label = "${selectedProfile.label} TX",
                            value = estimateText(analysis, slow = selectedProfile == HFTextSpeedProfile.Slow)
                        )
                    }
                } else {
                    Column(
                        modifier = Modifier.fillMaxWidth(),
                        verticalArrangement = Arrangement.spacedBy(10.dp)
                    ) {
                        StatusRow(label = "Bridge", value = nativeSnapshot.bridgeStatus)
                        StatusRow(label = "Core", value = nativeSnapshot.core)
                        StatusRow(label = "Protocol", value = nativeSnapshot.protocol)
                        StatusRow(label = "Slow", value = nativeSnapshot.slowProfile)
                        StatusRow(label = "Fast", value = nativeSnapshot.fastProfile)
                        StatusRow(label = "Tones", value = toneFrequenciesText(selectedToneFrequencies))
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
private fun ReceivedMessagesPanel(
    messages: List<ReceivedMessage>,
    onClear: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = 150.dp)
            .background(Color(0xFF202832), RoundedCornerShape(8.dp))
            .padding(horizontal = 14.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "Received",
                color = Color(0xFF9FB3C8),
                style = MaterialTheme.typography.labelLarge
            )
            Spacer(modifier = Modifier.weight(1f))
            OutlinedButton(
                onClick = onClear,
                enabled = messages.isNotEmpty(),
                colors = ButtonDefaults.outlinedButtonColors(
                    contentColor = Color(0xFFE6EDF3)
                )
            ) {
                Text("Clear")
            }
        }

        if (messages.isEmpty()) {
            Text(
                text = "--",
                color = Color.White,
                style = MaterialTheme.typography.bodyMedium
            )
        } else {
            messages.forEach { message ->
                Text(
                    text = "[${message.clockText}] ${message.text}",
                    color = Color.White,
                    style = MaterialTheme.typography.bodyMedium
                )
            }
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

private fun evidenceReportFile(savedAudio: HFTextSavedRxAudio): File {
    val modemFile = File(savedAudio.modemPath)
    val directory = modemFile.parentFile ?: File(".")
    val baseName = modemFile.name.removeSuffix("-modem.wav").ifBlank {
        modemFile.nameWithoutExtension
    }
    return File(directory, "$baseName.txt")
}

private fun evidenceFiles(reportFile: File, savedAudio: HFTextSavedRxAudio): List<File> {
    return listOf(
        reportFile,
        File(savedAudio.rawPath),
        File(savedAudio.modemPath)
    ).filter { it.isFile }
}

private fun buildRxEvidenceReport(
    nativeSnapshot: HFTextNativeSnapshot,
    callsign: String,
    selectedProfile: HFTextSpeedProfile,
    inputMode: HFTextAudioInputMode,
    analysis: HFTextTextAnalysis,
    rxStatus: String,
    rxDecodeStatus: String,
    rxStats: HFTextAudioStats,
    rxReceiverStats: HFTextAudioStats,
    rxReceiverGain: Float,
    rxBufferSeconds: Double,
    rxAccepted: Long,
    rxRejected: Long,
    rxSync: Long,
    rxEvents: Long,
    receivedMessages: List<ReceivedMessage>,
    toneFrequencies: List<Float>,
    savedAudio: HFTextSavedRxAudio
): String {
    val generatedAt = currentDateTimeText()
    return buildString {
        appendLine("HFText Android RX evidence")
        appendLine("Generated at: $generatedAt")
        appendLine("Bridge: ${nativeSnapshot.bridgeStatus}")
        appendLine("Core: ${nativeSnapshot.core}")
        appendLine("Protocol: ${nativeSnapshot.protocol}")
        appendLine("Profile: ${selectedProfile.label}")
        appendLine("Input mode: ${inputMode.label}")
        appendLine("Tones: ${toneFrequenciesText(toneFrequencies)}")
        appendLine("Callsign: $callsign")
        appendLine("RX status: $rxStatus")
        appendLine("Decoder: $rxDecodeStatus")
        appendLine("RX session: accepted $rxAccepted | rejected $rxRejected | sync $rxSync | events $rxEvents")
        appendLine("RX buffer: ${formatSeconds(rxBufferSeconds)}")
        appendLine("Saved audio: ${formatSeconds(savedAudio.durationSeconds)}")
        appendLine("Sample rate: ${savedAudio.sampleRate} Hz")
        appendLine("Samples: ${savedAudio.sampleCount}")
        appendLine("Raw level: ${rxLevelReportText(rxStats)}")
        appendLine("Modem level: ${rxLevelReportText(rxReceiverStats)}")
        appendLine("Receiver gain: x${formatGain(rxReceiverGain)}")
        appendLine("Raw WAV: ${savedAudio.rawPath}")
        appendLine("Modem WAV: ${savedAudio.modemPath}")
        appendLine("Sanitized: ${displayText(analysis.sanitizedMessage)}")
        appendLine("Payload: ${displayText(analysis.payload)}")
        appendLine("Symbols: ${analysis.payloadSymbols}/${analysis.maxPayloadSymbols} payload | ${analysis.messageSymbols} message")
        appendLine("Slow TX estimate: ${estimateText(analysis, slow = true)}")
        appendLine("Fast TX estimate: ${estimateText(analysis, slow = false)}")
        appendLine()
        appendLine("--- Received Text ---")
        if (receivedMessages.isEmpty()) {
            appendLine("--")
        } else {
            receivedMessages.forEach { message ->
                appendLine("[${message.dateTimeText}] ${message.text}")
            }
        }
        appendLine()
        appendLine("--- Summary CSV ---")
        appendLine(
            listOf(
                "generated_at",
                "core",
                "protocol",
                "profile",
                "input_mode",
                "callsign",
                "tones_hz",
                "rx_buffer_s",
                "saved_audio_s",
                "sample_rate_hz",
                "saved_samples",
                "raw_peak",
                "modem_peak",
                "receiver_gain",
                "raw_clipping_percent",
                "accepted",
                "rejected",
                "sync",
                "events",
                "received_lines",
                "raw_wav",
                "modem_wav"
            ).joinToString(",")
        )
        appendLine(
            listOf(
                generatedAt,
                nativeSnapshot.core,
                nativeSnapshot.protocol,
                selectedProfile.label,
                inputMode.label,
                callsign,
                toneFrequenciesCsvText(toneFrequencies),
                formatCsvNumber(rxBufferSeconds),
                formatCsvNumber(savedAudio.durationSeconds),
                savedAudio.sampleRate.toString(),
                savedAudio.sampleCount.toString(),
                formatCsvNumber(rxStats.peak.toDouble()),
                formatCsvNumber(rxReceiverStats.peak.toDouble()),
                formatCsvNumber(rxReceiverGain.toDouble()),
                formatCsvNumber(rxStats.clippingPercent),
                rxAccepted.toString(),
                rxRejected.toString(),
                rxSync.toString(),
                rxEvents.toString(),
                receivedMessages.size.toString(),
                savedAudio.rawPath,
                savedAudio.modemPath
            ).joinToString(",") { csvCell(it) }
        )
        appendLine()
        appendLine("--- Messages CSV ---")
        appendLine("received_at,text")
        receivedMessages.forEach { message ->
            appendLine(listOf(message.dateTimeText, message.text).joinToString(",") { csvCell(it) })
        }
    }
}

private fun rxLevelReportText(stats: HFTextAudioStats): String {
    if (!stats.ok) {
        return stats.error.ifBlank { "unavailable" }
    }
    if (stats.sampleCount <= 0) {
        return "--"
    }
    return "peak ${formatPercent(stats.peak.toDouble())} | clipped ${stats.clippedSamples} | clip ${formatCsvNumber(stats.clippingPercent)}%"
}

private fun toneFrequenciesText(frequencies: List<Float>): String {
    if (frequencies.isEmpty()) {
        return "--"
    }
    return frequencies.joinToString(" ") { frequency ->
        "${frequency.roundToInt()} Hz"
    }
}

private fun toneFrequenciesCsvText(frequencies: List<Float>): String {
    if (frequencies.isEmpty()) {
        return ""
    }
    return frequencies.joinToString(" ") { frequency ->
        frequency.roundToInt().toString()
    }
}

private fun csvCell(value: String): String {
    return "\"${value.replace("\"", "\"\"")}\""
}

private fun formatCsvNumber(value: Double): String {
    return String.format(Locale.US, "%.6f", value)
}

private fun currentClockText(date: Date = Date()): String {
    return SimpleDateFormat("HH:mm:ss", Locale.US).format(date)
}

private fun currentDateTimeText(date: Date = Date()): String {
    return SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(date)
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
        toneFrequencies = { floatArrayOf(1050.0f, 1180.0f, 1310.0f, 1440.0f, 1570.0f, 1700.0f, 1830.0f, 1960.0f) },
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
