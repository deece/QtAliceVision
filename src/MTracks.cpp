#include "MTracks.hpp"

#include <aliceVision/matching/io.hpp>
#include <aliceVision/track/TracksBuilder.hpp>
#include <aliceVision/track/tracksUtils.hpp>

#include <QThreadPool>
#include <QFileInfo>
#include <QDebug>


namespace qtAliceVision {

/**
 * @brief QRunnable object dedicated to load sfmData using AliceVision.
 */
class TracksIORunnable : public QObject, public QRunnable
{
    Q_OBJECT

public:

    explicit TracksIORunnable(const QUrl& matchingFolder):
    _matchingFolder(matchingFolder)
    {}

    Q_SLOT void run() override;

    Q_SIGNAL void resultReady(aliceVision::track::TracksMap* _tracks, aliceVision::track::TracksPerView* _tracksPerView);

private:
    const QUrl _matchingFolder;
};

void TracksIORunnable::run()
{
    using namespace aliceVision;

    std::unique_ptr<track::TracksMap> tracks(new track::TracksMap());
    std::unique_ptr<track::TracksPerView> tracksPerView(new track::TracksPerView());
    try
    {
        matching::PairwiseMatches pairwiseMatches;
        if (!matching::Load(pairwiseMatches,
                            /*viewsKeysFilter=*/{},
                            {_matchingFolder.toLocalFile().toStdString()},
                            /*descTypes=*/{},
                            /*maxNbMatches=*/0,
                            /*minNbMatches=*/0))
        {
            qDebug() << "[QtAliceVision] Failed to load matches: " << _matchingFolder << ".";
        }
        track::TracksBuilder tracksBuilder;
        tracksBuilder.build(pairwiseMatches);
        tracksBuilder.exportToSTL(*tracks);
        track::computeTracksPerView(*tracks, *tracksPerView);
    }
    catch(std::exception& e)
    {
        qDebug() << "[QtAliceVision] Failed to load matches: " << _matchingFolder << "."
                 << "\n" << e.what();
    }

    Q_EMIT resultReady(tracks.release(), tracksPerView.release());
}

void MTracks::load()
{
    if(_matchingFolder.isEmpty())
    {
        clear();
        setStatus(None);
        return;
    }
    if(!QFileInfo::exists(_matchingFolder.toLocalFile()))
    {
        clear();
        setStatus(Error);
        return;
    }

    setStatus(Loading);    

    // load matches from file in a seperate thread
    TracksIORunnable* ioRunnable = new TracksIORunnable(_matchingFolder);
    connect(ioRunnable, &TracksIORunnable::resultReady, this, &MTracks::onReady);
    QThreadPool::globalInstance()->start(ioRunnable);
}

void MTracks::onReady(aliceVision::track::TracksMap* tracks, aliceVision::track::TracksPerView* tracksPerView)
{
    _tracks.reset(tracks);
    _tracksPerView.reset(tracksPerView);
    setStatus(Ready);
}

}

#include "MTracks.moc"
