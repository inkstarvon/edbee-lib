#include "accessibletexteditorwidget.h"

#include <QWidget>
#include <QAccessibleInterface>

#include "edbee/models/changes/textchange.h"
#include "edbee/models/textdocument.h"
#include "edbee/models/textrange.h"
#include "edbee/texteditorwidget.h"
#include "edbee/texteditorcontroller.h"
#include "edbee/views/textselection.h"
#include "edbee/views/textrenderer.h"
#include "edbee/views/components/texteditorcomponent.h"

#include "edbee/debug.h"

/// I've tried to use the TextEditorWidget direct.
/// This works for Windows Reader and Mac OS X Speaker, but doesn't seem to work for NVDA
/// The TexteditorComponent is the component that has the true focus
#define VIA_EDITOR_COMPONENT

/// QT Acessibility has an issue with reporting blank lines
/// between elements lines.
/// defining 'WINDOWS_EMPTY_LINE_READING_ERROR_FIX' adds a space before a newline.
/// Which is a workaround for this issue.
/// (It does some offset-length and string magic when this is enabled)
#if defined(Q_OS_WIN32)
#define WINDOWS_EMPTY_LINE_READING_ERROR_FIX
#endif

namespace edbee {

/// Workaround for the strang line-reading on windows
/// It virtually replaces every <newline> with <space><newline>
static int D2V(TextEditorWidget* widget, int offset)
{
#ifndef WINDOWS_EMPTY_LINE_READING_ERROR_FIX
    return offset;
#endif

    int line = widget->textDocument()->lineFromOffset(offset);
    return offset + line;
}

/// Converts the virtual-offset to the document-offset
static int V2D(TextEditorWidget* widget, int vOffset, bool debug=false)
{
#ifndef WINDOWS_EMPTY_LINE_READING_ERROR_FIX
    return vOffset;
#endif

  TextDocument* doc = widget->textDocument();
  int displacement = 0;
  for( int i=0, cnt = doc->lineCount(); i < cnt; ++i ) {
      int lineVOffset = doc->offsetFromLine(i) + i;
      //if(debug) { qDebug() << " =?>> i: " << i << ", vOffset: " << vOffset <<  ", lineVOffset: " << lineVOffset << " ::"; }

      if( vOffset < lineVOffset) {
        //if(debug) { qDebug() << " =A> " << vOffset << " - " <<  displacement << " => " << (vOffset - displacement); }
        return vOffset - displacement; // remove the newline count
      }
      displacement = i;
  }
  //if(debug) { qDebug() << " =B> " << vOffset << " - " << (doc->length()); }
  //  return doc->length();
  return vOffset - displacement;
}

/// returns the virtual length of the textdocument
static int VLEN(TextEditorWidget* widget)
{
#ifndef WINDOWS_EMPTY_LINE_READING_ERROR_FIX
    return widget->textDocument()->length();
#endif

    return widget->textDocument()->length() + widget->textDocument()->lineCount() - 1;
}

/// Converts the given text to a virtual text
/// It prepends every newline with a \n
static const QString VTEXT(QString str)
{
#ifndef WINDOWS_EMPTY_LINE_READING_ERROR_FIX
    return str;
#endif

    QString result = str.replace(QString("\n"), QString(" \n"));
    return result;
}


AccessibleTextEditorWidget::AccessibleTextEditorWidget(TextEditorWidget* widget)
#ifdef VIA_EDITOR_COMPONENT
    : QAccessibleWidget(widget->textEditorComponent(), QAccessible::EditableText)
#else
    : QAccessibleWidget(widget, QAccessible::EditableText)
#endif
    , textWidgetRef_(widget)
{
}

AccessibleTextEditorWidget::~AccessibleTextEditorWidget()
{
}

/// Construct the AccessibleTextEditor interface for the given widget
QAccessibleInterface *AccessibleTextEditorWidget::factory(const QString &className, QObject *object)
{
    // edbee::TextMarginComponent, edbee::TextEditorScrollArea, edbee::TextEditorComponent
#ifdef VIA_EDITOR_COMPONENT
    if (className == QLatin1String("edbee::TextEditorComponent") && object && object->isWidgetType()) {
        return new AccessibleTextEditorWidget(static_cast<edbee::TextEditorComponent *>(object)->controller()->widget());
    }
#else
    if (className == QLatin1String("edbee::TextEditorWidget") && object && object->isWidgetType()) {
        return new AccessibleTextEditorWidget(static_cast<edbee::TextEditorWidget *>(object));
    }

#endif

    return nullptr;
}

/// Returns the widget that should be used for accessibility events
QWidget* AccessibleTextEditorWidget::eventWidgetForTextEditor(TextEditorWidget* widget)
{
#ifdef VIA_EDITOR_COMPONENT
    return widget->textEditorComponent();
#else
    return widget;
#endif
}

/// Notifies a text-selection change event
void AccessibleTextEditorWidget::notifyTextSelectionEvent(TextEditorWidget *widget, TextSelection *selection)
{
    QWidget* eventWidget = eventWidgetForTextEditor(widget);
    for(int i=0, cnt = selection->rangeCount(); i < cnt; ++i) {
        TextRange range = selection->range(i);

        QAccessibleTextSelectionEvent ev(eventWidget, D2V(widget, range.min()), D2V(widget, range.max()));
        ev.setCursorPosition(D2V(widget, range.caret()));
        QAccessible::updateAccessibility(&ev);
    }
}

/// Notifies a text change event happens
void AccessibleTextEditorWidget::notifyTextChangeEvent(TextEditorWidget *widget, TextBufferChange *change)
{
    QWidget* eventWidget = eventWidgetForTextEditor(widget);

    // this is a bit tricky, a textbuffer change uses the newtext-value
    // for storing the old text. The new text can be found in the document
    QString oldText(change->newText());
    QString newText = widget->textDocument()->textPart(change->offset(), change->newTextLength());

    QAccessibleTextUpdateEvent ev(eventWidget, D2V(widget, change->offset()), VTEXT(oldText), VTEXT(newText));
    // TODO: When a caret is included, (Inherited change, use this caret position)
    QAccessible::updateAccessibility(&ev);
}


void *AccessibleTextEditorWidget::interface_cast(QAccessible::InterfaceType t)
{
    if (t == QAccessible::TextInterface) {
        return static_cast<QAccessibleTextInterface*>(this);
    }
    if (t == QAccessible::EditableTextInterface) {
        return static_cast<QAccessibleEditableTextInterface*>(this);
    }
    return QAccessibleWidget::interface_cast(t);
}


QAccessible::State AccessibleTextEditorWidget::state() const
{
    QAccessible::State s = QAccessibleWidget::state();
    s.selectableText = true;
//    s.multiSelectable = true;
//    s.extSelectable = true;
    s.multiLine = true;
    s.focusable = true;
//    s.marqueed = true; // The object displays scrolling contents, e.g. a log view.
    return s;
}


/// Returns a selection. The size of the selection is returned in startOffset and endOffset.
/// If there is no selection both startOffset and endOffset are nullptr.
///
/// The accessibility APIs support multiple selections. For most widgets though, only one selection
/// is supported with selectionIndex equal to 0.
void AccessibleTextEditorWidget::selection(int selectionIndex, int *startOffset, int *endOffset) const
{   
    if(selectionIndex >= textSelection()->rangeCount()) {
        *startOffset = 0;
        *endOffset = 0;
    }

    TextRange& range = textSelection()->range(selectionIndex);
    *startOffset = D2V(textWidget(), range.min());
    *endOffset = D2V(textWidget(), range.max());
    // qDebug() << " selection >> " << selectionIndex << ", " << range.min() << " =>" << *startOffset << ", " << range.max() << " => " << *endOffset;
}

/// Returns the number of selections in this text.
int AccessibleTextEditorWidget::selectionCount() const
{
    return textSelection()->rangeCount();
}

/// Select the text from startOffset to endOffset. The startOffset is the first character that will be selected.
/// The endOffset is the first character that will not be selected.
///
/// When the object supports multiple selections (e.g. in a word processor), this adds a new selection,
/// otherwise it replaces the previous selection.
///
/// The selection will be endOffset - startOffset characters long.
void AccessibleTextEditorWidget::addSelection(int startOffset, int endOffset)
{
    TextSelection selection = *textSelection();
    selection.addRange(V2D(textWidget(), startOffset), V2D(textWidget(), endOffset));
    controller()->changeAndGiveTextSelection(&selection);
}

/// Clears the selection with index selectionIndex.
void AccessibleTextEditorWidget::removeSelection(int selectionIndex)
{
    TextSelection selection = *textSelection();
    selection.removeRange(selectionIndex);
    controller()->changeAndGiveTextSelection(&selection);
}

/// Set the selection selectionIndex to the range from startOffset to endOffset.
void AccessibleTextEditorWidget::setSelection(int selectionIndex, int startOffset, int endOffset)
{
    TextSelection selection = *textSelection();
    selection.setRange(V2D(textWidget(), startOffset), V2D(textWidget(), endOffset), selectionIndex);
    controller()->changeAndGiveTextSelection(&selection);
}

/// Returns the current cursor position.
int AccessibleTextEditorWidget::cursorPosition() const
{
    int caret = D2V(textWidget(), textSelection()->range(0).caret());   
    // qDebug() << " cursorPosition() >> " << textSelection()->range(0).caret() << " => " << caret;
    return caret;
}

/// Move the cursor position
void AccessibleTextEditorWidget::setCursorPosition(int position)
{
//    qDebug() << " moveCaretToOffset() >> " << position;

    controller()->moveCaretToOffset(V2D(textWidget(), V2D(textWidget(), position)), false);
}

QString AccessibleTextEditorWidget::text(QAccessible::Text t) const
{
    if (t != QAccessible::Value) {
        return QAccessibleWidget::text(t);
    }
    return VTEXT(textWidget()->textDocument()->text());
}

/// Returns the text from startOffset to endOffset. The startOffset is the first character that will be returned.
/// The endOffset is the first character that will not be returned.
QString AccessibleTextEditorWidget::text(int vStartOffset, int vEndOffset) const
{
    int startOffset = V2D(textWidget(), vStartOffset, true);
    int endOffset = V2D(textWidget(), vEndOffset, true);

    QString str = textWidget()->textDocument()->textPart(startOffset, endOffset - startOffset);

    // return str.replace("\n\n", "\n \n");  /// << This seems to work.
    return VTEXT(str);
}

/// Returns the length of the text (total size including spaces).
int AccessibleTextEditorWidget::characterCount() const
{
    return VLEN(textWidget());
}


/// Returns the position and size of the character at position offset in screen coordinates.
QRect AccessibleTextEditorWidget::characterRect(int vOffset) const
{
    TextEditorComponent* comp = textWidget()->textEditorComponent();
    int offset = V2D(textWidget(), vOffset, true);

    // workaround for newline char rect (is at the wrong location)
#ifdef WINDOWS_EMPTY_LINE_READING_ERROR_FIX
    if(offset > 0 && offset < textDocument()->length()){
        if( textDocument()->charAt(offset) == '\n') {
            offset -= 1;
        }
    }
#endif

    int xPos = this->renderer()->xPosForOffset(offset);
    int yPos = this->renderer()->yPosForOffset(offset);
    QPoint point(xPos, yPos);
    QPoint pointScreen = comp->mapToGlobal(point);

    // qDebug() << " characterRect >>" << vOffset << " => " << offset << ": " << pointScreen;
    return QRect(pointScreen.x(), pointScreen.y(), renderer()->emWidth(), renderer()->lineHeight());
}


/// Returns the offset of the character at the point in screen coordinates.
int AccessibleTextEditorWidget::offsetAtPoint(const QPoint &point) const
{
    int line = renderer()->rawLineIndexForYpos(point.y());
    int col = renderer()->columnIndexForXpos(line, point.x());
    // qDebug() << " offsetAtPoint >>" << point << ": " << line << ", " << col;

   return D2V(textWidget(), textDocument()->offsetFromLineAndColumn(line, col));
}

/// Ensures that the text between startIndex and endIndex is visible.
void AccessibleTextEditorWidget::scrollToSubstring(int startIndex, int endIndex)
{
    // qDebug() << " scrollToSubstring >>" << startIndex << ", " << endIndex;
    controller()->scrollOffsetVisible(V2D(textWidget(), startIndex));
}


/// Returns the text attributes at the position offset.
/// In addition the range of the attributes is returned in startOffset and endOffset.
QString AccessibleTextEditorWidget::attributes(int offset, int *startOffset, int *endOffset) const
{
    // qDebug() << " attributes >>" << offset << ", " << *startOffset << ", " <<  *endOffset;
    return QString();
}

/// Returns the text item of type boundaryType that is right after offset offset and sets startOffset
/// and endOffset values to the start and end positions of that item; returns an empty string if there
/// is no such an item. Sets startOffset and endOffset values to -1 on error.
///
/// This default implementation is provided for small text edits. A word processor or text editor should
/// provide their own efficient implementations. This function makes no distinction between paragraphs and lines.
///
/// Note: this function can not take the cursor position into account. By convention an offset of -2 means
/// that this function should use the cursor position as offset. Thus an offset of -2 must be converted
/// to the cursor position before calling this function.
/// An offset of -1 is used for the text length and custom implementations of this function have to return the
/// result as if the length was passed in as offset.
QString AccessibleTextEditorWidget::textAfterOffset(int vOffset, QAccessible::TextBoundaryType boundaryType, int *startOffset, int *endOffset) const
{
    if(boundaryType == QAccessible::LineBoundary && vOffset >= 0) {
        int offset = V2D(textWidget(), vOffset);

        int line = textDocument()->lineFromOffset(offset);
        // qDebug() << " => " << textDocument()->line(line);
        QString str = VTEXT(textDocument()->line(line));
        *startOffset = D2V(textWidget(), textDocument()->offsetFromLine(line));
        *endOffset = D2V(textWidget(), textDocument()->offsetFromLine(line+1));

//        qDebug() << "textAtOffset: vOffset: " << vOffset << " => " << offset
//                 << ", startOffset: " << *startOffset
//                 << ", endOffset: " << *endOffset
//                 << ", boundaryType: " << boundaryType <<  " => " << VTEXT(textDocument()->line(line));

        return str;
    }

    QString str = QAccessibleTextInterface::textAfterOffset(vOffset, boundaryType, startOffset, endOffset);
    qDebug() << "textAfterOffset: vOffset: " << vOffset << ", boundaryType: " << boundaryType << " => " << str << " (" << *startOffset << ", " << *endOffset <<")";
    return str;
}


/// Returns the text item of type boundaryType at offset offset and sets startOffset and endOffset
/// values to the start and end positions of that item; returns an empty string if there is no such an item.
/// Sets startOffset and endOffset values to -1 on error.
///
/// This default implementation is provided for small text edits. A word processor or text editor should
/// provide their own efficient implementations. This function makes no distinction between paragraphs and lines.
///
/// Note: this function can not take the cursor position into account. By convention an offset of -2 means that
/// this function should use the cursor position as offset. Thus an offset of -2 must be converted to the cursor
/// position before calling this function. An offset of -1 is used for the text length and custom implementations
/// of this function have to return the result as if the length was passed in as offset.
QString AccessibleTextEditorWidget::textAtOffset(int vOffset, QAccessible::TextBoundaryType boundaryType, int *startOffset, int *endOffset) const
{
    //  QAccessible::CharBoundary	0	Use individual characters as boundary.
    //  QAccessible::WordBoundary	1	Use words as boundaries.
    //  QAccessible::SentenceBoundary	2	Use sentences as boundary.
    //  QAccessible::ParagraphBoundary	3	Use paragraphs as boundary.
    //  QAccessible::LineBoundary	4	Use newlines as boundary.
    //  QAccessible::NoBoundary	5	No boundary (use the whole text).

    if(boundaryType == QAccessible::LineBoundary && vOffset >= 0) {
        int offset = V2D(textWidget(), vOffset);
        int line = textDocument()->lineFromOffset(offset);

        QString str = VTEXT(textDocument()->line(line));
        *startOffset = D2V(textWidget(), textDocument()->offsetFromLine(line));
        *endOffset = D2V(textWidget(), textDocument()->offsetFromLine(line+1));

//        qDebug() << "textAtOffset: vOffset: " << vOffset << " => " << offset
//                 << ", startOffset: " << *startOffset
//                 << ", endOffset: " << *endOffset
//                 << ", boundaryType: " << boundaryType <<  " => " << VTEXT(textDocument()->line(line));
        return str;
    }

    QString str = QAccessibleTextInterface::textAtOffset(vOffset, boundaryType, startOffset, endOffset);
    qDebug() << "textAtOffset: offset: " << vOffset << ", boundaryType: " << boundaryType << " => " << str << " (" << *startOffset << ", " << *endOffset <<")";
    return str;
}

/// Returns the text item of type boundaryType that is close to offset offset and sets startOffset and endOffset values
/// to the start and end positions of that item; returns an empty string if there is no such an item.
/// Sets startOffset and endOffset values to -1 on error.
///
/// This default implementation is provided for small text edits. A word processor or text editor should provide their
///  own efficient implementations. This function makes no distinction between paragraphs and lines.
///
/// Note: this function can not take the cursor position into account. By convention an offset of -2 means that this
/// function should use the cursor position as offset. Thus an offset of -2 must be converted to the cursor position
/// before calling this function. An offset of -1 is used for the text length and custom implementations of this function
/// have to return the result as if the length was passed in as offset.
QString AccessibleTextEditorWidget::textBeforeOffset(int offset, QAccessible::TextBoundaryType boundaryType, int *startOffset, int *endOffset) const
{
    //qDebug() << "textBeforeOffset: offset: " << offset << ", boundaryType: " << boundaryType;
    return QAccessibleTextInterface::textBeforeOffset(offset, boundaryType, startOffset, endOffset);
}


/// Deletes the text from startOffset to endOffset.
void AccessibleTextEditorWidget::deleteText(int startOffset, int endOffset)
{
    // qDebug() << "?? deleteText >>" << startOffset << ", " << endOffset;
    controller()->replace(startOffset, endOffset - startOffset, QString(), 0);
}

/// Inserts text at position offset.
void AccessibleTextEditorWidget::insertText(int offset, const QString &text)
{
    // qDebug() << "?? insertText >>" << offset << ", " << text;
    controller()->replace(offset, 0, text, 0);
}

/// Removes the text from startOffset to endOffset and instead inserts text.
void AccessibleTextEditorWidget::replaceText(int startOffset, int endOffset, const QString &text)
{
    // qDebug() << "?? replaceText >>" << startOffset << ", " << endOffset << ", " << text;
    controller()->replace(startOffset, endOffset - startOffset, text, 0);
}

QAccessibleInterface *AccessibleTextEditorWidget::focusChild() const
{
    QAccessibleInterface* child = QAccessibleWidget::focusChild();
    return child;
}

/// Returns the rectangle for the editor widget
/// It returns the location of the textWidget (even when the TextComponent has got focus)
QRect AccessibleTextEditorWidget::rect() const
{
    QRect widgetRect = textWidget()->rect();
    QRect focusRect(textWidget()->mapToGlobal(widgetRect.topLeft()), widgetRect.size());
    return focusRect;
}

TextDocument* AccessibleTextEditorWidget::textDocument() const
{
    return textWidget()->textDocument();
}

TextSelection *AccessibleTextEditorWidget::textSelection() const
{
    return textWidget()->textSelection();
}

TextEditorController* AccessibleTextEditorWidget::controller() const
{
    return textWidget()->controller();
}

TextRenderer *AccessibleTextEditorWidget::renderer() const
{
    return textWidget()->textRenderer();
}

TextEditorWidget *AccessibleTextEditorWidget::textWidget() const
{
    return textWidgetRef_;
}


} // namespace Edbee
