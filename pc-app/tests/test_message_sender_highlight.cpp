#include "MessageSenderHighlight.h"

#include <cassert>

int main() {
    assert(hftext_pc::messageSenderCallsign("pu5lrk Hello") == "PU5LRK");
    assert(hftext_pc::messageSenderCallsign("  N0CALL\nmultiline") == "N0CALL");
    assert(hftext_pc::messageSenderCallsign("   ").isEmpty());

    assert(hftext_pc::messageMatchesSender("Pu5Lrk CQ", "pu5lrk"));
    assert(!hftext_pc::messageMatchesSender("PU5LRK2 CQ", "PU5LRK"));
    assert(!hftext_pc::messageMatchesSender("PU5LRK CQ", ""));

    const QStringList callsigns = hftext_pc::senderCallsignsFromMessages({
        "zz9z Last",
        "PU5LRK First",
        "pu5lrk Duplicate",
        "N0CALL Test",
        ""
    });
    assert(callsigns == QStringList({"N0CALL", "PU5LRK", "ZZ9Z"}));
    return 0;
}
