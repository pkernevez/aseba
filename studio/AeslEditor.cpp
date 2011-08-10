/*
	Aseba - an event-based framework for distributed robot control
	Copyright (C) 2007--2011:
		Stephane Magnenat <stephane at magnenat dot net>
		(http://stephane.magnenat.net)
		and other contributors, see authors.txt for details
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published
	by the Free Software Foundation, version 3 of the License.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "AeslEditor.h"
#include "MainWindow.h"
#include "CustomWidgets.h"
#include <QtGui>

#include <AeslEditor.moc>

namespace Aseba
{
	/** \addtogroup studio */
	/*@{*/
	
	AeslHighlighter::AeslHighlighter(AeslEditor *editor, QTextDocument *parent) :
		QSyntaxHighlighter(parent),
		editor(editor)
	{
		HighlightingRule rule;
		
		// keywords
		QTextCharFormat keywordFormat;
		keywordFormat.setForeground(Qt::darkRed);
		QStringList keywordPatterns;
		keywordPatterns << "\\bemit\\b" << "\\bwhile\\b" << "\\bdo\\b"
						<< "\\bfor\\b" << "\\bin\\b" << "\\bstep\\b"
						<< "\\bif\\b" << "\\bthen\\b" << "\\belse\\b" << "\\belseif\\b"
						<< "\\bend\\b" << "\\bvar\\b" << "\\bcall\\b"
						<< "\\bonevent\\b" << "\\bontimer\\b" << "\\bwhen\\b"
						<< "\\band\\b" << "\\bor\\b" << "\\bnot\\b" 
						<< "\\bsub\\b" << "\\bcallsub\\b";
		foreach (QString pattern, keywordPatterns)
		{
			rule.pattern = QRegExp(pattern);
			rule.format = keywordFormat;
			highlightingRules.append(rule);
		}
		
		// literals
		QTextCharFormat literalsFormat;
		literalsFormat.setForeground(Qt::darkBlue);
		rule.pattern = QRegExp("\\b(-{0,1}\\d+|0x([0-9]|[a-f]|[A-F])+|0b[0-1]+)\\b");
		rule.format = literalsFormat;
		highlightingRules.append(rule);
		
		// comments #
		QTextCharFormat commentFormat;
		commentFormat.setForeground(Qt::gray);
		rule.pattern = QRegExp("[^\\*]{1}#(?!\\*).*");   // '#' without '*' right after or before
		rule.format = commentFormat;
		highlightingRules.append(rule);

		rule.pattern = QRegExp("^#(?!\\*).*");           // '#' without '*' right after + beginning of line
		rule.format = commentFormat;
		highlightingRules.append(rule);

		// comments #* ... *#
		rule.pattern = QRegExp("#\\*.*\\*#");
		rule.format = commentFormat;
		highlightingRules.append(rule);
		
		// multilines block of comments #* ... [\n]* ... *#
		commentBlockRules.begin = QRegExp("#\\*(?!.*\\*#)");    // '#*' with no corresponding '*#'
		commentBlockRules.end = QRegExp(".*\\*#");
		commentBlockRules.format = commentFormat;

		// todo/fixme
		QTextCharFormat todoFormat;
		todoFormat.setForeground(Qt::black);
		todoFormat.setBackground(QColor(255, 192, 192));
		rule.pattern = QRegExp("#.*(\\bTODO\\b|\\bFIXME\\b).*");
		rule.format = todoFormat;
		highlightingRules.append(rule);
	}
	
	void AeslHighlighter::highlightBlock(const QString &text)
	{
		AeslEditorUserData *uData = static_cast<AeslEditorUserData *>(currentBlockUserData());
		
		// current line background blue
		bool isActive = uData && uData->properties.contains("active");
		bool isExecutionError = uData && uData->properties.contains("executionError");
		bool isBreakpointPending = uData && uData->properties.contains("breakpointPending");
		bool isBreakpoint = uData && uData->properties.contains("breakpoint");
		
		QColor breakpointPendingColor(255, 240, 178);
		QColor breakpointColor(255, 211, 178);
		QColor activeColor(220, 220, 255);
		QColor errorColor(240, 100, 100);

		// use the QTextEdit::ExtraSelection class to highlight the background
		// of special lines (breakpoints, active line,...)
		QList<QTextEdit::ExtraSelection> extraSelections;
		if (currentBlock().blockNumber() != 0)
			// past the first line, we recall the previous "selections"
			extraSelections = editor->extraSelections();

		// prepare the current "selection"
		QTextEdit::ExtraSelection selection;
		selection.format.setProperty(QTextFormat::FullWidthSelection, true);
		selection.cursor = QTextCursor(currentBlock());

		if (isBreakpointPending)
			selection.format.setBackground(breakpointPendingColor);
		if (isBreakpoint)
			selection.format.setBackground(breakpointColor);
		if (editor->debugging)
		{
			if (isActive)
				selection.format.setBackground(activeColor);
			if (isExecutionError)
				selection.format.setBackground(errorColor);
		}
		
		// we are done
		extraSelections.append(selection);
		editor->setExtraSelections(extraSelections);

		// syntax highlight
		foreach (HighlightingRule rule, highlightingRules)
		{
			QRegExp expression(rule.pattern);
			int index = text.indexOf(expression);
			while (index >= 0)
			{
				int length = expression.matchedLength();
				QTextCharFormat format = rule.format;
				setFormat(index, length, format);
				index = text.indexOf(expression, index + length);
			}
		}
		

		// Prepare the format for multilines comment block
		int index;
		QTextCharFormat format = commentBlockRules.format;

		// Comply with the breakpoint formatting
		if (isBreakpointPending)
			format.setBackground(breakpointPendingColor);

		// Search for a multilines comment block
		setCurrentBlockState(NO_COMMENT);                // No comment
		if (previousBlockState() != COMMENT)
		{
			// Search for a beginning
			if ((index = text.indexOf(commentBlockRules.begin)) != -1)
			{
				// Found one
				setFormat(index, text.length() - index, format);
				setCurrentBlockState(COMMENT);
			}
		}
		else
		{
			// Search for an end
			if ((index = text.indexOf(commentBlockRules.end)) != -1)
			{
				// Found one
				int length = commentBlockRules.end.matchedLength();
				setFormat(0, length, format);
				setCurrentBlockState(NO_COMMENT);
			}
			else
			{
				// Still inside a block
				setFormat(0, text.length(), format);
				setCurrentBlockState(COMMENT);
			}
		}

		// error word in red
		if (uData && uData->properties.contains("errorPos"))
		{
			int pos = uData->properties["errorPos"].toInt();
			int len = 0;

			if (pos + len < text.length())
			{
				// find length of number or word
				while (pos + len < text.length())
					if (
						(!text[pos + len].isDigit()) &&
						(!text[pos + len].isLetter()) &&
						(text[pos + len] != '_') &&
						(text[pos + len] != '.')
					)
						break;
					else
						len++;
			}
			len = len > 0 ? len : 1;
			setFormat(pos, len, Qt::red);
		}
	}

	AeslEditorSidebar::AeslEditorSidebar(AeslEditor* editor) :
		QWidget(editor),
		editor(editor),
		currentSizeHint(0,0),
		verticalScroll(0)
	{
		setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
		connect(editor->verticalScrollBar(), SIGNAL(valueChanged(int)), SLOT(scroll(int)));
		connect(editor, SIGNAL(textChanged()), SLOT(update()));
	}

	void AeslEditorSidebar::scroll(int dy)
	{
		verticalScroll = dy;
		update();		// repaint
	}

	QSize AeslEditorSidebar::sizeHint() const
	{
		return QSize(idealWidth(), 0);
	}

	void AeslEditorSidebar::paintEvent(QPaintEvent *event)
	{
		QSize newSizeHint = sizeHint();

		if (currentSizeHint != newSizeHint)
		{
			// geometry has changed, recompute the layout based on sizeHint()
			updateGeometry();
			currentSizeHint = newSizeHint;
		}
	}

	int AeslEditorSidebar::posToLineNumber(int y)
	{

		// iterate over all text blocks
		QTextBlock block = editor->document()->firstBlock();
		int offset = editor->contentsRect().top();		// top margin

		while (block.isValid())
		{
			QRectF bounds = block.layout()->boundingRect();
			bounds.translate(0, offset + block.layout()->position().y() - verticalScroll);
			if (y > bounds.top() && y < bounds.bottom())
			{
				// this is it
				return block.blockNumber();
			}

			block = block.next();
		}

		// not found
		return -1;
	}

	AeslLineNumberSidebar::AeslLineNumberSidebar(AeslEditor *editor) :
		AeslEditorSidebar(editor)
	{
	}

	void AeslLineNumberSidebar::showLineNumbers(bool state)
	{
		setVisible(state);
	}

	void AeslLineNumberSidebar::paintEvent(QPaintEvent *event)
	{
		AeslEditorSidebar::paintEvent(event);

		// begin painting
		QPainter painter(this);

		// fill the background
		painter.fillRect(event->rect(), QColor(210, 210, 210));

		// get the editor's painting area
		QRect editorRect = editor->contentsRect();

		// enable clipping to match the vertical painting area of the editor
		painter.setClipRect(QRect(0, editorRect.top(), width(), editorRect.bottom()), Qt::ReplaceClip );
		painter.setClipping(true);

		// define the painting area for linenumbers (top / bottom are identic to the editor))
		QRect region = editorRect;
		region.setLeft(0);
		region.setRight(idealWidth());

		// get the first text block
		QTextBlock block = editor->document()->firstBlock();

		painter.setPen(Qt::darkGray);
		// iterate over all text blocks
		// FIXME: do clever painting
		while(block.isValid())
		{
			if (block.isVisible())
			{
				QString number = QString::number(block.blockNumber() + 1);
				// paint the line number
				int y = block.layout()->position().y() + region.top() - verticalScroll;
				painter.drawText(0, y, width(), fontMetrics().height(), Qt::AlignRight, number);
			}

			block = block.next();
		}
	}

	int AeslLineNumberSidebar::idealWidth() const
	{
		// This is based on the Qt code editor example
		// http://doc.qt.nokia.com/latest/widgets-codeeditor.html
		int digits = 1;
		int linenumber = editor->document()->blockCount();
		while (linenumber >= 10) {
			linenumber /= 10;
			digits++;
		}

		int space = 3 + fontMetrics().width(QLatin1Char('9')) * digits;

		return space;
	}

	AeslBreakpointSidebar::AeslBreakpointSidebar(AeslEditor *editor) :
		AeslEditorSidebar(editor),
		borderSize(1)
	{
		// define the breakpoint geometry, according to the font metrics
		QFontMetrics metrics = fontMetrics();
		int lineSpacing = metrics.lineSpacing();
		breakpoint = QRect(borderSize, borderSize, lineSpacing - 2*borderSize, lineSpacing - 2*borderSize);
	}

	void AeslBreakpointSidebar::paintEvent(QPaintEvent *event)
	{
		AeslEditorSidebar::paintEvent(event);

		// begin painting
		QPainter painter(this);

		// fill the background
		painter.fillRect(event->rect(), QColor(210, 210, 210));

		// get the editor's painting area
		QRect editorRect = editor->contentsRect();

		// enable clipping to match the vertical painting area of the editor
		painter.setClipRect(QRect(0, editorRect.top(), width(), editorRect.bottom()), Qt::ReplaceClip );
		painter.setClipping(true);

		// define the painting area for breakpoints (top / bottom are identic to the editor)
		QRect region = editorRect;
		region.setLeft(0);
		region.setRight(idealWidth());

		// get the first text block
		QTextBlock block = editor->document()->firstBlock();

		painter.setPen(Qt::red);
		painter.setBrush(Qt::red);
		// iterate over all text blocks
		// FIXME: do clever painting
		while(block.isValid())
		{
			if (block.isVisible())
			{
				// get the block data and check for a breakpoint
				if (editor->isBreakpoint(block))
				{
					// paint the breakpoint
					int y = block.layout()->position().y() + region.top() - verticalScroll;
					// FIXME: Hughly breakpoing design...
					painter.drawRect(breakpoint.translated(0, y));
				}

			}

			block = block.next();
		}
	}

	void AeslBreakpointSidebar::mousePressEvent(QMouseEvent *event)
	{
		// click in the breakpoint column
		int line = posToLineNumber(event->pos().y());
		editor->toggleBreakpoint(editor->document()->findBlockByNumber(line));
	}

	int AeslBreakpointSidebar::idealWidth() const
	{
		return breakpoint.width() + 2*borderSize;
	}

	AeslEditor::AeslEditor(const ScriptTab* tab) :
		tab(tab),
		debugging(false),
		dropSourceWidget(0)
	{
		QFont font;
		font.setFamily("");
		font.setStyleHint(QFont::TypeWriter);
		font.setFixedPitch(true);
		//font.setPointSize(10);
		setFont(font);
		setAcceptDrops(true);
		setAcceptRichText(false);
		setTabStopWidth( QFontMetrics(font).width(' ') * 4);
	}
	
	void AeslEditor::dropEvent(QDropEvent *event)
	{
		dropSourceWidget = event->source();
		QTextEdit::dropEvent(event);
		dropSourceWidget = 0;
		setFocus(Qt::MouseFocusReason);
	}
	
	void AeslEditor::insertFromMimeData ( const QMimeData * source )
	{
		QTextCursor cursor(this->textCursor());
		
		// check whether we are at the beginning of a line
		bool startOfLine(cursor.atBlockStart());
		const int posInBlock(cursor.position() - cursor.block().position());
		if (!startOfLine && posInBlock)
		{
			const QString startText(cursor.block().text().left(posInBlock));
			startOfLine = !startText.contains(QRegExp("\\S"));
		}
		
		// if beginning of a line and source widget is known, add some helper text
		if (dropSourceWidget && startOfLine)
		{
			const NodeTab* nodeTab(dynamic_cast<const NodeTab*>(tab));
			QString prefix(""); // before the text
			QString midfix(""); // between the text and the cursor
			QString postfix(""); // after the curser
			if (dropSourceWidget == nodeTab->vmFunctionsView)
			{
				// inserting function
				prefix = "call ";
				midfix = "(";
				// fill call from doc
				const TargetDescription *desc = nodeTab->vmFunctionsModel->descriptionRead;
				const std::wstring funcName = source->text().toStdWString();
				for (size_t i = 0; i < desc->nativeFunctions.size(); i++)
				{
					const TargetDescription::NativeFunction native(desc->nativeFunctions[i]);
					if (native.name == funcName)
					{
						for (size_t j = 0; j < native.parameters.size(); ++j)
						{
							postfix += QString::fromStdWString(native.parameters[j].name);
							if (j + 1 < native.parameters.size())
								postfix += ", ";
						}
						break;
					}
				}
				postfix += ")\n";
			}
			else if (dropSourceWidget == nodeTab->vmMemoryView)
			{
				midfix = " ";
			}
			else if (dropSourceWidget == nodeTab->vmLocalEvents)
			{
				// inserting local event
				prefix = "onevent ";
				midfix = "\n";
			}
			else if (dropSourceWidget == nodeTab->mainWindow->eventsDescriptionsView)
			{
				// inserting global event
				prefix = "onevent ";
				midfix = "\n";
			}
			
			cursor.beginEditBlock();
			cursor.insertText(prefix + source->text() + midfix);
			const int pos = cursor.position();
			cursor.insertText(postfix);
			cursor.setPosition(pos);
			cursor.endEditBlock();

			this->setTextCursor(cursor);
		}
		else
			cursor.insertText(source->text());
		
	}
	
	void AeslEditor::contextMenuEvent ( QContextMenuEvent * e )
	{
		// create menu
		QMenu *menu = createStandardContextMenu();
		if (!isReadOnly())
		{
			QAction *breakpointAction;
			menu->addSeparator();
			
			// check for breakpoint
			QTextBlock block = cursorForPosition(e->pos()).block();
			//AeslEditorUserData *uData = static_cast<AeslEditorUserData *>(block.userData());
			bool breakpointPresent = isBreakpoint(block);
			
			// add action
			if (breakpointPresent)
				breakpointAction = menu->addAction(tr("Clear breakpoint"));
			else
				breakpointAction = menu->addAction(tr("Set breakpoint"));
			QAction *breakpointClearAllAction = menu->addAction(tr("Clear all breakpoints"));
			
			// execute menu
			QAction* selectedAction = menu->exec(e->globalPos());
			
			// do actions
			if (selectedAction == breakpointAction)
			{
				// modify editor state
				if (breakpointPresent)
				{
					// clear breakpoint
					clearBreakpoint(block);
				}
				else
				{
					// set breakpoint
					setBreakpoint(block);
				}
			}
			if (selectedAction == breakpointClearAllAction)
			{
				clearAllBreakpoints();
			}
		}
		else
			menu->exec(e->globalPos());
		delete menu;
	}

	bool AeslEditor::isBreakpoint()
	{
		return isBreakpoint(textCursor().block());
	}

	bool AeslEditor::isBreakpoint(QTextBlock block)
	{
		AeslEditorUserData *uData = static_cast<AeslEditorUserData *>(block.userData());
		return (uData && (uData->properties.contains("breakpoint") || uData->properties.contains("breakpointPending") )) ;
	}

	bool AeslEditor::isBreakpoint(int line)
	{
		QTextBlock block = document()->findBlockByNumber(line);
		return isBreakpoint(block);
	}

	void AeslEditor::toggleBreakpoint()
	{
		toggleBreakpoint(textCursor().block());
	}

	void AeslEditor::toggleBreakpoint(QTextBlock block)
	{
		if (isBreakpoint(block))
			clearBreakpoint(block);
		else
			setBreakpoint(block);
	}

	void AeslEditor::setBreakpoint()
	{
		setBreakpoint(textCursor().block());
	}

	void AeslEditor::setBreakpoint(QTextBlock block)
	{
		AeslEditorUserData *uData = static_cast<AeslEditorUserData *>(block.userData());
		if (!uData)
		{
			// create user data
			uData = new AeslEditorUserData("breakpointPending");
			block.setUserData(uData);
		}
		else
			uData->properties.insert("breakpointPending", QVariant());
		emit breakpointSet(block.blockNumber());
	}

	void AeslEditor::clearBreakpoint()
	{
		clearBreakpoint(textCursor().block());
	}

	void AeslEditor::clearBreakpoint(QTextBlock block)
	{
		AeslEditorUserData *uData = static_cast<AeslEditorUserData *>(block.userData());
		uData->properties.remove("breakpointPending");
		uData->properties.remove("breakpoint");
		if (uData->properties.isEmpty())
		{
			// garbage collect UserData
			block.setUserData(0);
		}
		emit breakpointCleared(block.blockNumber());
	}

	void AeslEditor::clearAllBreakpoints()
	{
		for (QTextBlock it = document()->begin(); it != document()->end(); it = it.next())
		{
			AeslEditorUserData *uData = static_cast<AeslEditorUserData *>(it.userData());
			if (uData)
			{
				uData->properties.remove("breakpoint");
				uData->properties.remove("breakpointPending");
			}
		}
		emit breakpointClearedAll();
	}

	void AeslEditor::commentAndUncommentSelection(CommentOperation commentOperation)
	{
		QTextCursor cursor = textCursor();
		bool moveFailed = false;

		// get the last line of the selection
		QTextBlock endBlock = document()->findBlock(cursor.selectionEnd());
		int lineEnd = endBlock.blockNumber();
		int positionInEndBlock = cursor.selectionEnd() - endBlock.position();

		// if the end of the selection is at the begining of a line,
		// this last line should not be taken into account
		if (cursor.hasSelection() && (positionInEndBlock == 0))
			lineEnd--;

		// start at the begining of the selection
		cursor.setPosition(cursor.selectionStart());
		cursor.movePosition(QTextCursor::StartOfBlock);
		QTextCursor cursorRestore = cursor;

		while (cursor.block().blockNumber() <= lineEnd)
		{
			// prepare for insertion / deletion
			setTextCursor(cursor);

			if (commentOperation == CommentSelection)
			{
				// insert #
				cursor.insertText("#");
			}
			else if (commentOperation == UncommentSelection)
			{
				// delete #
				if (cursor.block().text().at(0) == QChar('#'))
					cursor.deleteChar();
			}

			// move to the next line
			if (!cursor.movePosition(QTextCursor::NextBlock))
			{
				// end of the document
				moveFailed = true;
				break;
			}
		}

		// select the changed lines
		cursorRestore.movePosition(QTextCursor::StartOfBlock);
		if (!moveFailed)
			cursor.movePosition(QTextCursor::StartOfBlock);
		else
			cursor.movePosition(QTextCursor::EndOfBlock);
		cursorRestore.setPosition(cursor.position(), QTextCursor::KeepAnchor);
		setTextCursor(cursorRestore);
	}

	void AeslEditor::keyPressEvent(QKeyEvent * event)
	{
		// handle tab and control tab
		if ((event->key() == Qt::Key_Tab) && textCursor().hasSelection())
		{
			QTextCursor cursor(document()->findBlock(textCursor().selectionStart()));
			
			cursor.beginEditBlock();
			
			while (cursor.position() < textCursor().selectionEnd())
			{
				cursor.movePosition(QTextCursor::StartOfLine);
				if (event->modifiers() & Qt::ControlModifier)
				{
					cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
					if ((cursor.selectedText() == "\t") ||
						(	(cursor.selectedText() == " ") &&
							(cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 3)) &&
							(cursor.selectedText() == "    ")
						)
					)
						cursor.removeSelectedText();
				}
				else
					cursor.insertText("\t");
				cursor.movePosition(QTextCursor::Down);
				cursor.movePosition(QTextCursor::EndOfLine);
			}
			
			cursor.movePosition(QTextCursor::StartOfLine);
			if (event->modifiers() & Qt::ControlModifier)
			{
				cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
				if ((cursor.selectedText() == "\t") ||
					(	(cursor.selectedText() == " ") &&
						(cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 3)) &&
						(cursor.selectedText() == "    ")
					)
				)
					cursor.removeSelectedText();
			}
			else
				cursor.insertText("\t");
				
			cursor.endEditBlock();
			return;
		}
		// handle indentation and new line
		else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
		{
			QString headingSpace("\n");
			const QString &line = textCursor().block().text();
			for (size_t i = 0; i < (size_t)line.length(); i++)
			{
				const QChar c(line[(unsigned)i]);
				if (c.isSpace())
					headingSpace += c;
				else
					break;
					
			}
			insertPlainText(headingSpace);
		}
		else
			QTextEdit::keyPressEvent(event);
		
	}
	
	/*@}*/
}; // Aseba
