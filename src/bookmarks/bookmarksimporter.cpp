/*
 * Copyright 2010 Jakub Zelenka <jakub.zelenka@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <qfile.h>
#include <qchar.h>
#include <qtextcodec.h>

#include "bookmarksimporter.h"
#include "bookmarknode.h"
#include "xbelreader.h"


// DEVICE

BookmarksDevice::BookmarksDevice(QIODevice *device, FileType type)
{
    m_device = device;
    m_line = m_column = 1;
    m_data = device->read(BUFF_SIZE);
    if (type == HTML)
        m_codec = QTextCodec::codecForHtml(m_data);
    else
        m_codec = QTextCodec::codecForName("UTF-8");

    m_utf = m_codec->name().contains("UTF");
    makeString();
}

int BookmarksDevice::line() const
{
    return m_line;
}

int BookmarksDevice::column() const
{
    return m_column;
}

bool BookmarksDevice::getChar(QChar &ch)
{
    if (m_pos == m_str.size()) {
        m_data = m_device->read(BUFF_SIZE);
        if (!makeString())
            return false;
    }

    ch = m_str[m_pos++];
    if (ch.category() == QChar::Separator_Line) {
        m_column = 1;
        m_line++;
    } else
        m_column++;

    return true;
}

bool BookmarksDevice::makeString()
{
    if (m_data.isEmpty())
        return false;

    if (m_utf) {
        char ch;
        // fixed division one utf character between two buffers
        while (static_cast<int>(m_data[m_data.size() - 1]) < 0 && m_device->getChar(&ch))
            m_data.append(ch);
    }

    m_pos = 0;
    m_str = m_codec->toUnicode(m_data);
    return true;
}

// TOKEN

BookmarkHTMLToken::BookmarkHTMLToken(BookmarksDevice *device)
{
    m_device = device;
    m_error = false;

    if (m_device->getChar(m_char)) {
        m_last = false;
        readNext();
    } else {
        m_last = true;
        m_type = Empty;
    }
}

bool BookmarkHTMLToken::readNext()
{
    m_type = Empty;
    if (m_last)
        return true;

    Tag tag;
    do {
        tag = readTag();
        if (m_error)
            return false;
    } while (tag.comment);

    if (tag.test("META")) {
        m_type = Meta;
    } else if (tag.test("TITLE")) {
        m_type = Title;
        m_content = readContent();
        tag = readTag();
        if (!tag.test("TITLE", true))
            m_error = true;
    } else if (tag.test("H1")) {
        m_type = Header;
        m_content = readContent();
        tag = readTag();
        if (!tag.test("H1", true))
            m_error = true;
    } else if (tag.test("HR")) {
        m_type = Separator;
    } else if (tag.name == QLatin1String("DL")) {
        if (tag.end)
            m_type = ListEnd;
        else
            m_type = ListStart;
    } else if (tag.test("P")) {
        m_type = Paragraph;
    } else if (tag.test("DT")) {
        tag = readTag();
        if (tag.test("H3")) {
            m_type = Folder;
            m_content = readContent();
            tag = readTag();
            if (!tag.test("H3", true))
                m_error = true;
        } else if (tag.test("A")) {
            m_type = Bookmark;
            m_content = readContent();
            tag = readTag();
            if (!tag.test("A", true))
                m_error = true;
        }
        else
            m_error = true;
    } else if (tag.test("DD")) {
        m_type = Description;
        m_content = readContent();
    } else
        m_error = true;

    if (m_error)
        m_last = true;

    return !m_last;
}

bool BookmarkHTMLToken::Tag::test(const char *str, bool isEnd) const
{
    return name == QLatin1String(str) && isEnd == end && !error;
}

bool BookmarkHTMLToken::last() const
{
    return m_last;
}

bool BookmarkHTMLToken::error() const
{
    return m_error;
}

BookmarkHTMLToken::Type BookmarkHTMLToken::type() const
{
    return m_type;
}

QString BookmarkHTMLToken::content() const
{
    return m_content;
}

QString BookmarkHTMLToken::attr(const QString &key) const
{
    if (m_attributes.contains(key))
        return m_attributes[key];
    else
        return QString();
}

BookmarkHTMLToken::Tag BookmarkHTMLToken::readTag(bool saveAttributes)
{
    Tag tag;
    tag.error = tag.end = tag.comment = false;

    if (!skipBlanks())
        return tag;

    if (!cmpNext('<')) {
        m_error = true;
        return tag;
    }

    if (m_char == QLatin1Char('!')) { // comment
        tag.comment = true;
        do { // skip
            if (!m_device->getChar(m_char)) {
                m_error = true;
                return tag;
            }
        } while (m_char != QLatin1Char('>'));
    } else {
        if (m_char == QLatin1Char('/')) {
            tag.end = true;
            if (!m_device->getChar(m_char))
                m_error = true;
        }

        tag.name = readIdent();
        if (m_error)
            return tag;

        if (tag.end) {
            skipBlanks();
            if (m_char != QLatin1Char('>'))
                m_error = true;
        } else {
            if (!readAttributes(saveAttributes))
                m_error = true;
        }
    }

    if (!m_error)
        m_last = (!m_device->getChar(m_char) || !skipBlanks());
    else
        tag.error = true;

    return tag;
}

QString BookmarkHTMLToken::readIdent()
{
    QString str;

    skipBlanks();
    while (m_char.isLetterOrNumber() || m_char == QLatin1Char('-') || m_char == QLatin1Char('_')) {
        str.append(m_char);
        if (!m_device->getChar(m_char)) {
            m_error = true;
            return QString();
        }
    }
    return str.toUpper();
}

QString BookmarkHTMLToken::readContent(QChar endChar)
{
    QString str;
    do {
        if (m_char == endChar)
            break;
        str.append(m_char);

        if (!m_device->getChar(m_char)) {
            m_error = true;
            return QString();
        }
    } while (true);

    // replacing entities
    QString content;
    QRegExp rx(QLatin1String("&([^;]{2,8});"));
    int pos = 0;
    int left = 0;
    while ((pos = rx.indexIn(str, pos)) != -1) {
        QString repl;
        QString entity = rx.cap(1);
        if (entity.startsWith(QLatin1Char('#'))) {
            bool ok;
            int charCode = entity.mid(1).toInt(&ok, 10);
            if (ok) {
                repl = QChar(charCode);
            } else
                repl = QString(QLatin1String("&%1;")).arg(entity);
        } else if (entity == QLatin1String("amp")) {
            repl = QLatin1Char('&');
        } else if (entity == QLatin1String("lt")) {
            repl = QLatin1Char('<');
        } else if (entity == QLatin1String("gt")) {
            repl = QLatin1Char('>');
        } else if (entity == QLatin1String("quot")) {
            repl = QLatin1Char('"');
        } else if (entity == QLatin1String("amp")) {
            repl = QString(QLatin1String("&%1;")).arg(entity);
        }
        content.append(str.mid(left, pos - left) + repl);
        pos += rx.matchedLength();
        left = pos;
    }
    content.append(str.mid(left));

    return content;
}

bool BookmarkHTMLToken::readAttributes(bool save)
{
    while (true) {
        if (!skipBlanks())
            return false;
        if (m_char == QLatin1Char('>'))
            return true;

        QString name = readIdent();
        if (m_error || !skipBlanks())
            return false;

        QString value;
        if (m_char == QLatin1Char('=')) {
            if (!m_device->getChar(m_char) || !skipBlanks() || !cmpNext('"'))
                return false;
            
            value = readContent(QLatin1Char('"'));
            if (m_error || !m_device->getChar(m_char))
                return false;
        }
        else
            value = QLatin1String("yes");
        
        if (save)
            m_attributes[name.toUpper()] = value;
    }
}

bool BookmarkHTMLToken::skipBlanks()
{
    if (!m_char.isSpace())
        return true;
    while (m_device->getChar(m_char)) {
        if (!m_char.isSpace())
            return true;
    }
    return false;
}

bool BookmarkHTMLToken::cmpNext(char ch)
{
    return m_char == QLatin1Char(ch) && m_device->getChar(m_char);
}


// IMPORTER

BookmarksImporter::BookmarksImporter(const QString &fileName)
{
    m_root = new BookmarkNode(BookmarkNode::Root);
    m_error = false;
    m_errorLine = m_errorColumn = 0;

    QFile file(fileName);
    if (!file.exists())
        return;
    file.open(QFile::ReadOnly);
    m_device = new BookmarksDevice(&file);

    if (fileName.endsWith(QLatin1String(".html"))) {
        parseHTML();
    } else if (fileName.endsWith(QLatin1String(".adr"))) {
        parseADR();
    } else {
        XbelReader reader;
        m_root = reader.read(&file);
        if (reader.error() != QXmlStreamReader::NoError) {
            m_error = true;
            m_errorString = reader.errorString();
            m_errorLine = reader.lineNumber();
            m_errorColumn = reader.columnNumber();
        }
    }

    if (m_error) {
        delete m_root;
        m_root = 0;
    }
    delete m_device;
}

bool BookmarksImporter::error() const
{
    return m_error;
}

int BookmarksImporter::errorLine() const
{
    return m_errorLine;
}

int BookmarksImporter::errorColumn() const
{
    return m_errorColumn;
}

QString BookmarksImporter::errorString() const
{
    return m_errorString;
}

BookmarkNode *BookmarksImporter::rootNode() const
{
    return m_root;
}

void BookmarksImporter::parseHTML()
{
    m_HTMLToken = new BookmarkHTMLToken(m_device);
    if (m_HTMLToken->type() == BookmarkHTMLToken::Empty) {
        if (m_HTMLToken->error())
            m_error = true;
        return;
    }
    m_error = false;

    if (m_HTMLToken->type() == BookmarkHTMLToken::Meta)
        m_HTMLToken->readNext();
    if (m_HTMLToken->type() == BookmarkHTMLToken::Title)
        m_HTMLToken->readNext();
    if (m_HTMLToken->type() == BookmarkHTMLToken::Header)
        m_HTMLToken->readNext();

    if (m_HTMLToken->type() == BookmarkHTMLToken::ListStart)
        parseHTMLFolder(m_root);
    else if (m_HTMLToken->type() != BookmarkHTMLToken::Empty)
        m_error = true;

    delete m_HTMLToken;
}

void BookmarksImporter::parseHTMLFolder(BookmarkNode *parent)
{
    m_HTMLToken->readNext();
    if (m_HTMLToken->type() == BookmarkHTMLToken::Paragraph)
        m_HTMLToken->readNext();

    while (!m_HTMLToken->error() && m_HTMLToken->type() != BookmarkHTMLToken::ListEnd) {
        if (m_HTMLToken->type() == BookmarkHTMLToken::Separator) {
            new BookmarkNode(BookmarkNode::Separator, parent);
            m_HTMLToken->readNext();
        } else if (m_HTMLToken->type() == BookmarkHTMLToken::Bookmark) {
            BookmarkNode *bookmark = new BookmarkNode(BookmarkNode::Bookmark, parent);
            bookmark->title = m_HTMLToken->content();
            if (bookmark->title.isEmpty())
                bookmark->title = tr("Unknown title");
            bookmark->url = m_HTMLToken->attr(QLatin1String("HREF"));
            m_HTMLToken->readNext();
            if (m_HTMLToken->type() == BookmarkHTMLToken::Description) {
                bookmark->desc = m_HTMLToken->content();
                m_HTMLToken->readNext();
            }
        } else if (m_HTMLToken->type() == BookmarkHTMLToken::Folder) {
            BookmarkNode *folder = new BookmarkNode(BookmarkNode::Folder, parent);
            folder->title = m_HTMLToken->content();
            folder->expanded = m_HTMLToken->attr(QLatin1String("FOLDED")).toUpper() == QLatin1String("NO");

            m_HTMLToken->readNext();
            if (m_HTMLToken->type() == BookmarkHTMLToken::Description) {
                folder->desc = m_HTMLToken->content();
                m_HTMLToken->readNext();
            }
            if (m_HTMLToken->type() == BookmarkHTMLToken::ListStart) {
                parseHTMLFolder(folder);
                m_HTMLToken->readNext();
                if (m_HTMLToken->type() == BookmarkHTMLToken::Paragraph)
                    m_HTMLToken->readNext();
            }
        } else {
            m_error = true;
            break;
        }
    }

    if (m_HTMLToken->error())
        m_error = true;

    if (m_error) {
        m_errorString = tr("Invalid syntax in HTML file");
        m_errorLine = m_device->line();
        m_errorColumn = m_device->column();
    }
}

void BookmarksImporter::parseADR()
{

}
