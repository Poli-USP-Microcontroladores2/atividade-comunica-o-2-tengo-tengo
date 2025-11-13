/*
 * UART Interrupt-Driven Melhorado para FRDM-KL25Z
 * 
 * Melhorias:
 * - RX buffer thread-safe com processamento fora da ISR
 * - Logs formatados e organizados
 * - Toggle RX mais estável
 * - Sincronização adequada entre ISR e main thread
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <ctype.h>

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

/* Estrutura de pacote RX recebido */
struct rx_packet {
	uint8_t data[RX_BUFFER_SIZE];
	size_t len;
	bool ready;
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

/* Double buffer RX - processamento assíncrono */
static uint8_t rx_buffer[2][RX_BUFFER_SIZE];
static volatile uint8_t rx_write_idx = 0;  // Buffer sendo escrito pela ISR
static volatile size_t rx_pos = 0;
static struct rx_packet rx_ready_packet;
static struct k_spinlock rx_lock;
static struct k_sem rx_data_sem;
static volatile uint32_t rx_isr_count = 0;  // Debug: contador de chamadas ISR

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
				tx_busy = false;
				
				/* Tentar pegar próximo pacote da fila */
				if (tx_queue_pop(&tx_current)) {
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
		k_spinlock_key_t key;
		
		rx_isr_count++;  // Debug: incrementar contador
		
		while (uart_fifo_read(dev, &byte, 1) > 0) {
			key = k_spin_lock(&rx_lock);
			
			/* Guardar no buffer de escrita atual */
			if (rx_pos < RX_BUFFER_SIZE - 1) {
				rx_buffer[rx_write_idx][rx_pos++] = byte;
				
				/* Se recebeu enter ou chegou no limite, processar */
				if (byte == '\r' || byte == '\n' || rx_pos >= RX_BUFFER_SIZE - 1) {
					if (rx_pos > 1 || (rx_pos == 1 && byte != '\r' && byte != '\n')) {
						/* Copiar para o pacote pronto se ainda não tem um pendente */
						if (!rx_ready_packet.ready) {
							size_t copy_len = rx_pos;
							
							/* Remover \r\n do final se presente */
							if (copy_len > 0 && 
							    (rx_buffer[rx_write_idx][copy_len-1] == '\r' || 
							     rx_buffer[rx_write_idx][copy_len-1] == '\n')) {
								copy_len--;
							}
							
							if (copy_len > 0) {
								memcpy(rx_ready_packet.data, 
								       rx_buffer[rx_write_idx], 
								       copy_len);
								rx_ready_packet.len = copy_len;
								rx_ready_packet.ready = true;
								
								/* Sinalizar thread de processamento */
								k_sem_give(&rx_data_sem);
							}
						}
					}
					
					/* Trocar buffer e resetar posição */
					rx_write_idx = rx_write_idx ? 0 : 1;
					rx_pos = 0;
				}
			} else {
				/* Buffer cheio - resetar */
				rx_write_idx = rx_write_idx ? 0 : 1;
				rx_pos = 0;
			}
			
			k_spin_unlock(&rx_lock, key);
		}
	}
}

/* ===== THREAD DE PROCESSAMENTO RX ===== */

static void rx_processing_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	
	struct rx_packet local_packet;
	
	while (1) {
		/* Aguardar dados */
		k_sem_take(&rx_data_sem, K_FOREVER);
		
		/* Copiar pacote pronto */
		k_spinlock_key_t key = k_spin_lock(&rx_lock);
		if (rx_ready_packet.ready) {
			memcpy(&local_packet, &rx_ready_packet, sizeof(struct rx_packet));
			rx_ready_packet.ready = false;
		} else {
			k_spin_unlock(&rx_lock, key);
			continue;
		}
		k_spin_unlock(&rx_lock, key);
		
		/* Processar dados fora da ISR */
		LOG_INF("UART callback: RX_RDY (ISR calls: %u)", rx_isr_count);
		
		/* Print HEX */
		printk("Data (HEX): ");
		for (size_t i = 0; i < local_packet.len; i++) {
			printk("%02X ", local_packet.data[i]);
		}
		printk("\n");
		
		/* Print ASCII */
		printk("Data (ASCII): ");
		for (size_t i = 0; i < local_packet.len; i++) {
			if (isprint(local_packet.data[i])) {
				printk("%c", local_packet.data[i]);
			} else {
				printk(".");
			}
		}
		printk("\n\n");
	}
}

K_THREAD_DEFINE(rx_thread, 1024, rx_processing_thread, NULL, NULL, NULL, 5, 0, 0);

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

	/* Inicializar semáforo */
	k_sem_init(&rx_data_sem, 0, 1);

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

	LOG_INF("UART initialized successfully!\n");

	/* Loop principal */
	while (1) {
		/* Aguardar 5 segundos */
		k_sleep(K_SECONDS(5));

		/* Gerar número aleatório de pacotes (1 a 4) */
		num_packets = (sys_rand32_get() % LOOP_ITER_MAX_TX) + 1;
		
		/* Calcular tamanho aproximado do pacote */
		msg_len = snprintf(message, sizeof(message),
				   "Loop %d: Packet: %d\r\n", loop_counter, 0);
		
		LOG_INF("Loop %d:", loop_counter);
		LOG_INF("Sending %d packets (packet size: %d)", num_packets, msg_len);

		/* Criar e enviar pacotes */
		for (int i = 0; i < num_packets; i++) {
			msg_len = snprintf(message, sizeof(message),
					   "Packet: %d\r\n", i);
			
			if (msg_len > 0) {
				ret = uart_send_packet((uint8_t *)message, msg_len);
				if (ret != 0) {
					LOG_ERR("Failed to send packet %d", i);
				} else {
					LOG_INF("Packet: %d", i);
				}
			}
			
			/* Pequeno delay entre pacotes */
			k_sleep(K_MSEC(100));
		}
		
		printk("\n");

		/* Toggle RX a cada 2 loops (10 segundos) para dar tempo de testar */
		if (loop_counter % 2 == 0) {
			if (rx_enabled) {
				uart_irq_rx_disable(uart_dev);
				LOG_INF("RX is now disabled\n");
				rx_enabled = false;
			} else {
				/* Limpar buffers antes de habilitar */
				k_spinlock_key_t key = k_spin_lock(&rx_lock);
				rx_write_idx = 0;
				rx_pos = 0;
				rx_ready_packet.ready = false;
				k_spin_unlock(&rx_lock, key);
				
				/* Flush hardware FIFO para remover lixo */
				uint8_t dummy;
				while (uart_fifo_read(uart_dev, &dummy, 1) > 0) {
					/* Descartar bytes antigos */
				}
				
				/* Habilitar RX IRQ */
				uart_irq_rx_enable(uart_dev);
				
				/* Pequeno delay para estabilizar */
				k_sleep(K_MSEC(50));
				
				LOG_INF("RX is now enabled (ready to receive)\n");
				rx_enabled = true;
			}
		}

		loop_counter++;
	}

	return 0;
}