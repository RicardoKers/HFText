package org.hftext.android

import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference

internal data class HFTextProfiledReceiverUpdate(
    val profile: HFTextSpeedProfile,
    val update: HFTextReceiverUpdate
)

internal class HFTextReceiverProfileController(
    initialProfile: HFTextSpeedProfile,
    private val createReceiver: (HFTextSpeedProfile) -> HFTextReceiverSession
) : AutoCloseable {
    private val requestedProfile = AtomicReference(initialProfile)
    private val resetRequested = AtomicBoolean(false)
    private var activeProfile = initialProfile
    private var receiver = createReceiver(initialProfile)

    fun requestProfile(profile: HFTextSpeedProfile) {
        requestedProfile.set(profile)
    }

    fun requestReset() {
        resetRequested.set(true)
    }

    fun applyPendingChanges(): Boolean {
        val nextProfile = requestedProfile.get()
        if (!resetRequested.getAndSet(false) && nextProfile == activeProfile) {
            return false
        }

        receiver.close()
        activeProfile = nextProfile
        receiver = createReceiver(activeProfile)
        return true
    }

    fun pushSamples(samples: FloatArray): HFTextProfiledReceiverUpdate {
        return HFTextProfiledReceiverUpdate(
            profile = activeProfile,
            update = receiver.pushSamples(samples)
        )
    }

    override fun close() {
        receiver.close()
    }
}
