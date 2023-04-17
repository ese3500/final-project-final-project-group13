#ifndef ADCDUINO_H
#define ADCDUINO_H

#include "stringify.h"

#include <../deprecated/adc1_private.h>
#include <../deprecated/driver/adc.h>
#include </Users/willsturgeon/esp/esp-idf/components/driver/deprecated/driver/adc.h>
#include <driver/rtc_io.h>
#include <esp_log.h>
#include <hal/adc_ll.h>
#include <hal/gpio_hal.h>
#include <soc/adc_channel.h>
#include <soc/io_mux_reg.h>
#include <soc/rtc_io_channel.h>
#include <soc/rtc_io_periph.h>
#include <soc/rtc_io_reg.h>
#include <soc/sens_reg.h>

#include <inttypes.h>
#include <stdint.h>

#define ADC_ATTENUATION 3
#define ADC_CLK_DIV 1
#define ADC_BIT_WIDTH 12
#define ADC_PIN 36
#define ADC_CHANNEL PASTE(PASTE(ADC1_GPIO, ADC_PIN), _CHANNEL)
#define ADC_IO_REG PASTE(PASTE(IO_MUX_GPIO, ADC_PIN), _REG)

#define RTC_IO_CHANNEL PASTE(PASTE(RTCIO_GPIO, ADC_PIN), _CHANNEL)

typedef enum {
  ADC_0db,
  ADC_2_5db,
  ADC_6db,
  ADC_11db,
  ADC_ATTENDB_MAX
} adc_attenuation_t;

extern rtc_io_desc_t const rtc_io_desc[SOC_RTCIO_PIN_COUNT];

__attribute__((always_inline)) inline static volatile uint32_t force_32b_read(uint32_t const volatile* const restrict reg, uint32_t mask) {
  volatile uint32_t v32b = *reg;
  return v32b & mask;
}

__attribute__((always_inline)) inline static void dropin_adc1_config_width(void) {
  REG(SENS_SAR_START_FORCE_REG) |= SENS_SAR1_BIT_WIDTH_M; // 0x3 => full 12 bits
  REG(SENS_SAR_READ_CTRL_REG) |= SENS_SAR1_SAMPLE_BIT_M;  // ditto ^
}

__attribute__((always_inline)) inline static void dropin_adc_set_clk_div(uint8_t d) {
  ESP_ERROR_CHECK(adc_set_clk_div(d));
}

__attribute__((always_inline)) inline static void dropin_rtc_gpio_deinit(void) {
  REG(rtc_io_desc[RTC_IO_CHANNEL].reg) &= ~rtc_io_desc[RTC_IO_CHANNEL].mux;
}

__attribute__((always_inline)) inline static void dropin_gpio_set_intr_type(void) {
  // gpio_set_intr_type(ADC_PIN, GPIO_INTR_DISABLE);

  // #define gpio_hal_set_intr_type(hal, gpio_num, intr_type) gpio_ll_set_intr_type((hal)->dev, gpio_num, intr_type)
  //   gpio_hal_set_intr_type(gpio_context.gpio_hal, ADC_PIN, GPIO_INTR_DISABLE);
  // gpio_ll_set_intr_type(gpio_context.gpio_hal->dev, ADC_PIN, GPIO_INTR_DISABLE);
  // gpio_context.gpio_hal->dev->pin[ADC_PIN].int_type = GPIO_INTR_DISABLE;
  REG(PASTE(PASTE(GPIO_PIN, ADC_PIN), _REG)) &= ~PASTE(PASTE(GPIO_PIN, ADC_PIN), _INT_TYPE_M);

  //   gpio_context.isr_clr_on_entry_mask &= ~(1ULL << ADC_PIN);
  //   gpio_hal_clear_intr_status_bit(gpio_context.gpio_hal, ADC_PIN);
  // #define gpio_hal_clear_intr_status_bit(hal, gpio_num) (((gpio_num) < 32) ? gpio_ll_clear_intr_status((hal)->dev, 1 << gpio_num) : gpio_ll_clear_intr_status_high((hal)->dev, 1 << (gpio_num - 32)))
  //   HAL_FORCE_MODIFY_U32_REG_FIELD(hw->status1_w1tc, intr_st, mask);
}

__attribute__((always_inline)) inline static void dropin_gpio_intr_disable(void) {
  REG(GPIO_PIN36_REG) &= ~GPIO_PIN36_INT_ENA_M;
#if (ADC_PIN < 32)
  REG(GPIO_STATUS_W1TC_REG) = (1ULL << ADC_PIN);
#else
  REG(GPIO_STATUS1_W1TC_REG) = (1ULL << (ADC_PIN - 32U));
#endif
}

__attribute__((always_inline)) inline static void dropin_gpio_hal_iomux_func_sel(void) {
  REG(ADC_IO_REG) &= ~MCU_SEL_M;
  REG(ADC_IO_REG) |= (PIN_FUNC_GPIO << MCU_SEL_S);
}

__attribute__((always_inline)) inline static void dropin_gpio_config(void) {
  dropin_rtc_gpio_deinit();
  dropin_gpio_set_intr_type();
  dropin_gpio_intr_disable();
  dropin_gpio_hal_iomux_func_sel();
}

__attribute__((always_inline)) inline static void dropin_adc1_config_channel_atten(void) {
  ESP_ERROR_CHECK(adc1_config_channel_atten(PASTE(PASTE(ADC1_GPIO, ADC_PIN), _CHANNEL), ADC_ATTENUATION));
}

__attribute__((always_inline)) inline static uint16_t dropin_adc1_get_raw(void) {
  // adc1_rtc_mode_acquire();

  adc_ll_hall_disable();                              // Disable other peripherals.
  adc_ll_amp_disable();                               // Currently the LNA is not open, close it by default.
  adc_ll_set_controller(ADC_UNIT_1, ADC_LL_CTRL_RTC); // Set controller
  adc_oneshot_ll_set_channel(ADC_UNIT_1, ADC_CHANNEL);

  REG(SENS_SAR_MEAS_START1_REG) &= ~SENS_SAR1_EN_PAD_M;
  REG(SENS_SAR_MEAS_START1_REG) |= (1 << (ADC_CHANNEL + SENS_SAR1_EN_PAD_S));
  do {
  } while (force_32b_read(SENS_SAR_SLAVE_ADDR1_REG, SENS_MEAS_STATUS_M)); // This register is completely absent from the manual--not even its memory address :_)
  REG(SENS_SAR_MEAS_START1_REG) &= ~SENS_MEAS1_START_SAR_M;
  REG(SENS_SAR_MEAS_START1_REG) |= SENS_MEAS1_START_SAR_M;

  do {
  } while (adc_oneshot_ll_get_event(ADC_LL_EVENT_ADC1_ONESHOT_DONE) != true);
  uint16_t adc_value = adc_oneshot_ll_get_raw_result(ADC_UNIT_1);
  adc_oneshot_ll_disable_all_unit();

  // adc1_lock_release();
  return adc_value;
}

uint16_t FUCK(void) {
  dropin_adc1_config_width();
  // dropin_adc_set_clk_div(ADC_CLK_DIV);
  dropin_gpio_config();
  dropin_adc1_config_channel_atten();
  // NOTE: you can apparently safely delete everything above this line, but I haven't tested after unplugging
  return dropin_adc1_get_raw();
}

#endif // ADCDUINO_H
