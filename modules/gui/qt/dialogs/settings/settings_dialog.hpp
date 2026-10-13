/*****************************************************************************
 * settings_dialog.hpp : Authored settings shell (category panels pending)
 * Copyright (C) 2026 VLC authors and VideoLAN
 * SPDX-License-Identifier: GPL-2.0-or-later
 *****************************************************************************/
#ifndef VLC_QT_SETTINGS_DIALOG_HPP
#define VLC_QT_SETTINGS_DIALOG_HPP

#include "widgets/native/qvlcframe.hpp"
#include "settings_metadata.hpp"
#include <array>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QMouseEvent;
class SystemPalette;

class SettingsDialog : public QVLCDialog
{
    Q_OBJECT
public:
    SettingsDialog(QWindow *, qt_intf_t *);

signals:
    void allSettingsRequested();

protected:
    void mousePressEvent(QMouseEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;

private:
    void selectCategory(int);
    void search(const QString &);
    void updateAccent();
    bool saveAccent();
    void resetAccent();
    void openAllSettings();
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
    QWidget *m_accentRow;
    QLabel *m_accentValue;
    QLabel *m_pending;
    QLabel *m_restart;
    QLabel *m_resultCount;
    QPushButton *m_advancedButton;
    QTreeWidget *m_results;
};
#endif
