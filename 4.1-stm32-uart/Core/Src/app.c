#include "app.h"
#include "main.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* UART wired to the ESP32: USART1, PA9 TX / PA10 RX. */
#define LINK_UART       (&huart1)

/* Command line length including the terminating zero. */
#define UART_LINE_MAX   96

/* KEY sampling period, longer than the contact bounce. */
#define KEY_SAMPLE_MS   20U

/* Byte the HAL stores the next received character in. */
static uint8_t g_rx_byte;
/* Line collected up to the newline character. */
static char g_rx_line[UART_LINE_MAX];
static uint8_t g_rx_len;
/* 1 - a line is ready, the main loop will take it. */
static volatile uint8_t g_line_ready;

/* State of the LED on the ESP32 side that this board asks for with KEY. */
static uint8_t g_esp_led_on;
static GPIO_PinState g_key_last = GPIO_PIN_SET;
static uint32_t g_key_tick;

/* One string to the UART, no line ending. Transmit timeout 100 ms. */
static void uart_send(const char *s)
{
  HAL_UART_Transmit(LINK_UART, (uint8_t *)s, (uint16_t)strlen(s), 100);
}

/* Reply line: text and CR LF, as the master expects. */
static void uart_send_line(const char *s)
{
  uart_send(s);
  uart_send("\r\n");
}

/* On-board LED on PC13 is active low. */
static void led_set(uint8_t on)
{
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static uint8_t led_is_on(void)
{
  return HAL_GPIO_ReadPin(LED_GPIO_Port, LED_Pin) == GPIO_PIN_RESET;
}

/* Command letters to upper case so that "ping" and "PING" match. */
static void str_up(char *s)
{
  for (; *s; s++) {
    *s = (char)toupper((unsigned char)*s);
  }
}

/* After reset the slave reports on its own that it is alive. */
static void send_banner(void)
{
  uart_send_line("INFO STM32F411 slave started");
  uart_send_line("INFO commands:");
  uart_send_line("INFO   LED ON | LED OFF | LED TOGGLE");
  uart_send_line("INFO   TIME");
  uart_send_line("INFO   STATUS");
  uart_send_line("INFO   PING");
  uart_send_line("INFO   HELP");
  uart_send_line("INFO KEY button toggles the LED on the master");
  uart_send_line("OK READY");
}

/* Parses one line. The reply is always one line, OK or ERR. */
static void handle_line(char *line)
{
  char cmd[16] = {0};
  char a1[16] = {0};
  int n;

  str_up(line);

  /* First word is the command, second is the argument (for LED). */
  n = sscanf(line, "%15s %15s", cmd, a1);
  if (n < 1) {
    uart_send_line("ERR empty");
    return;
  }

  if (strcmp(cmd, "PING") == 0) {
    uart_send_line("OK PONG");
    return;
  }
  if (strcmp(cmd, "HELP") == 0) {
    uart_send_line("OK LED ON|OFF|TOGGLE | TIME | STATUS | PING | HELP");
    return;
  }
  if (strcmp(cmd, "TIME") == 0) {
    char out[UART_LINE_MAX];
    uint32_t ms = HAL_GetTick();
    uint32_t s = ms / 1000U;
    uint32_t h = s / 3600U;
    uint32_t m = (s / 60U) % 60U;
    uint32_t sec = s % 60U;
    /* Local time since reset, not on the master's time scale. */
    snprintf(out, sizeof(out),
             "OK TIME LOCAL_MS=%lu UPTIME=%02lu:%02lu:%02lu",
             (unsigned long)ms, (unsigned long)h, (unsigned long)m, (unsigned long)sec);
    uart_send_line(out);
    return;
  }
  if (strcmp(cmd, "STATUS") == 0) {
    char out[UART_LINE_MAX];
    snprintf(out, sizeof(out), "OK STATUS LED=%s ESP_LED=%s LOCAL_MS=%lu",
             led_is_on() ? "ON" : "OFF", g_esp_led_on ? "ON" : "OFF",
             (unsigned long)HAL_GetTick());
    uart_send_line(out);
    return;
  }
  if (strcmp(cmd, "LED") == 0) {
    if (n >= 2 && strcmp(a1, "ON") == 0) {
      led_set(1);
      uart_send_line("OK LED ON");
      return;
    }
    if (n >= 2 && strcmp(a1, "OFF") == 0) {
      led_set(0);
      uart_send_line("OK LED OFF");
      return;
    }
    if (n >= 2 && strcmp(a1, "TOGGLE") == 0) {
      led_set(!led_is_on());
      uart_send_line(led_is_on() ? "OK LED ON" : "OK LED OFF");
      return;
    }
    uart_send_line("ERR LED ON|OFF|TOGGLE");
    return;
  }

  uart_send_line("ERR unknown");
}

/*
 * Each KEY press flips the LED on the ESP32 side. The line starts with INFO
 * so the master does not take it for the reply to its own command.
 */
static void key_poll(void)
{
  GPIO_PinState key;

  if (HAL_GetTick() - g_key_tick < KEY_SAMPLE_MS) {
    return;
  }
  g_key_tick = HAL_GetTick();

  /* KEY is active low: act on the press edge only. */
  key = HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin);
  if (g_key_last == GPIO_PIN_SET && key == GPIO_PIN_RESET) {
    g_esp_led_on = !g_esp_led_on;
    uart_send_line(g_esp_led_on ? "INFO ESP_LED ON" : "INFO ESP_LED OFF");
  }
  g_key_last = key;
}

void App_Start(void)
{
  led_set(0);

  /* First UART byte. After that the callback restarts reception. */
  HAL_UART_Receive_IT(LINK_UART, &g_rx_byte, 1);

  /* Short pause so the master has time to open its monitor after reset. */
  HAL_Delay(50);
  send_banner();
}

void App_Poll(void)
{
  char line[UART_LINE_MAX];

  key_poll();

  if (!g_line_ready) {
    return;
  }

  /* Copy the line with interrupts disabled so reception does not tear the buffer. */
  __disable_irq();
  memcpy(line, g_rx_line, UART_LINE_MAX);
  g_line_ready = 0;
  __enable_irq();
  handle_line(line);
}

/* Every received UART byte. Collects a line up to \n. \r is dropped. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  uint8_t ch;

  if (huart != LINK_UART) {
    return;
  }

  ch = g_rx_byte;
  if (ch == '\r') {
    /* Empty step: the Windows line ending is not part of the command. */
  } else if (ch == '\n') {
    g_rx_line[g_rx_len] = 0;
    g_line_ready = 1;
    g_rx_len = 0;
  } else if (g_rx_len + 1U < UART_LINE_MAX) {
    g_rx_line[g_rx_len++] = (char)ch;
  } else {
    /* A line that is too long is dropped so it does not overrun the buffer. */
    g_rx_len = 0;
  }
  HAL_UART_Receive_IT(LINK_UART, &g_rx_byte, 1);
}

/* Framing or noise error on the line: reception stops otherwise. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == LINK_UART) {
    HAL_UART_Receive_IT(LINK_UART, &g_rx_byte, 1);
  }
}
