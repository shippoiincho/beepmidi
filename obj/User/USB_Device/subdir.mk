################################################################################
# MRS Version: {"version":"1.8.5","date":"2023/05/22"}
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../User/USB_Device/ch32v20x_usbfs_device.c \
../User/USB_Device/usb_desc.c 

OBJS += \
./User/USB_Device/ch32v20x_usbfs_device.o \
./User/USB_Device/usb_desc.o 

C_DEPS += \
./User/USB_Device/ch32v20x_usbfs_device.d \
./User/USB_Device/usb_desc.d 


# Each subdirectory must supply rules for building sources it contributes
User/USB_Device/%.o: ../User/USB_Device/%.c
	@	@	riscv-none-embed-gcc -march=rv32ecxw -mabi=ilp32e -msmall-data-limit=0 -msave-restore -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized  -g -I"C:\MRS_DATA\workspace\BeepMidi\Debug" -I"C:\MRS_DATA\workspace\BeepMidi\Core" -I"C:\MRS_DATA\workspace\BeepMidi\User" -I"C:\MRS_DATA\workspace\BeepMidi\Peripheral\inc" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@	@

