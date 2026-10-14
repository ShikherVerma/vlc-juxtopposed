/*****************************************************************************
 * settings_dialog.hpp : Authored settings shell and General / Interface editors
 * Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later
 *****************************************************************************/
#ifndef VLC_QT_SETTINGS_DIALOG_HPP
#define VLC_QT_SETTINGS_DIALOG_HPP

#include "widgets/native/qvlcframe.hpp"
#include "settings_metadata.hpp"
#include <array>
#include <QVariant>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QScrollArea;
class QMouseEvent;
class SystemPalette;
class QComboBox;
class QVBoxLayout;

class SettingsDialog : public QVLCDialog
{
    Q_OBJECT
public:
    SettingsDialog(QWindow *, qt_intf_t *);

signals:
    void allSettingsRequested(const QString &key);

protected:
    void mousePressEvent(QMouseEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;

private:
    void selectCategory(int);
    void search(const QString &);
    void activateResult(QTreeWidgetItem *);
    void updateAccent();
    bool saveChanges();
    void resetEditors();
    bool hasChanges() const;
    void addControl(const SettingsControlSpec &, QVBoxLayout *);
    QVariant editorValue(QWidget *) const;
    void setEditorValue(QWidget *, const QVariant &);
    struct Editor
    {
        QString key;
        int type;
        QWidget *widget;
        QVariant saved;
        QVariant defaults;
    };
    QVector<Editor> m_editors;
    std::array<QWidget *, 2> m_simpleForms{};
    std::array<QWidget *, 2> m_advancedForms{};
    QComboBox *m_theme;
    int m_savedTheme = 0;
    void openAllSettings(const QString &key = QString());
    void applyTheme();

    QVector<SettingsOptionMetadata> m_metadata;
    SystemPalette *m_palette;
    std::array<QPushButton *, 13> m_categories;
    std::array<bool, 13> m_advanced{};
    std::array<QPushButton *, 9> m_swatches;
    int m_category = 0;
    QString m_savedAccent;
    QString m_accent;
    QString m_defaultAccent;
    QLabel *m_title;
    QLineEdit *m_search;
    QStackedWidget *m_pages;
    QScrollArea *m_pageScroll;
    QWidget *m_accentRow;
    QLabel *m_accentValue;
    QLabel *m_pending;
    QLabel *m_restart;
    QLabel *m_resultCount;
    QPushButton *m_advancedButton;
    QTreeWidget *m_results;
};
#endif
