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
#include <qobject.h>

class QIODevice;
class BookmarkNode;

class BookmarkHTMLToken
{
public:
    enum Type {
        Empty,
        Meta,
        Title,
        Header,
        ListBegin,
        ListEnd,
        Paragraph,
        Folder,
        Link,
        Description,
        Separator
    };

    BookmarkHTMLToken(QIODevice *device);

    bool error() const;
    Type type() const;
    QString content() const;
    QString attr(const QString &key) const;

private:
    struct Tag
    {
        bool test(const char *str, bool isEnd = false) const;
        QString name;
        bool end;
        bool comment;
    };

    Tag readTag(bool saveAttributes = true);
    QString readIdent();
    QString readContent(char endChar = '<');
    bool readAttributes(bool save = true);
    bool skipBlanks();
    bool cmpNext(char ch);

    char m_char;
    QIODevice *m_device;
    bool m_error;
    Type m_type;
    QString m_content;
    QMap<QString, QString> m_attributes;
};

class BookmarksImporter : public QObject
{
public:
    BookmarksImporter(const QString &fileName);

    bool error() const;
    QString errorString() const;
    BookmarkNode *rootNode() const;

private:
    void parseHTML(QIODevice *device);
    void parseADR(QIODevice *device);

    bool m_error;
    QString m_errorString;
    BookmarkNode *m_root;
    BookmarkNode *m_parent;
};

#endif // BOOKMARKSIMPORTER_H
