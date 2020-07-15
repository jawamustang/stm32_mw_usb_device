# Middleware USB Device MCU Component

## Overview

**STM32Cube** is an STMicroelectronics original initiative to ease the developers life by reducing efforts, time and cost.

**STM32Cube** covers the overall STM32 products portfolio. It includes a comprehensive embedded software platform, delivered for each STM32 series.
   * The CMSIS modules (core and device) corresponding to the ARM(tm) core implemented in this STM32 product.
   * The STM32 HAL-LL drivers : an abstraction drivers layer, the API ensuring maximized portability across the STM32 portfolio.
   * The BSP Drivers of each evaluation or demonstration board provided by this STM32 series.
   * A consistent set of middlewares components such as RTOS, USB, FatFS, Graphics, STM32_TouchSensing_Library...
   * A full set of software projects (basic examples, applications or demonstrations) for each board provided by this STM32 series.

Two models of publication are proposed for the STM32Cube embedded software:
   * The monolithic **MCU Package** : all STM32Cube software modules of one STM32 series are present (Drivers, Middlewares, Projects, Utilities) in the repo (usual name **STM32Cubexx**, xx corresponding to the STM32 series).
   * The **MCU component** : progressively from November 2019, each STM32Cube software module being part of the STM32Cube MCU Package, will be delivered as an individual repo, allowing the user to select and get only the required software functions.

## Description

This **stm32_mw_usb_device** MCU component repository is one element **common to all** STM32Cube MCU embedded software packages, providing the **USB Device MCU Middleware** part.

## License

Copyright (c) 2015 STMicroelectronics.

This software component is licensed by  ST under Ultimate Liberty license SLA0044, the "License". You may not use this file except in compliance with this license. You may obtain a copy of the license [here](https://www.st.com/SLA0044).

## Release note

Details about the content of this release are available in the release note [here](https://htmlpreview.github.io/?https://github.com/STMicroelectronics/stm32_mw_usb_device/blob/master/Release_Notes.html).

## Compatibility information

This table shows the correspondence between the USB Device MW version and the corresponding HAL version of the targeted series. It is **crucial** that you use a consistent set of versions for the MW - HAL, as mentioned in this table.

Note that:
* in case a series does not support a particular tag, it is not mentioned in front of it.
* in case a series supports a recent tag, it is not mentioned in front of older ones.

USB Device | HAL |
---------- | ---------- |
Tag v2.5.1 | Tag v1.10.2 ([stm32l0xx_hal_driver](https://github.com/STMicroelectronics/stm32l0xx_hal_driver))
Tag v2.5.2 | Tag v1.4.0 ([stm32l1xx_hal_driver](https://github.com/STMicroelectronics/stm32l1xx_hal_driver))
Tag v2.5.3 | Tag v1.7.3 ([stm32f0xx_hal_driver](https://github.com/STMicroelectronics/stm32f0xx_hal_driver))<br>Tag v1.1.4 ([stm32f1xx_hal_driver](https://github.com/STMicroelectronics/stm32f1xx_hal_driver))<br>Tag v1.2.4 ([stm32f2xx_hal_driver](https://github.com/STMicroelectronics/stm32f2xx_hal_driver))<br>Tag v1.5.3 ([stm32f3xx_hal_driver](https://github.com/STMicroelectronics/stm32f3xx_hal_driver))

## Troubleshooting

If you have any issue with the **software content** of this repository, you can file an issue [here](https://github.com/STMicroelectronics/stm32_mw_usb_device/issues/new/choose).

For any other question related to the product, the tools, the environment, you can submit a topic to the [ST Community](https://community.st.com/s/).
