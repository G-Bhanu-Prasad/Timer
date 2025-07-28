#include <SPIFFS.h>
#include <FS.h>
#include "WAVFileReader.h"

#pragma pack(push, 1)
typedef struct {
    char riff_header[4];
    int  wav_size;
    char wave_header[4];
    char fmt_header[4];
    int  fmt_chunk_size;
    short audio_format;
    short num_channels;
    int   sample_rate;
    int   byte_rate;
    short sample_alignment;
    short bit_depth;
    char  data_header[4];
    int   data_bytes;
} wav_header_t;
#pragma pack(pop)

WAVFileReader::WAVFileReader(const char *file_name)
  : m_bytes_read(0)
{
    if (!SPIFFS.exists(file_name)) {
        Serial.println("ERROR: WAV file not found!");
        return;
    }
    m_file = SPIFFS.open(file_name, "r");
    wav_header_t hdr;
    m_file.read((uint8_t *)&hdr, sizeof(hdr));

    m_num_channels = hdr.num_channels;
    m_sample_rate = hdr.sample_rate;
    m_data_bytes  = hdr.data_bytes;

    // Seek to first data byte
    m_file.seek(sizeof(hdr));
}

WAVFileReader::~WAVFileReader() {
    if (m_file) m_file.close();
}

void WAVFileReader::getFrames(Frame_t *frames, int number_frames) {
    for (int i = 0; i < number_frames; i++) {
        // if we've read all data, pad with silence
        if (m_bytes_read >= m_data_bytes) {
            frames[i].left  = 0;
            frames[i].right = 0;
            continue;
        }
        // read left
        m_file.read((uint8_t *)&frames[i].left, sizeof(int16_t));
        m_bytes_read += sizeof(int16_t);

        if (m_num_channels == 1) {
            frames[i].right = frames[i].left;
        } else {
            m_file.read((uint8_t *)&frames[i].right, sizeof(int16_t));
            m_bytes_read += sizeof(int16_t);
        }
    }
}
