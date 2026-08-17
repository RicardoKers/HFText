#pragma once

#include <QPlainTextEdit>
#include <QStringList>

class QKeyEvent;

class TxMessageEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit TxMessageEdit(QWidget* parent = nullptr);

    void rememberSentMessage(const QString& message);

signals:
    void sendRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    bool recallPreviousMessage();
    bool recallNextMessage();
    void showHistoryText(const QString& text);
    void leaveHistoryNavigation();

    static constexpr int kMaxHistoryEntries = 100;
    QStringList sentMessages_;
    QString draftText_;
    int historyIndex_ = -1;
};
