/**
 * @file       main.cpp
 * @author     Pere Tuset-Peiro (peretuset@openmote.com)
 * @version    v0.1
 * @date       January, 2019
 * @brief
 *
 * @copyright  Copyright 2019, OpenMote Technologies, S.L.
 *             This file is licensed under the GNU General Public License v2.
 */

/*================================ include ==================================*/

#include "BoardImplementation.hpp"

#include "platform_types.hpp"

#include "Gpio.hpp"
#include "I2c.hpp"
#include "Spi.hpp"

#include "Callback.hpp"
#include "Scheduler.hpp"
#include "Semaphore.hpp"
#include "Task.hpp"

#include "At86rf215.hpp"
#include "At86rf215_conf.h"

#include "Bme280.hpp"
#include "Opt3001.hpp"

/*================================ define ===================================*/

#define HEARTBEAT_TASK_PRIORITY             ( tskIDLE_PRIORITY + 0 )
#define TRANSMIT_TASK_PRIORITY              ( tskIDLE_PRIORITY + 1 )

#define HEARTBEAT_TASK_STACK_SIZE           ( 128 )
#define TRANSMIT_TASK_STACK_SIZE            ( 1024 )

#define SPI_BAUDRATE                        ( 16000000 )

#define TX_BUFFER_LENGTH                    ( 127 )
#define EUI48_ADDDRESS_LENGTH               ( 6 )

#define SENSORS_CTRL_PORT                   ( GPIO_A_BASE )
#define SENSORS_CTRL_PIN                    ( GPIO_PIN_7 )

#define BME280_I2C_ADDRESS                  ( BME280_I2C_ADDR_PRIM )
#define OPT3001_I2C_ADDRESS                 ( OPT3001_I2C_ADDR_GND )

#define RADIO_CORE                          ( At86rf215::CORE_RF09 )
#define RADIO_SETTINGS                      ( &radio_settings[CONFIG_OFDM2_MCS0] )
#define RADIO_FREQUENCY                     ( &frequency_settings_09[FREQUENCY_09_OFDM2] )
#define RADIO_CHANNEL                       ( 0 )
#define RADIO_TX_POWER                      ( At86rf215::TransmitPower::TX_POWER_MAX )

/*================================ typedef ==================================*/

/*=============================== prototypes ================================*/

extern "C" void board_sleep(TickType_t xModifiableIdleTime);
extern "C" void board_wakeup(TickType_t xModifiableIdleTime);

static void prvHeartbeatTask(void *pvParameters);
static void prvTransmitTask(void *pvParameters);

static uint16_t prepare_packet(uint8_t* packet_ptr, uint8_t* eui48_address, uint32_t packet_counter);

static void radio_tx_init(void);
static void radio_tx_done(void);

/*=============================== variables =================================*/

static Task heartbeatTask{(const char *) "Heartbeat", 128, (unsigned char)HEARTBEAT_TASK_PRIORITY, prvHeartbeatTask, nullptr};
static Task radioTask{(const char *) "Transmit", 128, (unsigned char)TRANSMIT_TASK_PRIORITY, prvTransmitTask, nullptr};

static PlainCallback radio_tx_init_cb {&radio_tx_init};
static PlainCallback radio_tx_done_cb {&radio_tx_done};

static SemaphoreBinary semaphore {false};

static uint8_t radio_buffer[TX_BUFFER_LENGTH];
static uint8_t eui48_address[EUI48_ADDDRESS_LENGTH];

static bool board_slept;

/*================================= public ==================================*/

int main(void)
{
  /* Initialize the board */
  board.init();
  
  /* Enable the SPI interface */
  spi0.enable(SPI_BAUDRATE);

  /* Start the scheduler */
  Scheduler::run();
  
  return 0;
}

/*================================ private ==================================*/

static void prvTransmitTask(void *pvParameters)
{
  uint16_t packet_counter = 0;
  bool status;

  /* Get EUI48 address */
  board.getEUI48(eui48_address);

  /* Set radio callbacks and enable interrupts */
  at86rf215.setTxCallbacks(RADIO_CORE, &radio_tx_init_cb, &radio_tx_done_cb);
  at86rf215.enableInterrupts();

  /* Forever */
  while (true)
  {
    uint16_t tx_buffer_len;
    bool status = true;

    /* Turn on red LED */
    led_red.on();
    Scheduler::delay_ms(10);

    if (status)
    {
      bool sent;

      /* Turn AT86RF215 radio on */
      at86rf215.on();

      /* Wake up and configure radio */
      at86rf215.wakeup(RADIO_CORE);
      at86rf215.configure(RADIO_CORE, RADIO_SETTINGS, RADIO_FREQUENCY, RADIO_CHANNEL);
      at86rf215.setTransmitPower(RADIO_CORE, RADIO_TX_POWER);

      /* Prepare radio packet */
      tx_buffer_len = prepare_packet(radio_buffer, eui48_address, packet_counter);

      /* Load packet to radio */
      at86rf215.loadPacket(RADIO_CORE, radio_buffer, tx_buffer_len);

      /* Transmit packet */
      at86rf215.transmit(RADIO_CORE);

      /* Wait until packet has been transmitted */
      sent = semaphore.take();

      /* Turn AT86RF215 radio off */
      at86rf215.off();
    }

    /* Increment packet counter */
    packet_counter++;

    /* Turn off red LED */
    led_red.off();

    /* Delay for 10 seconds */
    Scheduler::delay_ms(1000);
  }
}

static void prvHeartbeatTask(void *pvParameters)
{
  /* Forever */
  while (true)
  {
    /* Turn on green LED for 10 ms */
    led_green.on();
    Scheduler::delay_ms(10);

    /* Turn off green LED for 990 ms */
    led_green.off();
    Scheduler::delay_ms(990);
  }
}

static void radio_tx_init(void)
{
  /* Turn on orange LED */
  led_orange.on();
}

static void radio_tx_done(void)
{
  /* Turn off orange LED */
  led_orange.off();

  /* Notify we have transmitted a packet */
  semaphore.giveFromInterrupt();
}

void board_sleep(TickType_t xModifiableIdleTime)
{
  /* Check if board can go to sleep */
  if (i2c.canSleep())
  {
    /* If so, put SPI & I2C to sleep */
    i2c.sleep();

    /* Remember that the board went to sleep */
    board_slept = true;
  }
  else
  {
    /* If not, remember that the board did NOT went to sleep */
    board_slept = false;

    /* And update the time to ensure it does NOT got to sleep */
    xModifiableIdleTime = 0;
  }
}

void board_wakeup(TickType_t xModifiableIdleTime)
{
  /* Check if the board went to sleep */
  if (board_slept)
  {
    /* If so, wakeup SPI & I2C */
    i2c.wakeup();
  }
}

static uint16_t prepare_packet(uint8_t* packet_ptr, uint8_t* eui48_address, uint32_t packet_counter)
{
  uint16_t packet_length = 0;
  uint8_t i;

  /* Copy MAC adress */
  for (i = 0; i < EUI48_ADDDRESS_LENGTH; i++)
  {
    packet_ptr[i] = eui48_address[i];
  }
  packet_length = i;

  /* Copy packet counter */
  i = packet_length;
  packet_ptr[i++] = (uint8_t)((packet_counter & 0xFF000000) >> 24);
  packet_ptr[i++] = (uint8_t)((packet_counter & 0x00FF0000) >> 16);
  packet_ptr[i++] = (uint8_t)((packet_counter & 0x0000FF00) >> 8);
  packet_ptr[i++] = (uint8_t)((packet_counter & 0x000000FF) >> 0);
  packet_length = i;

  return packet_length;
}
