#include "TxMessageEdit.h"

#include <QKeyEvent>
#include <QTextCursor>

TxMessageEdit::TxMessageEdit(QWidget* parent)
    : QPlainTextEdit(parent) {
}

void TxMessageEdit::rememberSentMessage(const QString& message) {
    if (message.trimmed().isEmpty()) {
        return;
    }
    if (sentMessages_.isEmpty() || sentMessages_.back() != message) {
        sentMessages_.push_back(message);
        while (sentMessages_.size() > kMaxHistoryEntries) {
            sentMessages_.removeFirst();
        }
    }
    leaveHistoryNavigation();
}

void TxMessageEdit::keyPressEvent(QKeyEvent* event) {
    const bool enter = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
    if (enter && !(event->modifiers() & Qt::ShiftModifier)) {
        event->accept();
        emit sendRequested();
        return;
    }

    if (event->modifiers() == Qt::NoModifier && event->key() == Qt::Key_Up && recallPreviousMessage()) {
        event->accept();
        return;
    }
    if (event->modifiers() == Qt::NoModifier && event->key() == Qt::Key_Down && recallNextMessage()) {
        event->accept();
        return;
    }

    if (historyIndex_ >= 0 && !event->text().isEmpty()) {
        leaveHistoryNavigation();
    }
    QPlainTextEdit::keyPressEvent(event);
}

bool TxMessageEdit::recallPreviousMessage() {
    if (sentMessages_.isEmpty()) {
        return false;
    }
    if (historyIndex_ < 0) {
        draftText_ = toPlainText();
        historyIndex_ = sentMessages_.size() - 1;
    } else if (historyIndex_ > 0) {
        --historyIndex_;
    }
    showHistoryText(sentMessages_.at(historyIndex_));
    return true;
}

bool TxMessageEdit::recallNextMessage() {
    if (historyIndex_ < 0) {
        return false;
    }
    if (historyIndex_ + 1 < sentMessages_.size()) {
        ++historyIndex_;
        showHistoryText(sentMessages_.at(historyIndex_));
    } else {
        const QString draft = draftText_;
        leaveHistoryNavigation();
        showHistoryText(draft);
    }
    return true;
}

void TxMessageEdit::showHistoryText(const QString& text) {
    setPlainText(text);
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    setTextCursor(cursor);
}

void TxMessageEdit::leaveHistoryNavigation() {
    historyIndex_ = -1;
    draftText_.clear();
}
