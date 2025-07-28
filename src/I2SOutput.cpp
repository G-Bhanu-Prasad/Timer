#include <Arduino.h>
#include "driver/i2s.h"
#include <math.h>
#include <SPIFFS.h>
#include <FS.h>

#include "SampleSource.h"
#include "I2SOutput.h"

// number of frames to try and send at once (a frame is a left and right sample)
// Reduced from 512 to 256 to conserve heap.
#define NUM_FRAMES_TO_SEND 256

void i2sWriterTask(void *param)
{
    I2SOutput *output = (I2SOutput *)param;
    int availableBytes = 0;
    int buffer_position = 0;
    Frame_t *frames = (Frame_t *)malloc(sizeof(Frame_t) * NUM_FRAMES_TO_SEND);
    if (!frames) {
        Serial.println("Failed to allocate frames buffer");
        vTaskDelete(NULL);
        return;
    }

    while (true)
    {
        // wait for some data to be requested
        i2s_event_t evt;
        if (xQueueReceive(output->m_i2sQueue, &evt, portMAX_DELAY) == pdPASS)
        {
            if (evt.type == I2S_EVENT_TX_DONE)
            {
                size_t bytesWritten = 0;
                do
                {
                    if (availableBytes == 0)
                    {
                        // Get a block of frames from the sample source
                        output->m_sample_generator->getFrames(frames, NUM_FRAMES_TO_SEND);

                        // 🔊 Volume Boost: multiply samples by gain and clamp
                        float gain = 2.5f;  // Adjust gain between 1.5 to 2.5 safely.
                        for (int i = 0; i < NUM_FRAMES_TO_SEND; i++) {
                            int32_t left = frames[i].left * gain;
                            int32_t right = frames[i].right * gain;

                            // Clamp to 16-bit range
                            if (left > 32767) left = 32767;
                            if (left < -32768) left = -32768;
                            if (right > 32767) right = 32767;
                            if (right < -32768) right = -32768;

                            frames[i].left = left;
                            frames[i].right = right;
                        }

                        availableBytes = NUM_FRAMES_TO_SEND * sizeof(uint32_t);
                        buffer_position = 0;
                    }

                    // Write available data to the I2S peripheral if any
                    if (availableBytes > 0)
                    {
                        i2s_write(output->m_i2sPort, (uint8_t *)frames + buffer_position,
                                  availableBytes, &bytesWritten, portMAX_DELAY);
                        availableBytes -= bytesWritten;
                        buffer_position += bytesWritten;
                    }
                } while (bytesWritten > 0);
            }
        }
    }
}

void I2SOutput::start(i2s_port_t i2sPort, i2s_pin_config_t &i2sPins, SampleSource *sample_generator)
{
    m_sample_generator = sample_generator;
    
    // Print free heap for debugging DMA-capable memory availability
    Serial.printf("Free heap before I2S install: %u bytes\n", ESP.getFreeHeap());
    
    // i2s config for writing both channels of I2S
    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = m_sample_generator->sampleRate(),
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 256  // Reduced dma buffer size from 1024 to 256 bytes per buffer
    };

    m_i2sPort = i2sPort;
    // Install and start i2s driver, check for errors:
    esp_err_t err = i2s_driver_install(m_i2sPort, &i2sConfig, 4, &m_i2sQueue);
    if (err != ESP_OK) {
        Serial.printf("Failed to install I2S driver: %s\n", esp_err_to_name(err));
        return;
    }
    // Set up the I2S pins
    i2s_set_pin(m_i2sPort, &i2sPins);
    // Clear the DMA buffers
    i2s_zero_dma_buffer(m_i2sPort);
    // Start a task to write samples to the I2S peripheral
    TaskHandle_t writerTaskHandle;
    xTaskCreate(i2sWriterTask, "i2s Writer Task", 4096, this, 1, &writerTaskHandle);
}

void I2SOutput::stop() {
    // Uninstall the I2S driver; this also kills the writer task.
    i2s_driver_uninstall(m_i2sPort);
    // Optionally, you could also vTaskDelete(m_i2sWriterTaskHandle) if you had stored the handle.
}
