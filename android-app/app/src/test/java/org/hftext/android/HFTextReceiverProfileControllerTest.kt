package org.hftext.android

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class HFTextReceiverProfileControllerTest {
    @Test
    fun switchingProfileRecreatesReceiverBeforeProcessingMoreAudio() {
        val createdProfiles = mutableListOf<HFTextSpeedProfile>()
        val sessions = mutableListOf<FakeReceiverSession>()
        val controller = HFTextReceiverProfileController(HFTextSpeedProfile.Fast) { profile ->
            createdProfiles += profile
            FakeReceiverSession(profile).also { sessions += it }
        }

        val fastUpdate = controller.pushSamples(floatArrayOf(0.1f))
        controller.requestProfile(HFTextSpeedProfile.Slow)
        val changed = controller.applyPendingChanges()
        val slowUpdate = controller.pushSamples(floatArrayOf(0.2f))

        assertTrue(changed)
        assertEquals(listOf(HFTextSpeedProfile.Fast, HFTextSpeedProfile.Slow), createdProfiles)
        assertEquals(HFTextSpeedProfile.Fast, fastUpdate.profile)
        assertEquals(HFTextSpeedProfile.Slow, slowUpdate.profile)
        assertTrue(sessions.first().closed)
        assertFalse(sessions.last().closed)

        controller.close()
        assertTrue(sessions.last().closed)
    }

    @Test
    fun requestingCurrentProfileDoesNotRecreateReceiver() {
        var createCount = 0
        val controller = HFTextReceiverProfileController(HFTextSpeedProfile.Slow) { profile ->
            ++createCount
            FakeReceiverSession(profile)
        }

        controller.requestProfile(HFTextSpeedProfile.Slow)

        assertFalse(controller.applyPendingChanges())
        assertEquals(1, createCount)
        controller.close()
    }

    private class FakeReceiverSession(
        private val profile: HFTextSpeedProfile
    ) : HFTextReceiverSession {
        var closed = false

        override fun pushSamples(samples: FloatArray): HFTextReceiverUpdate {
            return HFTextReceiverUpdate(
                ok = true,
                error = "",
                messages = listOf(profile.label),
                state = "",
                progress = 0.0,
                quality = 0.0,
                accepted = 0L,
                rejected = 0L,
                sync = 0L,
                eventCount = 0L,
                acceptedLatencies = emptyList()
            )
        }

        override fun close() {
            closed = true
        }
    }
}
