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

class BookmarksDevice {
public:
    enum FileType {
        ADR,
        HTML
    };

    BookmarksDevice(QIODevice *device, FileType type = HTML);
    int line() const;
    int column() const;
    bool getChar(QChar &ch);

private:
    static const int BUFF_SIZE = 512;

    bool makeString();

    QByteArray m_data;
    QString m_str;
    QTextCodec *m_codec;
    QIODevice *m_device;
    bool m_utf;
    int m_line;
    int m_column;
    int m_pos;
};

class BookmarkHTMLToken
{
public:
    enum Type {
        Empty,
        Meta,
        Title,
        Header,
        ListStart,
        ListEnd,
        Paragraph,
        Folder,
        Bookmark,
        Description,
        Separator
    };

    BookmarkHTMLToken(BookmarksDevice *device);

    bool readNext();
    bool last() const;
    bool error() const;
    Type type() const;
    QString content() const;
    QString attr(const QString &key) const;

private:
    class Tag
    {
    public:
        bool test(const char *str, bool isEnd = false) const;
        QString name;
        bool end;
        bool comment;
        bool error;
    };

    Tag readTag(bool saveAttributes = true);
    QString readIdent();
    QString readContent(QChar endChar = QLatin1Char('<'));
    bool readAttributes(bool save = true);
    bool skipBlanks();
    bool cmpNext(char ch);

    QChar m_char;
    bool m_last;
    bool m_error;
    Type m_type;
    QString m_content;
    QMap<QString, QString> m_attributes;
    BookmarksDevice *m_device;
};

class BookmarksImporter : public QObject
{
public:
    BookmarksImporter(const QString &fileName);

    bool error() const;
    int errorLine() const;
    int errorColumn() const;
    QString errorString() const;
    BookmarkNode *rootNode() const;

private:
    void parseHTML();
    void parseHTMLFolder(BookmarkNode *parent);
    void parseADR();

    bool m_error;
    int m_errorLine;
    int m_errorColumn;
    QString m_errorString;
    BookmarksDevice *m_device;
    BookmarkNode *m_root;
    BookmarkHTMLToken *m_HTMLToken;
};

#endif // BOOKMARKSIMPORTER_H
