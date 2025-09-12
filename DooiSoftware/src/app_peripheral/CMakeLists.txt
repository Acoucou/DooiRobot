zephyr_include_directories(./)

zephyr_library_sources(
    servo.c
    pca9635.c
    led.c
    motor.c
    app_peripherals.c
    soc_adc.c
    key.c
    action_sequences.c
    nvs.c
)
