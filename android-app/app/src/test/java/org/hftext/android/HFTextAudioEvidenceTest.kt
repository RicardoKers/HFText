package org.hftext.android

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class HFTextAudioEvidenceTest {
    @Test
    fun summarizesTheCompleteSavedWindow() {
        val raw = floatArrayOf(0.0f, 0.10f, -0.50f, 1.0f)
        val modem = floatArrayOf(0.0f, 0.20f, -1.0f, 1.0f)

        val summary = summarizeEvidenceAudio(raw, modem, sampleRate = 4)

        assertTrue(summary.rawStats.ok)
        assertEquals(4L, summary.rawStats.sampleCount)
        assertEquals(1.0f, summary.rawStats.peak, 0.0001f)
        assertEquals(1L, summary.rawStats.clippedSamples)
        assertEquals(25.0, summary.rawStats.clippingPercent, 0.0001)
        assertEquals(1.0, summary.rawStats.durationSeconds, 0.0001)
        assertEquals(2L, summary.modemStats.clippedSamples)
        assertTrue(summary.effectiveGain > 1.0f)
    }

    @Test
    fun silentWindowUsesUnityEffectiveGain() {
        val summary = summarizeEvidenceAudio(FloatArray(8), FloatArray(8), sampleRate = 8)

        assertEquals(1.0f, summary.effectiveGain, 0.0001f)
        assertEquals(0.0f, summary.rawStats.peak, 0.0001f)
        assertFalse(summary.rawStats.error.isNotEmpty())
    }
}
