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

#ifndef BOOKMARKSIMPORTER_H
#define BOOKMARKSIMPORTER_H

#include <qmap.h>

class QIODevice;
class BookmarkNode;

class BookmarkHTMLToken
{
public:
    enum Type {
        Meta,
        Title,
        Header,
        ListStart,
        ListEnd,
        Folder,
        Link,
        Separator
    };

    BookmarkHTMLToken(QIODevice *device);

    bool error() { return m_error; }
    Type type() { return m_type;  }
    QString content() { return m_content; }
    QString attr(const QString &key);

private:
    QString readIdent();
    QString readContent(char endChar);
    void skipBlanks();

    char m_char;
    QIODevice *m_device;
    bool m_error;
    Type m_type;
    QString m_content;
    QMap m_attributes;
};

class BookmarksImporter
{
public:
    BookmarksImporter(const QString &path);

    bool error() { return m_error; }
    QString errorString() { return m_errorString; }

    BookmarkNode *rootNode() { return m_root; }

private:
    void parseHTML(QIODevice *device);
    void parseADR(QIODevice *device);

    bool m_error;
    QString m_errorString;
    BookmarkNode *m_root;
};

#endif // BOOKMARKSIMPORTER_H
