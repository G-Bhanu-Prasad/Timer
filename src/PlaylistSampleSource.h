#ifndef __playlist_source_h__
#define __playlist_source_h__

#include <vector>
#include "SampleSource.h"
#include "WAVFileReader.h"

class PlaylistSampleSource : public SampleSource {
private:
    std::vector<const char*> m_files;
    size_t                  m_index;
    WAVFileReader          *m_current;

    // Open the next file in the list (or nullptr if done)
    void openNext();

public:
    PlaylistSampleSource(const std::vector<const char*> &files);
    ~PlaylistSampleSource();

    int  sampleRate() override;
    void getFrames(Frame_t *frames, int number_frames) override;
};

#endif
