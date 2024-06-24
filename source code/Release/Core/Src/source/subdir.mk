################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/source/eeprom.c \
../Core/Src/source/fonts.c \
../Core/Src/source/ssd1306.c 

OBJS += \
./Core/Src/source/eeprom.o \
./Core/Src/source/fonts.o \
./Core/Src/source/ssd1306.o 

C_DEPS += \
./Core/Src/source/eeprom.d \
./Core/Src/source/fonts.d \
./Core/Src/source/ssd1306.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/source/%.o Core/Src/source/%.su Core/Src/source/%.cyclo: ../Core/Src/source/%.c Core/Src/source/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-source

clean-Core-2f-Src-2f-source:
	-$(RM) ./Core/Src/source/eeprom.cyclo ./Core/Src/source/eeprom.d ./Core/Src/source/eeprom.o ./Core/Src/source/eeprom.su ./Core/Src/source/fonts.cyclo ./Core/Src/source/fonts.d ./Core/Src/source/fonts.o ./Core/Src/source/fonts.su ./Core/Src/source/ssd1306.cyclo ./Core/Src/source/ssd1306.d ./Core/Src/source/ssd1306.o ./Core/Src/source/ssd1306.su

.PHONY: clean-Core-2f-Src-2f-source

