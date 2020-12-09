################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
drivers/pinout.obj: C:/ti/fir/examples/boards/ek-tm4c129exl/drivers/pinout.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: ARM Compiler'
	"C:/ti/ccs1011/ccs/tools/compiler/ti-cgt-arm_20.2.1.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=FPv4SPD16 -me -O2 --include_path="C:/Users/migue/Codigos/CCS/proyecto" --include_path="C:/Users/migue/Codigos/CCS/proyecto" --include_path="C:/ti/fir/examples/boards/ek-tm4c129exl" --include_path="C:/ti/fir" --include_path="C:/ti/fir/third_party/lwip-1.4.1/src/include" --include_path="C:/ti/fir/third_party/lwip-1.4.1/src/include/ipv4" --include_path="C:/ti/fir/third_party/lwip-1.4.1/apps" --include_path="C:/ti/fir/third_party/lwip-1.4.1/ports/tiva-tm4c129/include" --include_path="C:/ti/fir/third_party" --include_path="C:/ti/ccs1011/ccs/tools/compiler/ti-cgt-arm_20.2.1.LTS/include" --define=ccs="ccs" --define=PART_TM4C129ENCPDT --define=TARGET_IS_TM4C129_RA2 --define=LWIP_1_4_1 --define=USE_HTTP_1_0 --define=UART_BUFFERED -g --gcc --diag_warning=225 --diag_wrap=off --display_error_number --gen_func_subsections=on --abi=eabi --ual --preproc_with_compile --preproc_dependency="drivers/$(basename $(<F)).d_raw" --obj_directory="drivers" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


