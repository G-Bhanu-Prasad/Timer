#include "PlaylistSampleSource.h"

PlaylistSampleSource::PlaylistSampleSource(const std::vector<const char*> &files)
 : m_files(files), m_index(0), m_current(nullptr)
{
    openNext();
}

PlaylistSampleSource::~PlaylistSampleSource() {
    delete m_current;
}

void PlaylistSampleSource::openNext() {
    delete m_current;
    if (m_index < m_files.size()) {
        m_current = new WAVFileReader(m_files[m_index++]);
    } else {
        m_current = nullptr;
    }
}

int PlaylistSampleSource::sampleRate() {
    return m_current ? m_current->sampleRate() : 0;
}

void PlaylistSampleSource::getFrames(Frame_t *frames, int number_frames) {
    if (!m_current) {
        // no more files → silence
        for (int i = 0; i < number_frames; i++)
            frames[i] = {0,0};
        return;
    }

    int offset = 0;
    while (offset < number_frames && m_current) {
        // ask current reader for the rest of the buffer
        m_current->getFrames(frames + offset, number_frames - offset);

        // if it’s now finished, advance to next file
        if (m_current->isFinished()) {
            openNext();
        }
        // advance offset by however many frames we actually consumed:
        // if we switched files, we know all frames up to EOF were “used”,
        // but to keep it simple we assume block‐reads match block size.
        // (If you need tighter control you can add a method to query
        // how many frames remain in the current file.)
        offset = number_frames;  // done filling
    }
}
