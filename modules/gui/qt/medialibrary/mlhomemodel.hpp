/******************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Author: Ash <ashutoshv191@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 ******************************************************************************/
#pragma once

#include "mlmediamodel.hpp"
#include <mutex>

// Home projections of existing ML media/history and collection data. All
// enumeration and ranking runs on the ML thread through MLListCacheLoader.
class MLHomeModel : public MLMediaModel
{
    Q_OBJECT
    Q_PROPERTY(Section section READ section WRITE setSection NOTIFY sectionChanged FINAL)
public:
    ~MLHomeModel() override { stopEventListener(); }

    enum Section { ContinueWatching, ListenAgain, Folders, Playlists, Artists };
    Q_ENUM(Section)
    enum { HOME_SUBTITLE = MEDIA_ROLES_COUNT, HOME_SCORE, HOME_LAST_PLAYED };

    explicit MLHomeModel(QObject* parent = nullptr) : MLMediaModel(parent) {}
    Section section() const { return m_section; }
    void setSection(Section section);
    QHash<int, QByteArray> roleNames() const override;
    QVariant itemRoleData(const MLItem* item, int role) const override;

signals:
    void sectionChanged();

protected:
    std::unique_ptr<MLListCacheLoader> createMLLoader() const override;
    void onVlcMlEvent(const MLEvent& event) override;
    void thumbnailUpdated(const QModelIndex&, MLItem*, const QString&, vlc_ml_thumbnail_status_t) override;

private:
    Section m_section = ContinueWatching;
    struct Loader : MLListCacheLoader::MLOp
    {
        explicit Loader(const MLHomeModel& model) : MLOp(model), section(model.section()) {}
        size_t count(vlc_medialibrary_t*, const vlc_ml_query_params_t*) const override;
        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t*, const vlc_ml_query_params_t*) const override;
        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t*, MLItemId) const override;
        const std::vector<std::unique_ptr<MLItem>>& snapshot(vlc_medialibrary_t*) const;
        std::vector<std::unique_ptr<MLItem>> buildSnapshot(vlc_medialibrary_t*) const;
        mutable std::once_flag snapshotOnce;
        mutable std::vector<std::unique_ptr<MLItem>> snapshotRows;
        Section section;
    };
};
