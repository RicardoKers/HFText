package org.hftext.android

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class MessageSenderHighlightTest {
    @Test
    fun extractsNormalizedFirstToken() {
        assertEquals("PU5LRK", messageSenderCallsign(" pu5lrk Hello"))
        assertEquals("N0CALL", messageSenderCallsign("N0CALL\nmultiline"))
        assertNull(messageSenderCallsign("   "))
    }

    @Test
    fun matchesOnlyTheCompleteSenderToken() {
        assertTrue(messageMatchesSender("Pu5Lrk CQ", "pu5lrk"))
        assertFalse(messageMatchesSender("PU5LRK2 CQ", "PU5LRK"))
        assertFalse(messageMatchesSender("PU5LRK CQ", null))
    }

    @Test
    fun returnsUniqueSortedCallsigns() {
        assertEquals(
            listOf("N0CALL", "PU5LRK", "ZZ9Z"),
            senderCallsignsFromMessages(
                listOf("zz9z Last", "PU5LRK First", "pu5lrk Duplicate", "N0CALL Test", "")
            )
        )
    }
}
