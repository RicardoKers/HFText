#include "TxMessageEdit.h"

#include <QApplication>
#include <QKeyEvent>

#include <cassert>

namespace {

void sendKey(TxMessageEdit& edit, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    const QString text = key == Qt::Key_Return || key == Qt::Key_Enter ? "\r" : QString();
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(&edit, &event);
}

}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    TxMessageEdit edit;
    int sendRequests = 0;
    QObject::connect(&edit, &TxMessageEdit::sendRequested, [&sendRequests]() {
        ++sendRequests;
    });

    edit.setPlainText("CQ CQ");
    sendKey(edit, Qt::Key_Return);
    assert(sendRequests == 1);
    assert(edit.toPlainText() == "CQ CQ");

    sendKey(edit, Qt::Key_Return, Qt::ShiftModifier);
    assert(sendRequests == 1);
    assert(edit.toPlainText().contains('\n'));

    edit.rememberSentMessage("first");
    edit.rememberSentMessage("second");
    edit.setPlainText("draft");

    sendKey(edit, Qt::Key_Up);
    assert(edit.toPlainText() == "second");
    sendKey(edit, Qt::Key_Up);
    assert(edit.toPlainText() == "first");
    sendKey(edit, Qt::Key_Up);
    assert(edit.toPlainText() == "first");
    sendKey(edit, Qt::Key_Down);
    assert(edit.toPlainText() == "second");
    sendKey(edit, Qt::Key_Down);
    assert(edit.toPlainText() == "draft");

    return 0;
}
