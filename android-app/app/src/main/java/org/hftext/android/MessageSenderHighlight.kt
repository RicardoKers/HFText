package org.hftext.android

import java.util.Locale

internal fun messageSenderCallsign(message: String): String? {
    val trimmed = message.trim()
    if (trimmed.isEmpty()) {
        return null
    }
    return trimmed.takeWhile { !it.isWhitespace() }.uppercase(Locale.ROOT)
}

internal fun messageMatchesSender(message: String, callsign: String?): Boolean {
    val normalized = callsign?.trim()?.uppercase(Locale.ROOT).orEmpty()
    return normalized.isNotEmpty() && messageSenderCallsign(message) == normalized
}

internal fun senderCallsignsFromMessages(messages: Iterable<String>): List<String> {
    return messages
        .mapNotNull(::messageSenderCallsign)
        .distinct()
        .sorted()
}
