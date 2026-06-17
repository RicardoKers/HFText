#include "hftext_app_rx.h"

#include <algorithm>

namespace hftext {

bool isStrongSyncEvent(const StreamingReceiverEvent& event, int mismatchLimit) {
    return event.syncMismatches <= mismatchLimit;
}

bool isDisplayableRejectedEvent(
    const StreamingReceiverEvent& event,
    float confidenceFloor,
    int mismatchLimit
) {
    return isStrongSyncEvent(event, mismatchLimit) && event.confidence >= confidenceFloor;
}

int frameProgressPercent(const StreamingReceiverEvent& event) {
    if (event.bitsExpected <= 0) {
        return 0;
    }

    if (event.bitsAvailable >= event.bitsExpected) {
        return 100;
    }

    const double ratio = static_cast<double>(event.bitsAvailable) / static_cast<double>(event.bitsExpected);
    return static_cast<int>(std::clamp(ratio, 0.0, 1.0) * 100.0);
}

int frameProgressPermille(const StreamingReceiverEvent& event) {
    if (event.bitsExpected <= 0) {
        return 0;
    }

    if (event.bitsAvailable >= event.bitsExpected) {
        return 1000;
    }

    const double ratio = static_cast<double>(event.bitsAvailable) / static_cast<double>(event.bitsExpected);
    return static_cast<int>(std::clamp(ratio, 0.0, 1.0) * 1000.0);
}

int frameProgressLogMilestone(int percent, int stepPercent) {
    if (stepPercent <= 0) {
        stepPercent = kDefaultFrameProgressLogStepPercent;
    }
    if (percent >= 100) {
        return 100;
    }
    return std::clamp((std::max(0, percent) / stepPercent) * stepPercent, 0, 100);
}

bool isBetterSyncEvent(
    const StreamingReceiverEvent& candidate,
    const StreamingReceiverEvent* current
) {
    if (current == nullptr) {
        return true;
    }
    if (candidate.syncMismatches != current->syncMismatches) {
        return candidate.syncMismatches < current->syncMismatches;
    }
    return candidate.confidence > current->confidence;
}

bool isBetterWaitingEvent(
    const StreamingReceiverEvent& candidate,
    const StreamingReceiverEvent* current
) {
    if (current == nullptr) {
        return true;
    }
    const double candidateProgress = candidate.bitsExpected <= 0
        ? 0.0
        : static_cast<double>(candidate.bitsAvailable) / static_cast<double>(candidate.bitsExpected);
    const double currentProgress = current->bitsExpected <= 0
        ? 0.0
        : static_cast<double>(current->bitsAvailable) / static_cast<double>(current->bitsExpected);
    if (candidateProgress != currentProgress) {
        return candidateProgress > currentProgress;
    }
    return candidate.syncMismatches < current->syncMismatches;
}

RxEventSelection selectRxEvents(
    const std::vector<StreamingReceiverEvent>& events,
    float rejectedConfidenceFloor,
    int strongSyncMismatchLimit
) {
    RxEventSelection selection;

    auto eventAt = [&events](int index) -> const StreamingReceiverEvent* {
        if (index < 0) {
            return nullptr;
        }
        return &events[static_cast<std::size_t>(index)];
    };

    for (std::size_t index = 0; index < events.size(); ++index) {
        const auto& event = events[index];
        const auto eventIndex = static_cast<int>(index);
        switch (event.type) {
        case StreamingReceiverEventType::FrameDecoded:
            if (selection.bestDecoded < 0 || event.confidence > eventAt(selection.bestDecoded)->confidence) {
                selection.bestDecoded = eventIndex;
            }
            break;
        case StreamingReceiverEventType::FrameRejected:
            if (isDisplayableRejectedEvent(event, rejectedConfidenceFloor, strongSyncMismatchLimit)) {
                ++selection.rejectedCount;
                if (selection.bestRejected < 0 || event.confidence > eventAt(selection.bestRejected)->confidence) {
                    selection.bestRejected = eventIndex;
                }
            }
            break;
        case StreamingReceiverEventType::PhysicalLengthRecovered:
            if (isStrongSyncEvent(event, strongSyncMismatchLimit)
                && isBetterSyncEvent(event, eventAt(selection.bestLength))) {
                selection.bestLength = eventIndex;
            }
            break;
        case StreamingReceiverEventType::FrameWaiting:
            if (isStrongSyncEvent(event, strongSyncMismatchLimit)
                && isBetterWaitingEvent(event, eventAt(selection.bestWaiting))) {
                selection.bestWaiting = eventIndex;
            }
            break;
        case StreamingReceiverEventType::SyncFound:
            if (isStrongSyncEvent(event, strongSyncMismatchLimit)
                && isBetterSyncEvent(event, eventAt(selection.bestSync))) {
                selection.bestSync = eventIndex;
            }
            break;
        case StreamingReceiverEventType::PhysicalLengthInvalid:
            selection.hasInvalidLength = true;
            break;
        }
    }

    return selection;
}

int rxQualityPermille(
    const std::vector<StreamingReceiverEvent>& events,
    float rejectedConfidenceFloor,
    int strongSyncMismatchLimit
) {
    const auto selection = selectRxEvents(events, rejectedConfidenceFloor, strongSyncMismatchLimit);
    int bestQuality = -1;
    if (selection.bestDecoded >= 0) {
        bestQuality = std::max(
            bestQuality,
            static_cast<int>(
                std::clamp(events[static_cast<std::size_t>(selection.bestDecoded)].confidence, 0.0F, 1.0F) * 1000.0F
            )
        );
    }
    if (selection.bestRejected >= 0) {
        bestQuality = std::max(
            bestQuality,
            static_cast<int>(
                std::clamp(events[static_cast<std::size_t>(selection.bestRejected)].confidence, 0.0F, 1.0F) * 1000.0F
            )
        );
    }
    return bestQuality;
}

RxSessionEventCounts rxSessionEventCounts(
    const std::vector<StreamingReceiverEvent>& events,
    float rejectedConfidenceFloor,
    int strongSyncMismatchLimit
) {
    const auto selection = selectRxEvents(events, rejectedConfidenceFloor, strongSyncMismatchLimit);
    RxSessionEventCounts counts;
    if (selection.bestLength >= 0) {
        counts.sync = 1;
        counts.length = 1;
    } else if (selection.bestSync >= 0) {
        counts.sync = 1;
    }
    if (selection.bestRejected >= 0) {
        counts.rejected = 1;
    }
    return counts;
}

bool hasTerminalRxCandidate(
    const std::vector<StreamingReceiverEvent>& events,
    float rejectedConfidenceFloor,
    int strongSyncMismatchLimit
) {
    const auto selection = selectRxEvents(events, rejectedConfidenceFloor, strongSyncMismatchLimit);
    return selection.bestDecoded >= 0 || selection.bestRejected >= 0;
}

}  // namespace hftext
