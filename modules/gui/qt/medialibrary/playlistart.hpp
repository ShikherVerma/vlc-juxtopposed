#ifndef VLC_QT_PLAYLISTART_HPP
#define VLC_QT_PLAYLISTART_HPP

#include <QSettings>
#include <QUrl>

// ML 0.13 exposes generated covers, but no saved-playlist artwork setter.
// Keep custom artwork outside the ML database, keyed by stable playlist ID.
namespace PlaylistArt {
inline QString key(qint64 id) { return QStringLiteral("PlaylistArtwork/%1").arg(id); }
inline void save(QSettings &settings, qint64 id, const QUrl &url)
{
    if (id <= 0)
        return;
    if (url.isEmpty())
        settings.remove(key(id));
    else if (url.isValid() && url.isLocalFile())
        settings.setValue(key(id), url.toString(QUrl::FullyEncoded));
}
inline QString load(QSettings &settings, qint64 id)
{
    return id > 0 ? settings.value(key(id)).toString() : QString();
}
}
#endif
