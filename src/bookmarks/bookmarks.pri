INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
HEADERS += addbookmarkdialog.h \
    bookmarksdialog.h \
    bookmarksmanager.h \
    bookmarksmenu.h \
    bookmarksmodel.h \
    bookmarkstoolbar.h \
    bookmarknode.h \
    bookmarksimporter.h
SOURCES += addbookmarkdialog.cpp \
    bookmarksdialog.cpp \
    bookmarksmanager.cpp \
    bookmarksmenu.cpp \
    bookmarksmodel.cpp \
    bookmarkstoolbar.cpp \
    bookmarknode.cpp \
    bookmarksimporter.cpp
FORMS += addbookmarkdialog.ui \
    bookmarksdialog.ui
include(xbel/xbel.pri)
