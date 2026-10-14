#include <QTest>
#include <QTemporaryDir>
#include "medialibrary/playlistart.hpp"
#include "medialibrary/mlplaylistlistmodel.hpp"

class TestPlaylistArt : public QObject
{
    Q_OBJECT
private slots:
    void playlistSorting()
    {
        QCOMPARE(MLPlaylistListModel::sortingCriteriaForName("title"), VLC_ML_SORTING_ALPHA);
        QCOMPARE(MLPlaylistListModel::sortingCriteriaForName(""), VLC_ML_SORTING_DEFAULT);
        QCOMPARE(MLPlaylistListModel::sortingCriteriaForName("unsupported"), VLC_ML_SORTING_DEFAULT);
    }
    void persistenceAndIdentity()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath("settings.ini");
        const QUrl art = QUrl::fromLocalFile(dir.filePath("cover with spaces.png"));
        {
            QSettings settings(file, QSettings::IniFormat);
            PlaylistArt::save(settings, 21, art);
            QCOMPARE(PlaylistArt::load(settings, 21), art.toString(QUrl::FullyEncoded));
            QVERIFY(PlaylistArt::load(settings, 22).isEmpty());
        }
        QSettings settings(file, QSettings::IniFormat);
        QCOMPARE(PlaylistArt::load(settings, 21), art.toString(QUrl::FullyEncoded));
        const auto second = QUrl::fromLocalFile(dir.filePath("second.png"));
        PlaylistArt::save(settings, 22, second);
        PlaylistArt::save(settings, 21, QUrl("https://example.com/unsupported.png"));
        QCOMPARE(PlaylistArt::load(settings, 21), art.toString(QUrl::FullyEncoded));
        PlaylistArt::save(settings, 21, second);
        QCOMPARE(PlaylistArt::load(settings, 21), second.toString(QUrl::FullyEncoded));
        PlaylistArt::save(settings, 21, QUrl());
        QVERIFY(PlaylistArt::load(settings, 21).isEmpty());
        QCOMPARE(PlaylistArt::load(settings, 22), second.toString(QUrl::FullyEncoded));
    }
    void rejectsUnsupportedArtwork()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.ini"), QSettings::IniFormat);
        PlaylistArt::save(settings, 0, QUrl::fromLocalFile("/cover.png"));
        PlaylistArt::save(settings, -1, QUrl::fromLocalFile("/cover.png"));
        PlaylistArt::save(settings, 3, QUrl("https://example.com/cover.png"));
        QVERIFY(settings.allKeys().isEmpty());
    }
};
QTEST_GUILESS_MAIN(TestPlaylistArt)
#include "test_playlist_art.moc"
