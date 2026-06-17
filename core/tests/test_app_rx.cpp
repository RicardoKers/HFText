#include "hftext_app_rx.h"

#include <cassert>
#include <vector>

namespace {

hftext::StreamingReceiverEvent event(hftext::StreamingReceiverEventType type) {
    hftext::StreamingReceiverEvent output;
    output.type = type;
    return output;
}

}  // namespace

int main() {
    auto weakSync = event(hftext::StreamingReceiverEventType::SyncFound);
    weakSync.syncMismatches = 5;
    weakSync.confidence = 0.9F;
    assert(!hftext::isStrongSyncEvent(weakSync));

    auto strongSync = event(hftext::StreamingReceiverEventType::SyncFound);
    strongSync.syncMismatches = 2;
    strongSync.confidence = 0.4F;
    assert(hftext::isStrongSyncEvent(strongSync));

    auto waiting25 = event(hftext::StreamingReceiverEventType::FrameWaiting);
    waiting25.syncMismatches = 1;
    waiting25.bitsAvailable = 25;
    waiting25.bitsExpected = 100;
    waiting25.syncSample = 1000;
    assert(hftext::frameProgressPercent(waiting25) == 25);
    assert(hftext::frameProgressPermille(waiting25) == 250);
    assert(hftext::frameProgressLogMilestone(26) == 20);
    assert(hftext::frameProgressLogMilestone(100) == 100);

    auto waiting50 = waiting25;
    waiting50.bitsAvailable = 50;
    waiting50.syncSample = 1000;
    assert(hftext::isBetterWaitingEvent(waiting50, &waiting25));
    assert(!hftext::isBetterWaitingEvent(waiting25, &waiting50));

    auto length = event(hftext::StreamingReceiverEventType::PhysicalLengthRecovered);
    length.syncMismatches = 0;
    length.payloadLength = 12;
    length.bitsExpected = 260;

    auto weakRejected = event(hftext::StreamingReceiverEventType::FrameRejected);
    weakRejected.syncMismatches = 1;
    weakRejected.confidence = 0.05F;
    assert(!hftext::isDisplayableRejectedEvent(weakRejected));

    auto rejected = weakRejected;
    rejected.confidence = 0.65F;
    assert(hftext::isDisplayableRejectedEvent(rejected));

    auto decoded = event(hftext::StreamingReceiverEventType::FrameDecoded);
    decoded.confidence = 0.8F;

    const std::vector<hftext::StreamingReceiverEvent> events{
        weakSync,
        strongSync,
        waiting25,
        waiting50,
        length,
        weakRejected,
        rejected,
        decoded,
    };

    const auto selection = hftext::selectRxEvents(events);
    assert(selection.bestSync == 1);
    assert(selection.bestWaiting == 3);
    assert(selection.bestLength == 4);
    assert(selection.bestRejected == 6);
    assert(selection.bestDecoded == 7);
    assert(selection.rejectedCount == 1);
    assert(!selection.hasInvalidLength);
    assert(hftext::rxQualityPermille(events) == 800);
    assert(hftext::hasTerminalRxCandidate(events));

    const auto counts = hftext::rxSessionEventCounts(events);
    assert(counts.sync == 1);
    assert(counts.length == 1);
    assert(counts.rejected == 1);

    const std::vector<hftext::StreamingReceiverEvent> weakOnly{weakSync, weakRejected};
    assert(hftext::selectRxEvents(weakOnly).bestSync < 0);
    assert(hftext::rxQualityPermille(weakOnly) == -1);
    assert(!hftext::hasTerminalRxCandidate(weakOnly));

    auto invalidLength = event(hftext::StreamingReceiverEventType::PhysicalLengthInvalid);
    const std::vector<hftext::StreamingReceiverEvent> invalidOnly{invalidLength};
    assert(hftext::selectRxEvents(invalidOnly).hasInvalidLength);
}
