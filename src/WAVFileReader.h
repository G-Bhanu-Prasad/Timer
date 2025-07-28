#ifndef __wav_file_reader_h__
#define __wav_file_reader_h__

#include <SPIFFS.h>
#include <FS.h>
#include "SampleSource.h"

class WAVFileReader : public SampleSource {
private:
    int      m_num_channels;
    int      m_sample_rate;
    File     m_file;
    uint32_t m_data_bytes;    // total bytes of sample data
    uint32_t m_bytes_read;    // how many we’ve already read

public:
    WAVFileReader(const char *file_name);
    ~WAVFileReader();
    int  sampleRate() override { return m_sample_rate; }
    void getFrames(Frame_t *frames, int number_frames) override;
    bool isFinished() const { return m_bytes_read >= m_data_bytes; }
};

#endif
