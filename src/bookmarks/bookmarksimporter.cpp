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

// parse one html token
BookmarkHTMLToken::BookmarkHTMLToken(QIODevice *device)
{
    m_device = device;
    m_error = false;

    if (m_device->getChar(&m_char)) {
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
    return name == QLatin1String(str) && isEnd == end;
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
    tag.end = tag.comment = false;

    if (!skipBlanks())
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
    } else {
        if (m_char == '/') {
            tag.end = true;
            if (!m_device->getChar(&m_char))
                m_error = true;
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
    }

    if (!m_error)
        m_last = (!m_device->getChar(&m_char) || !skipBlanks());

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
        if (m_char == endChar)
            break;
        str.append(QChar::fromAscii(m_char));

        if (!m_device->getChar(&m_char)) {
            m_error = true;
            return QString();
        }
    } while (true);

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
        if (m_error || !skipBlanks() || !cmpNext('=') || !skipBlanks() || !cmpNext('"'))
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
    m_root = new BookmarkNode(BookmarkNode::Root);
    m_error = false;

    QFile file(fileName);
    if (!file.exists()) {
        // error
        return;
    }
    file.open(QFile::ReadOnly);
    m_device = &file;

    if (fileName.endsWith(QLatin1String(".html"))) {
        parseHTML();
    } else if (fileName.endsWith(QLatin1String(".adr"))) {
        parseADR();
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
}

void BookmarksImporter::parseADR()
{

}
