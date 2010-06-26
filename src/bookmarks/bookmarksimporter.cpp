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


BookmarkHTMLToken::BookmarkHTMLToken(QIODevice *device)
{

}

QString BookmarkHTMLToken::attr(const QString &key)
{
    if (m_attributes.contains(key))
        return m_attributes[key];
    else
        return QString();
}

QString BookmarkHTMLToken::readIdent()
{
    skipBlanks();
    QByteArray otherChars = "-_";
    QString str;

    while (QChar(m_char).isLetterOrNumber() || otherChars.contains(m_char)) {
        str.append(m_char);
        if (!m_device->getChar(m_char)) {
            m_error = true;
            return QString();
        }
    }
    return str;
}

QString BookmarkHTMLToken::readContent(char endChar)
{
    QString str;
    while (m_char != endChar) {
        str.append(m_char);
        if (!m_device->getChar(m_char)) {
            m_error = true;
            return QString();
        }
    }
    return str;
}

void BookmarkHTMLToken::skipBlanks()
{
    if (!QChar(m_char).isSpace())
        return;
    while (m_device->getChar(m_char) && QChar(m_char).isSpace()) {}
}

BookmarksImporter::BookmarksImporter(const QString &path)
{
    root = new BookmarkNode(BookmarkNode::Root);
    error = false;

    QFile file(fileName);
    if (!file.exists()) {
        // error
        return;
    }
    file.open(QFile::ReadOnly);

    if (fileName.endsWith(QLatin1String(".html"))) {
        parseHTML(file);
    } else if (fileName.endsWith(QLatin1String(".adr"))) {
        parseADR(file);
    } else {
        XbelReader reader;
        importRootNode = reader.read(file);
    }

    if (reader.error() != QXmlStreamReader::NoError) {
        m_error = true;
        m_errorString = tr("Error when loading bookmarks on line %1, column %2:\n"
                           "%3").arg(reader.lineNumber()).arg(reader.columnNumber()).arg(reader.errorString());
        delete root;
        root = 0;
        return;
    }

    return root;
}

void BookmarksImporter::parseHTML(QIODevice *device)
{

}

void BookmarksImporter::parseADR(QIODevice *device)
{

}
