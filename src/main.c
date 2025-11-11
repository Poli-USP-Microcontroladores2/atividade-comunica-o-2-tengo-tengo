/*
 * UART Echo Bot for FRDM-KL25Z
 * Zephyr RTOS 4.2
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <string.h>

/* Define UART device - using console UART */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_console)

#define MSG_SIZE 32

/* Message queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* Receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

/*
 * UART interrupt callback
 * Reads characters until line end is detected, then pushes to message queue
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	/* Read until FIFO is empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
			/* Terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* Put message in queue (non-blocking) */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			/* Reset buffer position */
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* Characters beyond buffer size are dropped */
	}
}

/*
 * Print null-terminated string to UART
 */
void print_uart(const char *buf)
{
	int msg_len = strlen(buf);
	
	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
}

int main(void)
{
	char tx_buf[MSG_SIZE];

	printk("Starting UART Echo Bot on FRDM-KL25Z\n");

	/* Check if UART device is ready */
	if (!device_is_ready(uart_dev)) {
		printk("UART device not ready!\n");
		return -1;
	}

	/* Configure interrupt-driven UART */
	int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
	
	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API not supported\n");
			printk("Check if CONFIG_UART_INTERRUPT_DRIVEN=y in prj.conf\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return -1;
	}

	/* Enable RX interrupts */
	uart_irq_rx_enable(uart_dev);

	/* Print welcome message */
	print_uart("\r\n=================================\r\n");
	print_uart("  UART Echo Bot - FRDM-KL25Z\r\n");
	print_uart("=================================\r\n");
	print_uart("Type a message and press Enter\r\n");
	print_uart("> ");

	/* Main loop: wait for messages and echo them back */
	while (1) {
		if (k_msgq_get(&uart_msgq, &tx_buf, K_FOREVER) == 0) {
			print_uart("\r\nEcho: ");
			print_uart(tx_buf);
			print_uart("\r\n> ");
		}
	}

	return 0;
}