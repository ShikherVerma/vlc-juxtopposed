/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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
 *****************************************************************************/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mlvideofoldersmodel.hpp"
#include <algorithm>

// VLC includes
#include <vlc_media_library.h>

// Util includes
#include "util/covergenerator.hpp"
#include "util/vlctick.hpp"

// MediaLibrary includes
#include "mlcustomcover.hpp"
#include "mlfolder.hpp"
#include "mlhelper.hpp"

// Static variables

// NOTE: We multiply by 3 to cover most dpi settings.
static const int MLVIDEOFOLDERSMODEL_COVER_WIDTH  = 260 * 3; // 16 / 10 ratio
static const int MLVIDEOFOLDERSMODEL_COVER_HEIGHT = 162 * 3;

// Ctor / dtor

/* explicit */ MLVideoFoldersModel::MLVideoFoldersModel(QObject * parent) : MLBaseModel(parent) {}

void MLVideoFoldersModel::setHierarchical(bool value)
{
    if (m_hierarchical == value)
        return;
    m_hierarchical = value;
    emit hierarchicalChanged();
    emit resetRequested();
}

void MLVideoFoldersModel::rescan(const QUrl &url)
{
    if (!m_mediaLib)
        return;
    m_mediaLib->runOnMLThread(this, [url](vlc_medialibrary_t *ml) {
        const QByteArray mrl = url.toEncoded();
        vlc_ml_reload_folder(ml, mrl.isEmpty() ? nullptr : mrl.constData());
    });
}

// MLBaseModel reimplementation

QHash<int, QByteArray> MLVideoFoldersModel::roleNames() const /* override */
{
    return {
        { FOLDER_ID, QByteArrayLiteral("id") },
        { FOLDER_TITLE, QByteArrayLiteral("title") },
        { FOLDER_TITLE_FIRST_SYMBOL, QByteArrayLiteral("title_first_symbol") },
        { FOLDER_THUMBNAIL, QByteArrayLiteral("thumbnail") },
        { FOLDER_DURATION, QByteArrayLiteral("duration") },
        { FOLDER_COUNT, QByteArrayLiteral("count") },
        { FOLDER_NB_VIDEOS, QByteArrayLiteral("nb_videos") },
        { FOLDER_MRL, QByteArrayLiteral("mrl") },
    };
}

// Protected MLVideoModel implementation

QVariant MLVideoFoldersModel::itemRoleData(const MLItem * item, const int role) const /* override */
{
    if (item == nullptr)
        return QVariant();

    const MLFolder * folder = static_cast<const MLFolder *> (item);

    switch (role)
    {
        // NOTE: This is the condition for QWidget view(s).
        case Qt::DisplayRole:
            return QVariant::fromValue(folder->getTitle());
        // NOTE: These are the conditions for QML view(s).
        case FOLDER_ID:
            return QVariant::fromValue(folder->getId());
        case FOLDER_TITLE:
            return QVariant::fromValue(folder->getTitle());
        case FOLDER_MRL:
            return QUrl(folder->getMRL());
        case FOLDER_TITLE_FIRST_SYMBOL:
            return QVariant::fromValue( getFirstSymbol(folder->getTitle()) );
        case FOLDER_THUMBNAIL:
        {
            return MLCustomCover::url(folder->getId()
                                    , QSize(MLVIDEOFOLDERSMODEL_COVER_WIDTH, MLVIDEOFOLDERSMODEL_COVER_HEIGHT)
                                    , QStringLiteral(":/placeholder/noart_videoCover.svg"));
        }
        case FOLDER_DURATION:
            return QVariant::fromValue(folder->getDuration());
        case FOLDER_COUNT:
        case FOLDER_NB_VIDEOS:
            return QVariant::fromValue(folder->getVideoCount());
        default:
            return QVariant();
    }
}

vlc_ml_sorting_criteria_t MLVideoFoldersModel::nameToCriteria(QByteArray name) const /* override */
{
    return QHash<QByteArray, vlc_ml_sorting_criteria_t> {
        { QByteArrayLiteral("title"),    VLC_ML_SORTING_ALPHA    },
        { QByteArrayLiteral("duration"), VLC_ML_SORTING_DURATION },
    }.value(name, VLC_ML_SORTING_DEFAULT);
}

std::unique_ptr<MLListCacheLoader>
MLVideoFoldersModel::createMLLoader() const /* override */
{
    return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<MLVideoFoldersModel::Loader>(*this));
}

// Protected MLBaseModel reimplementation

void MLVideoFoldersModel::onVlcMlEvent(const MLEvent & event) /* override */
{
    int type = event.i_type;
    if (m_hierarchical && (type == VLC_ML_EVENT_FOLDER_UPDATED
        || type == VLC_ML_EVENT_ENTRY_POINT_ADDED || type == VLC_ML_EVENT_ENTRY_POINT_REMOVED
        || type == VLC_ML_EVENT_ENTRY_POINT_BANNED || type == VLC_ML_EVENT_ENTRY_POINT_UNBANNED))
    {
        emit resetRequested();
        return;
    }

    switch (type)
    {
        case VLC_ML_EVENT_FOLDER_ADDED:
        {
            emit resetRequested();
            return;
        }
        case VLC_ML_EVENT_FOLDER_UPDATED:
        {
            MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_FOLDER);
            updateItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_FOLDER_DELETED:
        {
            MLItemId itemId(event.deletion.i_entity_id, VLC_ML_PARENT_FOLDER);
            deleteItemInCache(itemId);
            return;
        }
        default:
            break;
    }

    MLBaseModel::onVlcMlEvent(event);
}

// The pinned ML subfolder API ignores query parameters. Adapt filtering,
// ordering and pagination in Qt, on the ML worker, not in the engine.
std::vector<std::unique_ptr<MLItem>> MLVideoFoldersModel::Loader::folders(vlc_medialibrary_t *ml) const
{
    vlc_ml_folder_list_t *raw = nullptr;
    if (m_parent.id > 0)
        vlc_ml_list(ml, VLC_ML_LIST_SUBFOLDERS, nullptr, m_parent.id, &raw);
    else
        raw = vlc_ml_list_entry_points(ml, nullptr);
    ml_unique_ptr<vlc_ml_folder_list_t> list(raw);
    std::vector<std::unique_ptr<MLItem>> result;
    if (!list)
        return result;
    const QString pattern = QString::fromUtf8(m_searchPattern);
    for (const auto &folder : ml_range_iterate<vlc_ml_folder_t>(list))
    {
        if (folder.b_banned || !QString::fromUtf8(folder.psz_name).contains(pattern, Qt::CaseInsensitive))
            continue;
        result.emplace_back(std::make_unique<MLFolder>(&folder));
    }
    std::stable_sort(result.begin(), result.end(), [this](const auto &a, const auto &b) {
        const auto left = static_cast<const MLFolder *>(a.get());
        const auto right = static_cast<const MLFolder *>(b.get());
        const int cmp = QString::localeAwareCompare(left->getTitle(), right->getTitle());
        return m_sortDesc ? cmp > 0 : cmp < 0;
    });
    return result;
}

// Loader
size_t MLVideoFoldersModel::Loader::count(vlc_medialibrary_t * ml, const vlc_ml_query_params_t* queryParams) const /* override */
{
    if (hierarchical)
        return folders(ml).size();
    return vlc_ml_count_folders_by_type(ml, queryParams, VLC_ML_MEDIA_TYPE_VIDEO);
}

std::vector<std::unique_ptr<MLItem>>
MLVideoFoldersModel::Loader::load(vlc_medialibrary_t * ml,
                                  const vlc_ml_query_params_t* queryParams) const /* override */
{
    if (hierarchical)
    {
        auto result = folders(ml);
        const size_t offset = std::min(size_t(queryParams->i_offset), result.size());
        const size_t end = queryParams->i_nbResults == 0 ? result.size()
            : std::min(result.size(), offset + queryParams->i_nbResults);
        std::vector<std::unique_ptr<MLItem>> page;
        for (size_t i = offset; i < end; ++i)
            page.push_back(std::move(result[i]));
        return page;
    }

    ml_unique_ptr<vlc_ml_folder_list_t> list(vlc_ml_list_folders_by_type(ml, queryParams,
                                                                         VLC_ML_MEDIA_TYPE_VIDEO));

    if (list == nullptr)
        return {};

    std::vector<std::unique_ptr<MLItem>> result;

    for (const vlc_ml_folder_t & folder : ml_range_iterate<vlc_ml_folder_t>(list))
    {
        result.emplace_back(std::make_unique<MLFolder>(&folder));
    }

    return result;
}

std::unique_ptr<MLItem>
MLVideoFoldersModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_FOLDER);
    ml_unique_ptr<vlc_ml_folder_t> folder(vlc_ml_get_folder(ml, itemId.id));
    if (!folder)
        return nullptr;
    return std::make_unique<MLFolder>(folder.get());
}
