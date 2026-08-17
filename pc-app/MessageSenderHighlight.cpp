#include "MessageSenderHighlight.h"

#include <QSet>

#include <algorithm>

namespace hftext_pc {

QString messageSenderCallsign(const QString& message) {
    const QString trimmed = message.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    qsizetype end = 0;
    while (end < trimmed.size() && !trimmed.at(end).isSpace()) {
        ++end;
    }
    return trimmed.left(end).toUpper();
}

bool messageMatchesSender(const QString& message, const QString& callsign) {
    const QString normalized = callsign.trimmed().toUpper();
    return !normalized.isEmpty() && messageSenderCallsign(message) == normalized;
}

QStringList senderCallsignsFromMessages(const QStringList& messages) {
    QSet<QString> uniqueCallsigns;
    for (const QString& message : messages) {
        const QString callsign = messageSenderCallsign(message);
        if (!callsign.isEmpty()) {
            uniqueCallsigns.insert(callsign);
        }
    }

    QStringList callsigns(uniqueCallsigns.begin(), uniqueCallsigns.end());
    std::sort(callsigns.begin(), callsigns.end(), [](const QString& left, const QString& right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });
    return callsigns;
}

}  // namespace hftext_pc
