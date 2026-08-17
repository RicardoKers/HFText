#pragma once

#include <QString>
#include <QStringList>

namespace hftext_pc {

QString messageSenderCallsign(const QString& message);
bool messageMatchesSender(const QString& message, const QString& callsign);
QStringList senderCallsignsFromMessages(const QStringList& messages);

}  // namespace hftext_pc
