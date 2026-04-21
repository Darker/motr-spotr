#include <nucleo_main/generated/generated_user_steps.h>
#include <Inc/main.h>

constexpr size_t ADC_BUFFER_LEN = 1024;
static uint32_t adc_buffer[ADC_BUFFER_LEN];

volatile uint8_t adc_half_ready = 0;
volatile uint8_t adc_full_ready = 0;

static uint32_t halves_read = 0;
static uint32_t fulls_read = 0;

extern "C" {
    void main_post_setup(ADC_HandleTypeDef* hadc1, DMA_HandleTypeDef* hdma_adc1, TIM_HandleTypeDef* htim2)
    {
        HAL_TIM_Base_Start(htim2);
        HAL_ADC_Start_DMA(hadc1, (uint32_t*)adc_buffer, ADC_BUFFER_LEN);
    }

    void main_pre_setup(ADC_HandleTypeDef* hadc1, DMA_HandleTypeDef* hdma_adc1, TIM_HandleTypeDef* htim2)
    {

    }

    void user_loop_main(ADC_HandleTypeDef* hadc1, DMA_HandleTypeDef* hdma_adc1, TIM_HandleTypeDef* htim2)
    {
        if(adc_half_ready)
        {
            adc_half_ready = 0;
            halves_read += 1;
        }
        if(adc_full_ready)
        {
            adc_full_ready = 0;
            fulls_read += 1;
        }
    }

    void ADC1_Init_user_0() {}
    void ADC1_Init_user_1() {}
    void ADC1_Init_user_2() {}
    void TIM2_Init_user_0() {}
    void TIM2_Init_user_1() {}
    void TIM2_Init_user_2() {}

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