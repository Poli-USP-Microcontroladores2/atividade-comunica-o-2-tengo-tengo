/*
 * UART Interrupt-Driven Completo para FRDM-KL25Z
 * 
 * Funcionalidades:
 * - Fila TX com limite de 4 pacotes
 * - Double buffer RX alternado
 * - Toggle periódico do RX (enable/disable)
 * - Processamento em callbacks/ISR
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

/* Configurações */
#define TX_QUEUE_SIZE 4
#define MAX_TX_LEN 64
#define RX_BUFFER_SIZE 64
#define LOOP_ITER_MAX_TX 4

/* Estrutura de pacote TX */
struct tx_packet {
	uint8_t data[MAX_TX_LEN];
	size_t len;
};

/* Fila TX */
static struct tx_packet tx_queue[TX_QUEUE_SIZE];
static volatile uint8_t tx_queue_head = 0;
static volatile uint8_t tx_queue_tail = 0;
static volatile uint8_t tx_queue_count = 0;
static struct k_spinlock tx_queue_lock;

/* TX atual */
static struct tx_packet tx_current;
static volatile size_t tx_pos = 0;
static volatile bool tx_busy = false;

/* Double buffer RX */
static uint8_t rx_buffer[2][RX_BUFFER_SIZE];
static volatile uint8_t rx_buffer_idx = 0;
static volatile size_t rx_pos = 0;

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* ===== FUNÇÕES DA FILA TX ===== */

static bool tx_queue_is_full(void)
{
	return tx_queue_count >= TX_QUEUE_SIZE;
}

static bool tx_queue_is_empty(void)
{
	return tx_queue_count == 0;
}

static bool tx_queue_push(const uint8_t *data, size_t len)
{
	k_spinlock_key_t key;
	
	if (len > MAX_TX_LEN) {
		return false;
	}
	
	key = k_spin_lock(&tx_queue_lock);
	
	if (tx_queue_is_full()) {
		k_spin_unlock(&tx_queue_lock, key);
		return false;
	}
	
	memcpy(tx_queue[tx_queue_head].data, data, len);
	tx_queue[tx_queue_head].len = len;
	tx_queue_head = (tx_queue_head + 1) % TX_QUEUE_SIZE;
	tx_queue_count++;
	
	k_spin_unlock(&tx_queue_lock, key);
	return true;
}

static bool tx_queue_pop(struct tx_packet *pkt)
{
	k_spinlock_key_t key;
	
	key = k_spin_lock(&tx_queue_lock);
	
	if (tx_queue_is_empty()) {
		k_spin_unlock(&tx_queue_lock, key);
		return false;
	}
	
	memcpy(pkt, &tx_queue[tx_queue_tail], sizeof(struct tx_packet));
	tx_queue_tail = (tx_queue_tail + 1) % TX_QUEUE_SIZE;
	tx_queue_count--;
	
	k_spin_unlock(&tx_queue_lock, key);
	return true;
}

/* ===== CALLBACK UART (ISR) ===== */

static void uart_isr_callback(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	/* ===== PROCESSAR TX ===== */
	if (uart_irq_tx_ready(dev)) {
		if (tx_busy && tx_pos < tx_current.len) {
			/* Enviar bytes do pacote atual */
			int sent = uart_fifo_fill(dev, &tx_current.data[tx_pos], 
						  tx_current.len - tx_pos);
			tx_pos += sent;
			
			if (tx_pos >= tx_current.len) {
				/* Pacote atual completo */
				LOG_DBG("TX complete: %d bytes", tx_current.len);
				tx_busy = false;
				
				/* Tentar pegar próximo pacote da fila */
				if (tx_queue_pop(&tx_current)) {
					LOG_DBG("Dequeued packet from TX queue");
					tx_pos = 0;
					tx_busy = true;
				} else {
					/* Fila vazia - desabilitar TX IRQ */
					uart_irq_tx_disable(dev);
				}
			}
		} else {
			/* Nada para enviar */
			uart_irq_tx_disable(dev);
		}
	}

	/* ===== PROCESSAR RX ===== */
	if (uart_irq_rx_ready(dev)) {
		uint8_t byte;
		uint8_t current_buf = rx_buffer_idx;
		
		while (uart_fifo_read(dev, &byte, 1) > 0) {
			/* Guardar no buffer atual */
			if (rx_pos < RX_BUFFER_SIZE - 1) {
				rx_buffer[current_buf][rx_pos++] = byte;
				
				/* Se recebeu enter, processar */
				if (byte == '\r' || byte == '\n') {
					rx_buffer[current_buf][rx_pos] = '\0';
					
					if (rx_pos > 1) {
						/* Imprimir pacote recebido */
						LOG_HEXDUMP_INF(rx_buffer[current_buf], 
								rx_pos - 1, "RX Packet");
					}
					
					/* Trocar buffer */
					rx_buffer_idx = rx_buffer_idx ? 0 : 1;
					rx_pos = 0;
				}
			} else {
				/* Buffer cheio - trocar e resetar */
				LOG_WRN("RX buffer full - switching");
				rx_buffer_idx = rx_buffer_idx ? 0 : 1;
				rx_pos = 0;
			}
		}
	}
}

/* ===== FUNÇÃO PARA ENVIAR PACOTE ===== */

static int uart_send_packet(const uint8_t *data, size_t len)
{
	k_spinlock_key_t key;
	bool start_tx = false;
	
	if (len > MAX_TX_LEN) {
		LOG_ERR("Packet too large: %d bytes", len);
		return -EINVAL;
	}
	
	key = k_spin_lock(&tx_queue_lock);
	
	if (!tx_busy) {
		/* TX livre - enviar imediatamente */
		memcpy(tx_current.data, data, len);
		tx_current.len = len;
		tx_pos = 0;
		tx_busy = true;
		start_tx = true;
	} else {
		/* TX ocupado - enfileirar */
		k_spin_unlock(&tx_queue_lock, key);
		
		if (!tx_queue_push(data, len)) {
			LOG_WRN("TX queue full - packet dropped");
			return -ENOMEM;
		}
		LOG_DBG("Packet queued (%d/%d)", tx_queue_count, TX_QUEUE_SIZE);
		return 0;
	}
	
	k_spin_unlock(&tx_queue_lock, key);
	
	if (start_tx) {
		/* Habilitar TX IRQ para iniciar transmissão */
		uart_irq_tx_enable(uart_dev);
	}
	
	return 0;
}

/* ===== MAIN ===== */

int main(void)
{
	int loop_counter = 0;
	uint8_t num_packets;
	char message[MAX_TX_LEN];
	int msg_len;
	int ret;
	bool rx_enabled = false;

	LOG_INF("UART Interrupt-Driven - FRDM-KL25Z");
	LOG_INF("====================================");

	/* Verificar device */
	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not ready!");
		return -1;
	}

	/* Configurar callback */
	ret = uart_irq_callback_user_data_set(uart_dev, uart_isr_callback, NULL);
	if (ret != 0) {
		LOG_ERR("Failed to set UART callback (%d)", ret);
		return -1;
	}

	LOG_INF("UART initialized successfully!");
	LOG_INF("");

	/* Loop principal */
	while (1) {
		/* Aguardar 5 segundos */
		k_sleep(K_SECONDS(5));

		/* Gerar número aleatório de pacotes (1 a 4) */
		num_packets = (sys_rand32_get() % LOOP_ITER_MAX_TX) + 1;
		
		LOG_INF("Loop %d: Sending %d packets", loop_counter, num_packets);

		/* Criar e enviar pacotes */
		for (int i = 0; i < num_packets; i++) {
			msg_len = snprintf(message, sizeof(message),
					   "Loop %d: Packet: %d\r\n",
					   loop_counter, i);
			
			if (msg_len > 0) {
				ret = uart_send_packet((uint8_t *)message, msg_len);
				if (ret != 0) {
					LOG_ERR("Failed to send packet %d", i);
				}
			}
			
			/* Pequeno delay entre pacotes */
			k_sleep(K_MSEC(50));
		}

		/* Toggle RX - demonstrar controle da UART */
		if (rx_enabled) {
			uart_irq_rx_disable(uart_dev);
			LOG_INF("RX disabled");
		} else {
			rx_buffer_idx = 0;
			rx_pos = 0;
			uart_irq_rx_enable(uart_dev);
			LOG_INF("RX enabled");
		}
		rx_enabled = !rx_enabled;

		loop_counter++;
	}

	return 0;
}