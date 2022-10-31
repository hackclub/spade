pico_sdk_init()
add_definitions(-DSPADE_EMBEDDED)

add_library(Piano ../piano.c)
target_compile_definitions(Piano PRIVATE
        # compile time configuration of I2S
        PICO_AUDIO_I2S_MONO_INPUT=1
        #define for our example code
        USE_AUDIO_I2S=1
        PICO_AUDIO_I2S_DATA_PIN=9
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=10
#            PICO_DEFAULT_UART=0
#            PICO_DEFAULT_UART_TX_PIN=28
#            PICO_DEFAULT_UART_RX_PIN=29
        )
target_link_libraries(Piano PRIVATE
  pico_stdlib
  pico_audio_i2s
)

target_link_libraries(spade
  Piano
  pico_stdlib
  pico_multicore
  hardware_spi
  hardware_timer
)

pico_enable_stdio_usb(spade 1)
pico_enable_stdio_uart(spade 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(spade)

