/*
 * UART Interrupt-Driven para FRDM-KL25Z
 * 
 * Esta versão usa interrupt-driven API que é suportada nativamente
 * pelo Kinetis KL25. Funcionalidade similar ao async mas com controle manual.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <string.h>

LOG_MODULE_REGISTER(uart_int, LOG_LEVEL_INF);

/* UART device */
#define UART_DEVICE_NODE DT_NODELABEL(uart0)

/* Buffer sizes */
#define TX_BUF_SIZE 128
#define RX_BUF_SIZE 128

/* Buffers */
static uint8_t tx_buffer[TX_BUF_SIZE];
static uint8_t rx_buffer[RX_BUF_SIZE];
static volatile size_t tx_len = 0;
static volatile size_t tx_pos = 0;
static volatile size_t rx_pos = 0;

/* Flags */
static volatile bool tx_busy = false;
static struct k_sem tx_done_sem;

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* ISR callback */
static void uart_isr_callback(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	/* Processar TX */
	if (uart_irq_tx_ready(dev)) {
		if (tx_pos < tx_len) {
			/* Enviar próximo byte */
			int sent = uart_fifo_fill(dev, &tx_buffer[tx_pos], tx_len - tx_pos);
			tx_pos += sent;
			
			if (tx_pos >= tx_len) {
				/* TX completo */
				uart_irq_tx_disable(dev);
				tx_busy = false;
				k_sem_give(&tx_done_sem);
			}
		} else {
			/* Desabilitar TX IRQ */
			uart_irq_tx_disable(dev);
			tx_busy = false;
			k_sem_give(&tx_done_sem);
		}
	}

	/* Processar RX */
	if (uart_irq_rx_ready(dev)) {
		uint8_t byte;
		
		while (uart_fifo_read(dev, &byte, 1) > 0) {
			if (rx_pos < RX_BUF_SIZE - 1) {
				rx_buffer[rx_pos++] = byte;
				
				/* Echo do caractere */
				uart_poll_out(dev, byte);
				
				/* Se recebeu enter, processar linha */
				if (byte == '\r' || byte == '\n') {
					rx_buffer[rx_pos] = '\0';
					if (rx_pos > 1) {
						LOG_INF("Received: %s", rx_buffer);
					}
					rx_pos = 0;
				}
			} else {
				/* Buffer cheio - resetar */
				LOG_WRN("RX buffer overflow");
				rx_pos = 0;
			}
		}
	}
}

/* Função para enviar string */
static int uart_send_string(const struct device *dev, const char *str)
{
	size_t len = strlen(str);
	int ret;
	
	if (len > TX_BUF_SIZE) {
		LOG_ERR("String too long");
		return -EINVAL;
	}
	
	/* Aguardar TX anterior terminar */
	if (tx_busy) {
		ret = k_sem_take(&tx_done_sem, K_MSEC(1000));
		if (ret != 0) {
			LOG_ERR("TX timeout");
			return -ETIMEDOUT;
		}
	}
	
	/* Copiar para buffer */
	memcpy(tx_buffer, str, len);
	tx_len = len;
	tx_pos = 0;
	tx_busy = true;
	
	/* Habilitar TX IRQ */
	uart_irq_tx_enable(dev);
	
	return 0;
}

int main(void)
{
	int loop_counter = 0;
	char message[TX_BUF_SIZE];
	int ret;

	LOG_INF("UART Interrupt-Driven for FRDM-KL25Z");
	LOG_INF("======================================");

	/* Verificar device */
	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not ready!");
		return -1;
	}

	/* Inicializar semáforo */
	k_sem_init(&tx_done_sem, 0, 1);

	/* Configurar callback */
	ret = uart_irq_callback_user_data_set(uart_dev, uart_isr_callback, NULL);
	if (ret != 0) {
		LOG_ERR("Failed to set UART callback (%d)", ret);
		return -1;
	}

	/* Habilitar RX IRQ */
	uart_irq_rx_enable(uart_dev);

	LOG_INF("UART initialized successfully!");
	LOG_INF("Type something and press Enter...");
	LOG_INF("");

	/* Loop principal */
	while (1) {
		/* Aguardar 5 segundos */
		k_sleep(K_SECONDS(5));

		/* Gerar mensagens aleatórias */
		int num_msgs = (sys_rand32_get() % 4) + 1;
		
		LOG_INF("=== Loop %d: Sending %d messages ===", loop_counter, num_msgs);

		for (int i = 0; i < num_msgs; i++) {
			/* Criar mensagem */
			snprintf(message, sizeof(message),
				 "Loop %d: Packet: %d\r\n",
				 loop_counter, i);

			/* Enviar */
			ret = uart_send_string(uart_dev, message);
			if (ret != 0) {
				LOG_ERR("Failed to send message %d", i);
			} else {
				LOG_DBG("Sent message %d", i);
			}

			/* Aguardar TX completar */
			k_sem_take(&tx_done_sem, K_FOREVER);

			/* Delay entre mensagens */
			k_sleep(K_MSEC(100));
		}

		LOG_INF("=== Loop %d complete ===\n", loop_counter);
		loop_counter++;
	}

	return 0;
}