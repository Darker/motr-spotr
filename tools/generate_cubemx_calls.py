import re
import sys
from pathlib import Path
from typing import Optional, Protocol

PROJECT_ROOT = Path(__file__).parent.parent
# nucleo_adc\stm32cubemx\nucleo_settings\USB_DEVICE\App\usbd_cdc_if.c
CUBE_MX_MAIN = PROJECT_ROOT / "nucleo_adc/stm32cubemx/nucleo_settings/Core/Src/main.c"
CUBE_MX_USB = PROJECT_ROOT / "nucleo_adc/stm32cubemx/nucleo_settings/USB_DEVICE/App/usbd_cdc_if.c"
GENERATED_DECLS = PROJECT_ROOT / "nucleo_adc/nucleo_main/include/nucleo_main/generated"

GENERATED_FILENAME = "generated_user_steps.h"
GENERATED_FILENAME_USB = "generated_user_usb_steps.h"

USER_BEGIN_RE = re.compile(
    r"/\*\s*USER CODE (BEGIN|END)\s+([A-Za-z0-9_]+)\s+([0-9]*)\s*\*/"
)

class CubeMXUnknownTagHandler(Protocol):
    def __call__(
        self,
        tag_name: str,
        tag_step: str,
        prototypes: list[str],
        current_tag: Optional[str],
        cur_line: int,
    ) -> tuple[Optional[str], Optional[str]]:
        ...


def add_current_tag(cur_tag: Optional[str], value: Optional[str], line: str, idx: int, new_src: list[str]):
    # if cur_tag is None:
    #     new_src.append(line)
    #     return True
    mline = USER_BEGIN_RE.search(line)
    if mline:
        
        code_mode = mline.group(1)
        end_tag = mline.group(2)

        print(f"#{idx:04d} {code_mode} {end_tag} (expected END {cur_tag})")
        
        assert code_mode == "END", f"Unexpected begin {code_mode} of {end_tag} while looking for end of {cur_tag}"
        assert end_tag == cur_tag, f"Unexpected end of {end_tag} while looking for end of {cur_tag}"
        new_src.append("")
        return True

    if line == value:
        return False
    if new_src[len(new_src) - 1] == value:
        return False
    assert value is not None
    new_src.append(value)
    return False

# ADC_HandleTypeDef hadc1;
# DMA_HandleTypeDef hdma_adc1;

# TIM_HandleTypeDef htim2;
MAIN_FUNC_ARGDEF = "ADC_HandleTypeDef* hadc1, DMA_HandleTypeDef* hdma_adc1, TIM_HandleTypeDef* htim2"
MAIN_FUNC_ARGCALL = "&hadc1, &hdma_adc1, &htim2"

def process_file(src_path: Path, header_path: Path, tag_handler: CubeMXUnknownTagHandler, includes: list[str]):
    src = src_path.read_text().splitlines()
    prototypes = []
    new_src = []

    current_tag: Optional[str] = None
    current_tag_value: Optional[str] = None
    
    for i, line in enumerate(src):
        if current_tag is not None:
            reset_tag = add_current_tag(current_tag, current_tag_value, line, i, new_src)
            if not reset_tag:
                continue

        current_tag = None
        current_tag_value = None

        m = USER_BEGIN_RE.search(line)
        new_src.append(line)
        if not m:
            continue
        
        if m.group(1) == "END":
            continue

        tag = m.group(2)  # e.g. ADC1_Init
        tag_step = m.group(3)

        if len(tag_step) > 0:
            print(f"#{i:04d} OPEN {tag} ({tag_step})")
            func = f"{tag}_user_{tag_step}"

            # Add prototype
            prototypes.append(f"void {func}();")
            current_tag = tag
            current_tag_value = f"    {func}();  // auto-inserted"
        else:
            print(f"#{i:04d} OPEN {tag} (custom_step)")
            (current_tag, current_tag_value) = tag_handler(tag, tag_step, prototypes, current_tag, i)
        
        if current_tag is None:
            print(f"#{i:04d} CLOSE {tag} (unused)")


    # Write modified source
    src_path.write_text("\n".join(new_src))

    include_calls: list[str] = []
    for include_path in includes:
        if include_path.startswith("\"") or include_path.startswith("<"):
            include_calls.append(f"#include {include_path}")
        else:
            include_calls.append(f"#include \"{include_path}\"")
    include_str = "\n".join(include_calls)
    # Write header
    header_path.write_text(
        "#pragma once\n\n" +
        f"{include_str}\n\n" +
        "#ifdef __cplusplus\n" +
        "extern \"C\" {\n" +
        "#endif\n  "+ 
        "\n  ".join(prototypes) +

        "\n#ifdef __cplusplus\n}\n#endif"
    )



def cube_mx_main_unknown_tag_hanlder(tag_name: str, tag_step: str, prototypes: list[str], current_tag: Optional[str], cur_line: int):
    current_tag, current_tag_value = (None, None)
    if tag_name == "Includes":
        print(f"#{cur_line:04d} OPEN {tag_name} ({tag_step})")
        current_tag = "Includes"
        current_tag_value = f"#include \"nucleo_main/generated/{GENERATED_FILENAME}\""
    elif tag_name == "1":
        current_tag = "1"
        current_tag_value = f"    main_pre_setup({MAIN_FUNC_ARGCALL});  // auto-inserted"
        prototypes.append(f"void main_pre_setup({MAIN_FUNC_ARGDEF});")
    elif tag_name == "2":
        current_tag = "2"
        current_tag_value = f"    main_post_setup({MAIN_FUNC_ARGCALL});  // auto-inserted"
        prototypes.append(f"void main_post_setup({MAIN_FUNC_ARGDEF});")
    elif tag_name == "WHILE":
        current_tag = "WHILE"
        current_tag_value = f"    while(1) user_loop_main({MAIN_FUNC_ARGCALL});"
        prototypes.append(f"void user_loop_main({MAIN_FUNC_ARGDEF});")
    elif tag_name == "3":
        current_tag = "3"
        current_tag_value = f"    // while loop reformated"
    elif tag_name == "MX_GPIO_Init_2":
        current_tag = tag_name
        current_tag_value = f"    user_gpio_init();"
        prototypes.append(f"void user_gpio_init();")

    return (current_tag, current_tag_value)

USB_CMD_FUNC_ARGDEF = "uint8_t cmd, uint8_t* pbuf, uint16_t length, USBD_HandleTypeDef* device"
USB_CMD_FUNC_ARGCALL = "cmd, pbuf, length, &hUsbDeviceFS"

def cube_mx_usb_tag_handler(tag_name: str, tag_step: str, prototypes: list[str], current_tag: Optional[str], cur_line: int):
    current_tag, current_tag_value = (None, None)
    if tag_name == "INCLUDE":
        print(f"#{cur_line:04d} OPEN {tag_name} ({tag_step})")
        current_tag = "INCLUDE"
        current_tag_value = f"#include \"nucleo_main/generated/{GENERATED_FILENAME_USB}\""
    elif tag_name == "5":
        current_tag = "5"
        current_tag_value = f"    return CDC_Control_FS_user({USB_CMD_FUNC_ARGCALL});  // auto-inserted"
        prototypes.append(f"int8_t CDC_Control_FS_user({USB_CMD_FUNC_ARGDEF});")

    return (current_tag, current_tag_value)


if __name__ == "__main__":

    process_file(CUBE_MX_MAIN, GENERATED_DECLS / GENERATED_FILENAME, cube_mx_main_unknown_tag_hanlder, ["<Inc/main.h>"])

    process_file(CUBE_MX_USB, GENERATED_DECLS / GENERATED_FILENAME_USB, cube_mx_usb_tag_handler, ["\"usbd_def.h\""])