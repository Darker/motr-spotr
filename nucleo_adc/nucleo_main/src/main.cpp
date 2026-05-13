#include <nucleo_shared/adc_stream_constants.h>
#include <nucleo_shared/SimpleQueue.h>
#include <Inc/main.h>

#include <nucleo_main/generated/generated_user_steps.h>
#include <nucleo_main/generated/generated_user_usb_steps.h>
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

extern "C" 
{
    extern USBD_HandleTypeDef hUsbDeviceFS;
}

static uint16_t adc_buffer[ADC_BUFFER_LEN];
volatile uint8_t adc_half_ready = 0;
volatile uint8_t adc_full_ready = 0;

static uint32_t halves_read = 0;
static uint32_t fulls_read = 0;

volatile bool usb_connected;

void pack12to3(const uint16_t *in, size_t count, uint8_t *out)
{
    size_t o = 0;

    for (size_t i = 0; i < count; i += 2)
    {
        uint16_t a = in[i] & 0x0FFF;
        uint16_t b = (i + 1 < count) ? (in[i + 1] & 0x0FFF) : 0;

        out[o+0] = (uint8_t)(a & 0xFF);                     // A[7:0]
        out[o+1] = (uint8_t)((a >> 8) | ((b & 0x0F) << 4)); // A[11:8] + B[3:0]
        out[o+2] = (uint8_t)(b >> 4);                       // B[11:4]


        o += 3;
    }
}

static inline bool usb_is_busy(void)
{
    USBD_CDC_HandleTypeDef* hcdc =
        (USBD_CDC_HandleTypeDef*) hUsbDeviceFS.pClassData;

    return (hcdc->TxState != 0);
}

constexpr size_t USB_QUEUE_SIZE = 5;
// static uint8_t usb_buffer[USB_QUEUE_SIZE][USB_BUFFER_LEN];
// volatile size_t curBuferPos = 0;

enum class UsbState
{
    PENDING,
    SENDING,
    SENT
};
struct UsbChunk
{
    uint8_t data[USB_BUFFER_LEN];
    UsbState state;
};

static SimpleQueue<UsbChunk, USB_QUEUE_SIZE> usbQueue;

static void writeToQueue(const uint16_t* data, size_t dataLen, uint8_t* curBuffer)
{
    memcpy(curBuffer, MAGIC_BEGIN, sizeof(MAGIC_BEGIN));
    //constexpr const uint32_t data_len = ADC_PACKED_CHUNK_LEN + sizeof(MAGIC_END);
    // for now, I'll just write zeros and rely on magic only
    curBuffer[HEADER_OFFSET_DATA_LEN+0] = 0;
    curBuffer[HEADER_OFFSET_DATA_LEN+1] = 0;
    curBuffer[HEADER_OFFSET_DATA_LEN+2] = 0;
    curBuffer[HEADER_OFFSET_DATA_LEN+3] = 0;

    pack12to3(data, dataLen, curBuffer + HEADER_OFFSET_DATA);
    memcpy(curBuffer + HEADER_OFFSET_MAGIC_END, MAGIC_END, sizeof(MAGIC_END));

    // size_t total_len = HEADER_OFFSET_MAGIC_END + sizeof(MAGIC_END);
}

static bool pushBufferToQueue(const uint16_t* data, size_t dataLen, bool mock = false)
{
    if(usbQueue.isFull())
    {
        return false;
    }

    UsbChunk* chunk = usbQueue.pushNext();
    chunk->state = UsbState::PENDING;
    writeToQueue(data, dataLen, chunk->data);
    return true;
}

static void sendFromQueue()
{
    if(usbQueue.size() == 0)
    {
        return;
    }
    if(usbQueue.size() == 1 && usbQueue.at(0).state == UsbState::SENDING)
    {
        if(!usb_is_busy())
        {
            usbQueue.popOldest();
        }
        return;
    }
    if(!usb_is_busy())
    {
        if(usbQueue.at(0).state == UsbState::SENDING)
        {
            usbQueue.popOldest();
        }
        uint8_t result = CDC_Transmit_FS(usbQueue.at(0).data, USB_BUFFER_LEN);
        if(result == USBD_OK)
        {
            usbQueue.at(0).state = UsbState::SENDING;
        }
    }
}

extern "C"
{
    int8_t CDC_Control_FS_user(uint8_t cmd, uint8_t *pbuf, uint16_t length,
                               USBD_HandleTypeDef *device)
    {
        switch (cmd)
        {
        case CDC_SEND_ENCAPSULATED_COMMAND:

            break;

        case CDC_GET_ENCAPSULATED_RESPONSE:

            break;

        case CDC_SET_COMM_FEATURE:

            break;

        case CDC_GET_COMM_FEATURE:

            break;

        case CDC_CLEAR_COMM_FEATURE:

            break;

            /*******************************************************************************/
            /* Line Coding Structure */
            /*-----------------------------------------------------------------------------*/
            /* Offset | Field       | Size | Value  | Description */
            /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits
             * per second*/
            /* 4      | bCharFormat |   1  | Number | Stop bits */
            /*                                        0 - 1 Stop bit */
            /*                                        1 - 1.5 Stop bits */
            /*                                        2 - 2 Stop bits */
            /* 5      | bParityType |  1   | Number | Parity */
            /*                                        0 - None */
            /*                                        1 - Odd */
            /*                                        2 - Even */
            /*                                        3 - Mark */
            /*                                        4 - Space */
            /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or
             * 16).          */
            /*******************************************************************************/
        case CDC_SET_LINE_CODING:

            break;

        case CDC_GET_LINE_CODING:

            break;

        case CDC_SET_CONTROL_LINE_STATE:
            if (pbuf[0] & 0x01) {
                usb_connected = true;   // COM port opened
            } else {
                usb_connected = false;   // COM port closed
            }
            break;

        case CDC_SEND_BREAK:

            break;

        default:
            break;
        }

        return (USBD_OK);
    }

    void user_gpio_init()
    {
        // GPIO_InitTypeDef GPIO_InitStruct = {0};
        // GPIO_InitStruct.Pin = GPIO_PIN_4;
        // GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        // GPIO_InitStruct.Pull = GPIO_NOPULL;
        // HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    }

    void main_post_setup(ADC_HandleTypeDef *hadc1, DMA_HandleTypeDef *hdma_adc1,
                         TIM_HandleTypeDef *htim2)
    {
        // HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); // turn LED on
        // HAL_Delay(200);
        // HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // turn LED off
        // HAL_Delay(200);
        // HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); // on again

        HAL_TIM_Base_Start(htim2);
        HAL_ADC_Start_DMA(hadc1, (uint32_t*)adc_buffer, ADC_BUFFER_LEN);
    }

    void main_pre_setup(ADC_HandleTypeDef *hadc1, DMA_HandleTypeDef *hdma_adc1,
                        TIM_HandleTypeDef *htim2)
    {
    }

    void user_loop_main(ADC_HandleTypeDef *hadc1, DMA_HandleTypeDef *hdma_adc1,
                        TIM_HandleTypeDef *htim2)
    {
        while (true)
        {
            static uint32_t counter = 0;

            if (++counter >= 500000)
            { // adjust for blink speed
                counter = 0;
                if(usb_connected)
                {
                    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5); // blink LED

                    // check if still connected:
                    if(hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
                        usb_connected = false;
                    }
                }
                else
                {
                    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); // blink LED
                }
            }

            if (adc_half_ready)
            {
                adc_half_ready = 0;
                halves_read += 1;
                if(usb_connected)
                {
                    pushBufferToQueue(adc_buffer, ADC_BUFFER_HALF_SIZE_SAMPLES);
                }
            }
            if (adc_full_ready)
            {
                adc_full_ready = 0;
                fulls_read += 1;
                if(usb_connected)
                {
                    pushBufferToQueue(adc_buffer + ADC_BUFFER_HALF_SIZE_SAMPLES, ADC_BUFFER_HALF_SIZE_SAMPLES);
                }
            }
            sendFromQueue();

            //__WFI();
            //__WFE();
        }
    }

    void ADC1_Init_user_0()
    {
    }
    void ADC1_Init_user_1()
    {
    }
    void ADC1_Init_user_2()
    {
    }
    void TIM2_Init_user_0()
    {
    }
    void TIM2_Init_user_1()
    {
    }
    void TIM2_Init_user_2()
    {
    }

    void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
    {
        if (hadc->Instance == ADC1)
        {
            adc_half_ready = 1;
        }
    }

    void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
    {
        if (hadc->Instance == ADC1)
        {
            adc_full_ready = 1;
        }
    }
}

// /**
//   * @brief  Manage the CDC class requests
//   * @param  cmd: Command code
//   * @param  pbuf: Buffer containing command data (request parameters)
//   * @param  length: Number of data to be sent (in bytes)
//   * @retval Result of the operation: USBD_OK if all operations are OK else
//   USBD_FAIL
//   */
// static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
// {
//   /* USER CODE BEGIN 5 */
//   switch(cmd)
//   {
//     case CDC_SEND_ENCAPSULATED_COMMAND:

//     break;

//     case CDC_GET_ENCAPSULATED_RESPONSE:

//     break;

//     case CDC_SET_COMM_FEATURE:

//     break;

//     case CDC_GET_COMM_FEATURE:

//     break;

//     case CDC_CLEAR_COMM_FEATURE:

//     break;

//   /*******************************************************************************/
//   /* Line Coding Structure */
//   /*-----------------------------------------------------------------------------*/
//   /* Offset | Field       | Size | Value  | Description */
//   /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per
//   second*/
//   /* 4      | bCharFormat |   1  | Number | Stop bits */
//   /*                                        0 - 1 Stop bit */
//   /*                                        1 - 1.5 Stop bits */
//   /*                                        2 - 2 Stop bits */
//   /* 5      | bParityType |  1   | Number | Parity */
//   /*                                        0 - None */
//   /*                                        1 - Odd */
//   /*                                        2 - Even */
//   /*                                        3 - Mark */
//   /*                                        4 - Space */
//   /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16). */
//   /*******************************************************************************/
//     case CDC_SET_LINE_CODING:

//     break;

//     case CDC_GET_LINE_CODING:

//     break;

//     case CDC_SET_CONTROL_LINE_STATE:

//     break;

//     case CDC_SEND_BREAK:

//     break;

//   default:
//     break;
//   }

//   return (USBD_OK);
//   /* USER CODE END 5 */
// }