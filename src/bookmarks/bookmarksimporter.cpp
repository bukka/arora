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

#include "bookmarksimporter.h"
#include "bookmarknode.h"
#include "xbelreader.h"

// parse one commment
BookmarkHTMLToken::BookmarkHTMLToken(QIODevice *device)
{
    m_device = device;
    m_type = Empty;
    m_error = false;

    Tag tag;
    do {
        tag = readTag();
        if (m_error || tag.name.isEmpty())
            return;
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
            m_type = ListBegin;
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
            m_type = Link;
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

}

bool BookmarkHTMLToken::Tag::test(const char *str, bool isEnd) const
{
    return name == QLatin1String(str) && isEnd == end;
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
    tag.end = tag.comment = false;

    if (!m_device->getChar(&m_char) || !skipBlanks())
        return tag;

    if (!cmpNext('<')) {
        m_error = true;
        return tag;
    }

    if (m_char == '!') { // comment
        tag.comment = true;
        do { // skip
            if (!m_device->getChar(&m_char)) {
                m_error = true;
                return tag;
            }
        } while (m_char != '>');
        return tag;
    }

    if (m_char == '/') {
        tag.end = true;
        if (!m_device->getChar(&m_char)) {
            m_error = true;
            return tag;
        }
    }

    tag.name = readIdent();
    if (m_error)
        return tag;

    if (tag.end) {
        skipBlanks();
        if (m_char != '>')
            m_error = true;
    } else {
        if (!readAttributes(saveAttributes))
            m_error = true;
    }

    return tag;
}

QString BookmarkHTMLToken::readIdent()
{
    QString str;
    QChar ch;
    QByteArray otherChars = "-_";

    skipBlanks();
    while ((ch = QChar::fromAscii(m_char)).isLetterOrNumber() || otherChars.contains(m_char)) {
        str.append(ch);
        if (!m_device->getChar(&m_char)) {
            m_error = true;
            return QString();
        }
    }
    return str.toUpper();
}

QString BookmarkHTMLToken::readContent(char endChar)
{
    QString str;
    do {
        if (!m_device->getChar(&m_char)) {
            m_error = true;
            return QString();
        }
        if (m_char != endChar)
            break;
        str.append(QChar::fromAscii(m_char));
    } while (true);
    m_device->putChar(m_char);

    return str;
}

bool BookmarkHTMLToken::readAttributes(bool save)
{
    while (true) {
        if (!skipBlanks())
            return false;
        if (m_char == '>')
            return true;

        QString name = readIdent();
        if (m_error || !skipBlanks() || !cmpNext('=') || !skipBlanks() || m_char != '"')
            return false;

        QString value = readContent('"');
        if (m_error || !m_device->getChar(&m_char))
            return false;

        if (save)
            m_attributes[name.toUpper()] = value;
    }
}

bool BookmarkHTMLToken::skipBlanks()
{
    if (!QChar::fromAscii(m_char).isSpace())
        return true;
    while (m_device->getChar(&m_char)) {
        if (!QChar::fromAscii(m_char).isSpace())
            return true;
    }
    return false;
}

bool BookmarkHTMLToken::cmpNext(char ch)
{
    return m_char == ch && m_device->getChar(&m_char);
}

BookmarksImporter::BookmarksImporter(const QString &fileName)
{
    m_parent = m_root = new BookmarkNode(BookmarkNode::Root);
    m_error = false;

    QFile file(fileName);
    if (!file.exists()) {
        // error
        return;
    }
    file.open(QFile::ReadOnly);

    if (fileName.endsWith(QLatin1String(".html"))) {
        parseHTML(&file);
    } else if (fileName.endsWith(QLatin1String(".adr"))) {
        parseADR(&file);
    } else {
        XbelReader reader;
        m_root = reader.read(&file);
        if (reader.error() != QXmlStreamReader::NoError) {
            m_error = true;
            m_errorString = tr("Error when loading bookmarks on line %1, column %2:\n"
                               "%3").arg(reader.lineNumber()).arg(reader.columnNumber()).arg(reader.errorString());
            delete m_root;
            m_root = 0;
            return;
        }
    }
}

bool BookmarksImporter::error() const
{
    return m_error;
}
QString BookmarksImporter::errorString() const
{
    return m_errorString;
}

BookmarkNode *BookmarksImporter::rootNode() const
{
    return m_root;
}

void BookmarksImporter::parseHTML(QIODevice *device)
{

}

void BookmarksImporter::parseADR(QIODevice *device)
{

}
