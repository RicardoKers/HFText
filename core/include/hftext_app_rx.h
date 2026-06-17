#pragma once

#include "hftext_streaming_receiver.h"

#include <vector>

namespace hftext {

constexpr int kDefaultStrongSyncMismatchLimit = 4;
constexpr float kDefaultRejectedCandidateConfidenceFloor = 0.10F;
constexpr int kDefaultFrameProgressLogStepPercent = 10;

struct RxEventSelection {
    int bestDecoded = -1;
    int bestLength = -1;
    int bestWaiting = -1;
    int bestSync = -1;
    int bestRejected = -1;
    int rejectedCount = 0;
    bool hasInvalidLength = false;
};

struct RxSessionEventCounts {
    int sync = 0;
    int length = 0;
    int rejected = 0;
};

bool isStrongSyncEvent(
    const StreamingReceiverEvent& event,
    int mismatchLimit = kDefaultStrongSyncMismatchLimit
);

bool isDisplayableRejectedEvent(
    const StreamingReceiverEvent& event,
    float confidenceFloor = kDefaultRejectedCandidateConfidenceFloor,
    int mismatchLimit = kDefaultStrongSyncMismatchLimit
);

int frameProgressPercent(const StreamingReceiverEvent& event);
int frameProgressPermille(const StreamingReceiverEvent& event);
int frameProgressLogMilestone(
    int percent,
    int stepPercent = kDefaultFrameProgressLogStepPercent
);

bool isBetterSyncEvent(
    const StreamingReceiverEvent& candidate,
    const StreamingReceiverEvent* current
);

bool isBetterWaitingEvent(
    const StreamingReceiverEvent& candidate,
    const StreamingReceiverEvent* current
);

RxEventSelection selectRxEvents(
    const std::vector<StreamingReceiverEvent>& events,
    float rejectedConfidenceFloor = kDefaultRejectedCandidateConfidenceFloor,
    int strongSyncMismatchLimit = kDefaultStrongSyncMismatchLimit
);

int rxQualityPermille(
    const std::vector<StreamingReceiverEvent>& events,
    float rejectedConfidenceFloor = kDefaultRejectedCandidateConfidenceFloor,
    int strongSyncMismatchLimit = kDefaultStrongSyncMismatchLimit
);

RxSessionEventCounts rxSessionEventCounts(
    const std::vector<StreamingReceiverEvent>& events,
    float rejectedConfidenceFloor = kDefaultRejectedCandidateConfidenceFloor,
    int strongSyncMismatchLimit = kDefaultStrongSyncMismatchLimit
);

bool hasTerminalRxCandidate(
    const std::vector<StreamingReceiverEvent>& events,
    float rejectedConfidenceFloor = kDefaultRejectedCandidateConfidenceFloor,
    int strongSyncMismatchLimit = kDefaultStrongSyncMismatchLimit
);

}  // namespace hftext
