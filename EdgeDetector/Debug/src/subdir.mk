################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/EdgeDetector.c 

SRC_OBJS += \
./src/EdgeDetector.doj 

C_DEPS += \
./src/EdgeDetector.d 


# Each subdirectory must supply rules for building sources it contributes
src/EdgeDetector.doj: ../src/EdgeDetector.c
	@echo 'Building file: $<'
	@echo 'Invoking: CrossCore SHARC C/C++ Compiler'
	cc21k -c -file-attr ProjectName="EdgeDetector" -proc ADSP-21489 -flags-compiler --no_wrap_diagnostics -si-revision 0.2 -g -DCORE0 -DDO_CYCLE_COUNTS -D_DEBUG @includes-79581eea2df656c42b24f2c971cfdd96.txt -structs-do-not-overlap -no-const-strings -no-multiline -warn-protos -double-size-32 -swc -gnu-style-dependencies -MD -Mo "src/EdgeDetector.d" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


