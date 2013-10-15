/**
 * Copyright 2011-2013 - Reliable Bits Software by Blommers IT. All Rights Reserved.
 * Author Rick Blommers
 */

#include "textdocument.h"

#include <QStringList>

#include "edbee/models/changes/complextextchange.h"
#include "edbee/models/changes/linedatatextchange.h"
#include "edbee/models/changes/singletextchange.h"
#include "edbee/models/changes/singletextchangewithcaret.h"

#include "edbee/models/textlinedata.h"
#include "edbee/models/textdocumentfilter.h"
#include "edbee/models/textrange.h"
#include "edbee/models/textundostack.h"

#include "debug.h"

namespace edbee {


/// Constructs the textdocument
TextDocument::TextDocument( QObject* obj )
    : QObject(obj)
    , documentFilter_(0)
    , documentFilterRef_(0)
{
}


/// Destroys the textdocument
TextDocument::~TextDocument()
{
    delete documentFilter_;
}


/// This method can be used to change the number of reserved fields by the document
/// Increasing the amount will result in a realoc
/// Decreasting the fieldcount reults in the lost of the 'old' fields
/// At least the 'PredefinedFieldCount' amont of fields are required
/// This method EMPTIES the undo-stack. So after this call all undo history is gone!
void TextDocument::setLineDataFieldsPerLine( int count )
{
    Q_ASSERT( count >= PredefinedFieldCount );
    lineDataManager()->setFieldsPerLine( count );
    textUndoStack()->clear();
}


/// This method gives a given data item to a text line
void TextDocument::giveLineData(int line, int field, TextLineData* dataItem)
{
    Q_ASSERT(line < lineCount() );
    LineDataTextChange* change = new LineDataTextChange( line, field );
    change->giveLineData( dataItem );
    executeAndGiveChange( change, true );
}


/// Returns the line specific data at the given line
/// @param line the line number to retrieve the line data for
/// @param field the field to retrieve the data for
/// @return TextLineData the associated line data
TextLineData* TextDocument::getLineData(int line, int field)
{
    int len = lineDataManager()->length();
    Q_ASSERT( len == lineCount() );
    Q_ASSERT( line < len );
    return lineDataManager()->get( line, field );
}


/// Starts an undo group
/// @param group the textchange group that groups the undo operations
void TextDocument::beginUndoGroup(TextChangeGroup* group)
{
//    if( documentFilter() ) {
//        documentFilter()->filterBeginGroup( this, group );
//    }
    textUndoStack()->beginUndoGroup(group);
}


/// Ends the current undo group
/// @param coalesceId the coalesceId
/// @param flatten should the operation be flatten (flattens undo-group trees)
void TextDocument::endUndoGroup( int coalesceId, bool flatten)
{
//    if( documentFilter() ) {
//        TextChangeGroup* group = textUndoStack()->currentGroup();
//        documentFilter()->filterEndGroup( this, group, coalesceId, flatten );
//    }
    textUndoStack()->endUndoGroup(coalesceId,flatten);
}


/// Ends the undo group and discards all recorded information
/// Warning it doesn NOT undo all made changes!!!
void TextDocument::endUndoGroupAndDiscard()
{
    textUndoStack()->endUndoGroupAndDiscard();
}


/// this method return true if the undo stack is enabled
bool TextDocument::isUndoCollectionEnabled()
{
    return textUndoStack()->isCollectionEnabled();
}


/// Enables or disables the collection of undo commands
void TextDocument::setUndoCollectionEnabled(bool enabled)
{
    textUndoStack()->setCollectionEnabled(enabled);
}


/// This method should return true if the current change is the cause of an undo operation
bool TextDocument::isUndoRunning()
{
    return textUndoStack()->isUndoRunning();
}


/// Checks if currently an undo operation is running
bool TextDocument::isRedoRunning()
{
    return textUndoStack()->isRedoRunning();
}


/// Is it an undo or redo (which means all commands area already available)
bool TextDocument::isUndoOrRedoRunning()
{
    return isUndoRunning() || isRedoRunning();
}


/// Checks if the document is in a persited state
bool TextDocument::isPersisted()
{
    return textUndoStack()->isPersisted();
}


/// Calc this method to mark current state as persisted
void TextDocument::setPersisted(bool enabled)
{
    textUndoStack()->setPersisted(enabled);
}


/// Sets the document filter without tranfering the ownership
void TextDocument::setDocumentFilter(TextDocumentFilter* filter)
{
    delete documentFilter_;
    documentFilter_  = 0;
    documentFilterRef_ = filter;
}


/// this method sets the document filter
/// You can give a 0 pointer to delte the old filter!
void TextDocument::giveDocumentFilter(TextDocumentFilter* filter)
{
    delete documentFilter_;
    documentFilter_ = filter;
    documentFilterRef_ = filter;
}


/// This method returns the document filter
TextDocumentFilter* TextDocument::documentFilter()
{
    return documentFilterRef_;
}


/// replaces the given range-set with the given string
/// Warning when a documentfilter is installed it is possible the rangeSet is modified!!
/// @param rangeSet the rangeSet to replace
/// @param text the text to place at the given ranges
/// @param coalesceId the coalesceid
void TextDocument::replaceRangeSet(TextRangeSet& rangeSet, const QString& textIn, int coalesceId, TextEditorController* controller)
{
    return replaceRangeSet( rangeSet, QStringList(textIn), coalesceId, controller );
}


/// A special replace rangeset. This replace-rangeset can have a text per range
/// The number of items in the texts array should be the same as the number of ranges in the rangeset. If this is not
/// the text rotates and starts at the beginning
/// @param rangeSet the range to replace
/// @param texts the list of texts to paste in the rangeset.
/// @param coalesceId the coalesceId of the operation
/// @param controller the controller of the opration
void TextDocument::replaceRangeSet(TextRangeSet& rangeSet, const QStringList& textsIn, int coalesceId, TextEditorController* controller)
{

    beginUndoGroup( new ComplexTextChange( controller) );

    QStringList texts = textsIn;
    if( documentFilter() ) {
        documentFilter()->filterReplaceRangeSet( this, rangeSet, texts );
    }

    rangeSet.beginChanges();

    int idx = 0, oldRangeCount = 0;
    while( idx < (oldRangeCount = rangeSet.rangeCount())  ) {
        TextRange& range = rangeSet.range(idx);
        QString text = texts.at(idx%texts.size());  // rotating text-fetching

        SingleTextChangeWithCaret* change = new SingleTextChangeWithCaret(range.min(),range.length(),text,-1);

        // this can be filtered
        executeAndGiveChange( change, false );

        // so we need to adjust the caret with the (possible) adjusted change
        if( change->caret() < 0 ) {
            range.setCaret( change->offset() + change->length() );
        } else {
            range.setCaret( change->caret() );
        }
        range.reset();

        // next range.
        if( rangeSet.rangeCount() < oldRangeCount ) {
qlog_info() <<  "TEST TO SEE IF THIS REALLY HAPPENS!! I think it cannot happen. (but I'm not sure)";
Q_ASSERT(false);

        // else we stay at the same location
        } else {
            ++idx;
        }
    }

    rangeSet.endChanges();

    endUndoGroup(coalesceId,true);

}


/* Temporary disabled
void TextDocument::replace(int offset, int length, const QString& newText, bool merge )
{
    textUndoStack()->beginUndoGroup(0);  // we need to start a new undo-group, because line-data-changes are added seperately

    TextChange* change = new SingleTextChange( offset, length, newText );
    executeAndGiveChange( change, merge );

    textUndoStack()->endUndoGroup(0,true);
}
*/


/// call this method to execute a change. The change is first passed to the filter
/// so the documentFilter can handle the processing of the change
/// When not filter is active the 'execute' method is called on the change
void TextDocument::executeAndGiveChange(TextChange* change, int coalesceId )
{
    if( documentFilter() ) {
        documentFilter()->filterChange( this, change, coalesceId );
    } else {
        change->execute( this );
        giveChangeWithoutFilter( change, coalesceId );
    }
}


/// Appends the given text to the document
/// @param text the text to append
/// @param coalesceId (default 0) the coalesceId to use. Whe using the same number changes could be merged to one change. CoalesceId of 0 means no merging
void TextDocument::append(const QString& text, int coalesceId )
{
    replace( this->length(), 0, text, coalesceId );
}


/// Appends the given text
/// @param text the text to append
/// @param coalesceId (default 0) the coalesceId to use. Whe using the same number changes could be merged to one change. CoalesceId of 0 means no merging
void TextDocument::replace( int offset, int length, const QString& text, int coalesceId )
{
    executeAndGiveChange( new SingleTextChange( offset, length, text ), coalesceId );
}


/// Changes the compelte document text
/// @param text the new document text
void TextDocument::setText(const QString& text)
{
    replace( 0, length(), text, 0 );
}


/// begins the raw append modes. In raw append mode data is directly streamed
/// to the textdocument-buffer. No undo-data is collected and no events are fired
void TextDocument::rawAppendBegin()
{
    setUndoCollectionEnabled(false); // no undo's
    buffer()->rawAppendBegin();
}


/// When then raw appending is done. The events are fired that the document has been changed
/// The undo-collection is enabled again
void TextDocument::rawAppendEnd()
{
    buffer()->rawAppendEnd();
    setUndoCollectionEnabled(true);
}


/// Appends a single char in raw append mode
void TextDocument::rawAppend(QChar c)
{
    buffer()->rawAppend(c);
}


/// Appends an array of characters
void TextDocument::rawAppend(const QChar* chars, int length)
{
    buffer()->rawAppend(chars,length);
}


/// This method executes the given 'multi-text-change'
//void TextDocument::giveChange(TextChange *change, bool merge)
//{
//    if( documentFilter() ) {
//        documentFilter()->filterChange( this, change, merge );
//    } else {
//        giveChangeWithoutFilter( change, merge );
//    }
//}


/// Returns the length of the document in characters
/// default implementation is to forward this call to the textbuffer
int TextDocument::length()
{
    return buffer()->length();
}


/// Returns the number of lines
int TextDocument::lineCount()
{
    return buffer()->lineCount();
}


/// Returns the character at the given position
QChar TextDocument::charAt(int idx)
{
    return buffer()->charAt(idx);
}


/// Retrieves the character-offset of the given line
/// @param line the line number (0-based) to retrieve the offset for
/// @return the character offset
int TextDocument::offsetFromLine(int line)
{
    return buffer()->offsetFromLine(line);
}


/// returns the line number which contains the given offset
/// @param offset the character offset
/// @return the line number (0 is the first line )
int TextDocument::lineFromOffset(int offset)
{
    return buffer()->lineFromOffset(offset);
}


/// return the column position for the given offset and line
/// @param offset the offset position
/// @param line the line number which contains this offset. (When -1 the line number is calculated)
/// @return the column position of the given offset
int TextDocument::columnFromOffsetAndLine(int offset, int line)
{
    return buffer()->columnFromOffsetAndLine(offset,line);
}


/// Returns the character offset of the given line and column
/// @param line the line number
/// @param column the column position
/// @return the character offset in the document
int TextDocument::offsetFromLineAndColumn(int line, int column)
{
    return buffer()->offsetFromLineAndColumn(line,column);
}


/// Returns the length of the given line
/// @param line the line number
/// @return the line length
int TextDocument::lineLength(int line)
{
    return buffer()->lineLength(line);
}


/// Returns the document text as a QString
/// @return the complete document context
QString TextDocument::text()
{
    return buffer()->text();
}


/// Returns the given part of the text
/// @param offset the character offset in the document
/// @param length the length of the part in characters
/// @return the text at the given positions
QString TextDocument::textPart(int offset, int length)
{
    return buffer()->textPart( offset, length );
}


/// Returns the given line without the trailing \n character
/// @param line the line number to retrieve the data for
/// @return the content at the given line
QString TextDocument::lineWithoutNewline(int line)
{
    return buffer()->lineWithoutNewline(line);
}


//// Returns the contents at the given line inclusive the trailing \n character
/// @pparam line the line number to retrieve
/// @return the line at the given position
QString TextDocument::line(int line)
{
    return buffer()->line(line);
}


} // edbee