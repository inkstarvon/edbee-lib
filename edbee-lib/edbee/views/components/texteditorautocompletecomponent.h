#pragma once

#include <QModelIndex>
#include <QWidget>
#include <QLabel>
#include <QAbstractItemDelegate>
#include <QPainter>
#include <QScrollBar>
#include <QStylePainter>
#include <QPointer>
#include <QTextDocument>

class QListWidget;
class QListWidgetItem;

namespace edbee {

class TextDocument;
class TextEditorController;
class TextEditorComponent;
class TextEditorWidget;
class TextRange;

class FakeToolTip : public QWidget
{
    Q_OBJECT

public:
    explicit FakeToolTip(QWidget *parent = 0);
    void setText(const QString text);
    QTextDocument tipText;

protected:
    void paintEvent(QPaintEvent *e);
    void resizeEvent(QResizeEvent *e);
};

// inspiration:
// http://doc.qt.io/qt-5/qtwidgets-tools-customcompleter-example.html

/// An autocomplete list
/// Which receives it's autocomplete list from the document
class TextEditorAutoCompleteComponent : public QWidget
{
    Q_OBJECT
public:
    explicit TextEditorAutoCompleteComponent( TextEditorController* controller, TextEditorComponent *parent);

    TextEditorController* controller();

    QSize sizeHint() const;

protected:

    bool shouldDisplayAutoComplete(TextRange& range, QString& word);
    void showInfoTip();
    void hideInfoTip();
    bool fillAutoCompleteList(TextDocument *document, const TextRange &range, const QString& word );

    void positionWidgetForCaretOffset(int offset);
    bool eventFilter(QObject* obj, QEvent* event);

    void hideEvent(QHideEvent *event);
    void moveEvent(QMoveEvent *event);

    void insertCurrentSelectedListItem();
signals:

public slots:
    void backspacePressed();
    void textKeyPressed();
    void listItemClicked(QListWidgetItem*item);
    void listItemDoubleClicked(QListWidgetItem*item);
    void selectItemOnHover(QModelIndex modelIndex);

private:
    TextEditorController* controllerRef_;       ///< A reference to the controller
    QListWidget* listWidgetRef_;                ///< The current autocomplete words
    TextEditorComponent* editorComponentRef_;   ///< Reference to the editor component
    bool eventBeingFiltered_;                   ///< Prevent endless double filter when forwarding event to list item
    QString currentWord_;                       ///< The current word beïng entered
    QPointer<FakeToolTip> infoTipRef_;
};

class AutoCompleteDelegate : public QAbstractItemDelegate
{
    Q_OBJECT

public:
    AutoCompleteDelegate(QObject *parent = 0);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index ) const override;

public slots:

private:
    int pixelSize;
};

}
