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
#include "mlhomemodel.hpp"
#include "mlhomeranking.hpp"
#include "mlhelper.hpp"
#include <algorithm>

namespace {
class HomeCollection : public MLItem
{
public:
    HomeCollection(MLItemId id, QString title, QString cover)
        : MLItem(id), title(std::move(title)), cover(std::move(cover)) {}
    QString title, cover;
    uint64_t plays = 0;
    int64_t recent = 0;
};

int64_t recent(const MLItem* item)
{
    if (auto media = dynamic_cast<const MLMedia*>(item))
        return media->lastPlayedDate().toSecsSinceEpoch();
    return static_cast<const HomeCollection*>(item)->recent;
}
uint64_t plays(const MLItem* item)
{
    if (auto media = dynamic_cast<const MLMedia*>(item))
        return std::max(0, media->playCount());
    return static_cast<const HomeCollection*>(item)->plays;
}
QString title(const MLItem* item)
{
    if (auto media = dynamic_cast<const MLMedia*>(item))
        return media->title();
    return static_cast<const HomeCollection*>(item)->title;
}

void aggregate(HomeCollection& item, const vlc_ml_media_list_t* media)
{
    if (!media)
        return;
    for (const auto& m : ml_range_iterate<const vlc_ml_media_t>(media))
    {
        item.plays += m.i_playcount;
        item.recent = std::max(item.recent, int64_t(m.i_last_played_date));
    }
}
std::unique_ptr<MLItem> clone(const MLItem* item)
{
    if (auto video = dynamic_cast<const MLVideo*>(item))
        return std::make_unique<MLVideo>(*video);
    if (auto audio = dynamic_cast<const MLAudio*>(item))
        return std::make_unique<MLAudio>(*audio);
    return std::make_unique<HomeCollection>(*static_cast<const HomeCollection*>(item));
}
QString artwork(const vlc_ml_thumbnail_t& thumb)
{
    return thumb.i_status == VLC_ML_THUMBNAIL_STATUS_AVAILABLE ? qfu(thumb.psz_mrl) : QString{};
}
}

void MLHomeModel::setSection(Section section)
{
    if (m_section == section)
        return;
    m_section = section;
    emit sectionChanged();
    emit resetRequested();
}

QHash<int, QByteArray> MLHomeModel::roleNames() const
{
    auto roles = MLMediaModel::roleNames();
    roles.insert(HOME_SUBTITLE, "subtitle");
    roles.insert(HOME_SCORE, "home_playcount");
    roles.insert(HOME_LAST_PLAYED, "home_last_played");
    return roles;
}

QVariant MLHomeModel::itemRoleData(const MLItem* item, int role) const
{
    if (role == HOME_SCORE)
        return QVariant::fromValue(qulonglong(plays(item)));
    if (role == HOME_LAST_PLAYED)
        return QVariant::fromValue(qlonglong(recent(item)));
    if (role == HOME_SUBTITLE)
    {
        if (auto audio = dynamic_cast<const MLAudio*>(item))
            return audio->getArtist();
        return QString{};
    }
    if (auto collection = dynamic_cast<const HomeCollection*>(item))
    {
        switch (role)
        {
        case MEDIA_ID: return QVariant::fromValue(item->getId());
        case MEDIA_TITLE: return collection->title;
        case MEDIA_SMALL_COVER: return collection->cover;
        case MEDIA_PROGRESS: return 0.;
        default: return {};
        }
    }
    return MLMediaModel::itemRoleData(item, role);
}

void MLHomeModel::thumbnailUpdated(const QModelIndex& index, MLItem* item,
                                   const QString& mrl, vlc_ml_thumbnail_status_t status)
{
    if (auto video = dynamic_cast<MLVideo*>(item))
    {
        video->setSmallCover(status, mrl);
        emit dataChanged(index, index, { MEDIA_SMALL_COVER });
    }
}

void MLHomeModel::onVlcMlEvent(const MLEvent& event)
{
    // Membership and order depend on history/progress, not just item contents.
    switch (event.i_type)
    {
    case VLC_ML_EVENT_HISTORY_CHANGED:
    case VLC_ML_EVENT_MEDIA_ADDED:
    case VLC_ML_EVENT_MEDIA_UPDATED:
    case VLC_ML_EVENT_MEDIA_DELETED:
    case VLC_ML_EVENT_FOLDER_ADDED:
    case VLC_ML_EVENT_FOLDER_UPDATED:
    case VLC_ML_EVENT_FOLDER_DELETED:
    case VLC_ML_EVENT_PLAYLIST_ADDED:
    case VLC_ML_EVENT_PLAYLIST_UPDATED:
    case VLC_ML_EVENT_PLAYLIST_DELETED:
    case VLC_ML_EVENT_ARTIST_ADDED:
    case VLC_ML_EVENT_ARTIST_UPDATED:
    case VLC_ML_EVENT_ARTIST_DELETED:
        emit resetRequested();
        return;
    default: MLMediaModel::onVlcMlEvent(event);
    }
}

std::unique_ptr<MLListCacheLoader> MLHomeModel::createMLLoader() const
{
    return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<Loader>(*this));
}

std::vector<std::unique_ptr<MLItem>> MLHomeModel::Loader::buildSnapshot(vlc_medialibrary_t* ml) const
{
    std::vector<std::unique_ptr<MLItem>> rows;
    auto query = vlc_ml_query_params_create();
    if (section == ContinueWatching || section == ListenAgain)
    {
        ml_unique_ptr<vlc_ml_media_list_t> media(section == ContinueWatching
            ? vlc_ml_list_video_history(ml, &query)
            : vlc_ml_list_audio_history(ml, &query));
        if (media)
            for (const auto& m : ml_range_iterate<vlc_ml_media_t>(media))
            {
                if (m.i_last_played_date <= 0)
                    continue;
                if (section == ContinueWatching)
                {
                    if (MLHomeRanking::resumable(m.f_progress, m.i_last_played_date))
                        rows.emplace_back(std::make_unique<MLVideo>(&m));
                }
                else
                    rows.emplace_back(std::make_unique<MLAudio>(ml, &m));
            }
    }
    else if (section == Folders)
    {
        ml_unique_ptr<vlc_ml_folder_list_t> folders(vlc_ml_list_folders(ml, &query));
        if (folders)
            for (const auto& f : ml_range_iterate<vlc_ml_folder_t>(folders))
            {
                if (!f.b_present || f.b_banned || f.i_nb_media == 0)
                    continue;
                auto row = std::make_unique<HomeCollection>(MLItemId(f.i_id, VLC_ML_PARENT_FOLDER), qfu(f.psz_name), QString{});
                ml_unique_ptr<vlc_ml_media_list_t> media(vlc_ml_list_folder_media(ml, &query, f.i_id));
                aggregate(*row, media.get());
                if (row->plays > 0 || row->recent > 0)
                    rows.emplace_back(std::move(row));
            }
    }
    else if (section == Playlists)
    {
        ml_unique_ptr<vlc_ml_playlist_list_t> lists(vlc_ml_list_playlists(ml, &query, VLC_ML_PLAYLIST_TYPE_ALL));
        if (lists)
            for (const auto& p : ml_range_iterate<vlc_ml_playlist_t>(lists))
            {
                auto row = std::make_unique<HomeCollection>(MLItemId(p.i_id, VLC_ML_PARENT_PLAYLIST), qfu(p.psz_name), qfu(p.psz_artwork_mrl));
                ml_unique_ptr<vlc_ml_media_list_t> media(vlc_ml_list_playlist_media(ml, &query, p.i_id));
                aggregate(*row, media.get());
                if (row->plays > 0 || row->recent > 0)
                    rows.emplace_back(std::move(row));
            }
    }
    else if (section == Artists)
    {
        ml_unique_ptr<vlc_ml_artist_list_t> artists(vlc_ml_list_artists(ml, &query, true));
        if (artists)
            for (const auto& a : ml_range_iterate<vlc_ml_artist_t>(artists))
            {
                auto row = std::make_unique<HomeCollection>(MLItemId(a.i_id, VLC_ML_PARENT_ARTIST), qfu(a.psz_name), artwork(a.thumbnails[VLC_ML_THUMBNAIL_SMALL]));
                ml_unique_ptr<vlc_ml_media_list_t> media(vlc_ml_list_artist_tracks(ml, &query, a.i_id));
                aggregate(*row, media.get());
                if (row->plays > 0 || row->recent > 0)
                    rows.emplace_back(std::move(row));
            }
    }
    const bool frequent = section == Folders || section == Playlists || section == Artists;
    std::sort(rows.begin(), rows.end(), [frequent](const auto& a, const auto& b) {
        return MLHomeRanking::before(
            { plays(a.get()), recent(a.get()), title(a.get()), a->getId().id },
            { plays(b.get()), recent(b.get()), title(b.get()), b->getId().id }, frequent);
    });
    if (!m_searchPattern.isEmpty())
    {
        const QString pattern = QString::fromUtf8(m_searchPattern);
        rows.erase(std::remove_if(rows.begin(), rows.end(), [&pattern](const auto& row) {
            return !title(row.get()).contains(pattern, Qt::CaseInsensitive);
        }), rows.end());
    }
    return rows;
}

const std::vector<std::unique_ptr<MLItem>>& MLHomeModel::Loader::snapshot(vlc_medialibrary_t* ml) const
{
    // A loader is replaced on reset. Count, paging and getData read the same
    // immutable ranked snapshot, never independently re-enumerated datasets.
    std::call_once(snapshotOnce, [this, ml] { snapshotRows = buildSnapshot(ml); });
    return snapshotRows;
}

size_t MLHomeModel::Loader::count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t*) const
{
    return snapshot(ml).size();
}
std::vector<std::unique_ptr<MLItem>> MLHomeModel::Loader::load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* query) const
{
    const auto& rows = snapshot(ml);
    const size_t begin = std::min(size_t(query->i_offset), rows.size());
    const size_t end = query->i_nbResults == 0 ? rows.size() : std::min(rows.size(), begin + query->i_nbResults);
    std::vector<std::unique_ptr<MLItem>> result;
    for (size_t i = begin; i < end; ++i)
        result.emplace_back(clone(rows[i].get()));
    return result;
}
std::unique_ptr<MLItem> MLHomeModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId id) const
{
    const auto& rows = snapshot(ml);
    for (const auto& row : rows)
        if (row->getId() == id)
            return clone(row.get());
    return nullptr;
}
