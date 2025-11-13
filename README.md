
# PSI-Microcontroladores2 - Aula 10
**Atividade:** Comunicação UART

# Projeto UART – Atividade em Duplas (Echo Bot + Async API)

## 1. Informações Gerais

**Dupla:**

* Integrante 1: Tiago Mamoru Hayashi – Número USP: 16835290  
* Integrante 2: Dimitri de Moura Garcia – Número USP: 16862224

**Objetivo:** Implementar, testar e documentar aplicações de comunicação UART baseadas nos exemplos oficiais `echo_bot` e `async_api`, utilizando desenvolvimento orientado a testes (TDD), diagramas de sequência D2 e registro de evidências.

---

## 2. Estrutura Esperada do Repositório

```

README.md
src/

docs/
evidence/
sequence-diagrams/

````

---

## 3. Etapa 1 – Echo Bot (UART Polling/Interrupt)

### 3.1 Descrição do Funcionamento

O código lê mensagens de até 32 bytes (valor ajustável) e envia para uma fila de leitura:

```c
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);
````

Essa fila envia os dados da UART para a thread principal, podendo armazenar até 10 mensagens de 32 bytes. O programa inicia quando uma mensagem chega na UART e continua enquanto houver caracteres a serem lidos. Ele realiza a leitura sequencialmente, utilizando um buffer, e ao detectar `\n` ou `\r`, finaliza a leitura, adiciona `\0` ao final da mensagem e envia para a fila.

Uma função envia os dados de volta pela UART, caractere por caractere. A função `main` verifica se tudo está pronto, configura funções, envia uma mensagem de teste (boas-vindas) e entra em loop para processar a fila.

**Referência:**
[Echo Bot Zephyr Documentation](https://docs.zephyrproject.org/latest/samples/drivers/uart/echo_bot/README.html)

### 3.2 Casos de Teste Planejados (TDD)

#### CT1 – Eco Básico

* **Entrada:** Usuário digita uma mensagem no terminal do VS Code (ex: "oi").
* **Saída esperada:** Mesma mensagem digitada.
* **Critério de Aceitação:** Mensagem exibida exatamente como digitada, incluindo letras maiúsculas, minúsculas e caracteres especiais.

#### CT2 – Linha Vazia

* **Entrada:** Usuário envia um comando vazio.
* **Saída esperada:** Mensagem vazia.
* **Critério de Aceitação:** O sistema aceita comandos em branco sem falhas.

#### CT3 – Linha Longa

* **Entrada:** Sequência de cerca de 100 caracteres.
* **Saída esperada:** Apenas os primeiros 31 caracteres (o último byte é `\0`).
* **Critério de Aceitação:** O Echo Bot imprime apenas o número de caracteres permitido pela memória configurada.

> **Observação:** O tamanho máximo da mensagem depende da memória alocada, que tem limite definido pelo `proj.config`.

### 3.3 Implementação

* **Arquivos modificados:**

  * `proj.config`: adição de log, ajuste da memória, ativação da UART e do console.
  * `main.c`: modificação da define da UART (`zephyr_console`), substituição de `return 0` por `return -1`, ajustes nos `printk` e na interface do usuário.

* **Justificativa:**
  As alterações garantem funcionamento no VS Code, aumento da memória da UART, maior confiabilidade (`zephyr_console`) e melhor compreensão da interface de usuário.

### 3.4 Evidências de Funcionamento

Salvar em: `docs/evidence/echo_bot/`

Exemplo:

```
[Link para o log CT1](docs/evidence/echo_bot/ct1_output.txt)
```

Trecho ilustrativo:

```
Hello! I'm your echo bot. Tell me something and press enter:
Echo: Hello World!
```

### 3.5 Diagramas de Sequência D2

Adicionar diagramas completos e código-base em: `docs/sequence-diagrams/`

Referência: [D2 Sequence Diagrams](https://d2lang.com/tour/sequence-diagrams/)

---

## 4. Etapa 2 – Async API (Transmissão/Recepção Assíncrona)

### 4.1 Descrição do Funcionamento

O código envia e recebe pacotes de forma assíncrona:

* **Transmissão:** Pacotes são enviados diretamente ou enfileirados (até 4 pacotes) se a UART estiver ocupada.
* **Recepção:** Dois buffers alternados evitam perda de informação.
* A `main` cria pacotes aleatórios (1 a 4), desativa periodicamente o receptor e imprime mensagens de controle.

**Referência:**
[Async API Zephyr Documentation](https://docs.zephyrproject.org/latest/samples/drivers/uart/async_api/README.html)

### 4.2 Casos de Teste Planejados (TDD)

#### CT1 – Transmissão de pacotes a cada 5s

* Entrada:
* Saída esperada:
* Critério de Aceitação:

#### CT2 – Recepção

* Entrada:
* Saída esperada:
* Critério de Aceitação:

#### CT3 – Verificação de timing de 5s

* Entrada:
* Saída esperada:
* Critério de Aceitação:

> Adicionar mais casos se necessário.

### 4.3 Implementação

* **Arquivos modificados:**
* **Justificativa das alterações:**

### 4.4 Evidências de Funcionamento

Salvar em: `docs/evidence/async_api/`

Exemplo:

```
Loop 0:
Sending 3 packets (packet size: 5)
Packet: 0
Packet: 1
Packet: 2
```

```
RX is now enabled
UART callback: RX_RDY
Data (HEX): 48 65 6C 6C 6F
Data (ASCII): Hello
```

### 4.5 Diagramas de Sequência D2

Adicionar diagramas completos e código-base em: `docs/sequence-diagrams/`

Referência: [D2 Sequence Diagrams](https://d2lang.com/tour/sequence-diagrams/)

---

## 5. Conclusões da Dupla

* **O que deu certo:** No Echo Bot, tudo funcionou bem. O principal problema foi o tamanho da mensagem, facilmente resolvido ajustando a memória disponível.
* **Desafios enfrentados:**

```

```

```

---

Quer que eu faça isso?
```
