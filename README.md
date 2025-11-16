
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
* **Saída esperada:** Echo bot: Mesma mensagem digitada.
* **Critério de Aceitação:** Mensagem exibida exatamente como digitada,  ou seja, ocorre inclusive uma distinção entre letras maiúsculas e minúsculas além do reconhecimento de caracteres especiais, como espaços e acentos.

#### CT2 – Linha Vazia

* **Entrada:** Usuário envia um comando vazio.
* **Saída esperada:** Mensagem vazia.
* **Critério de Aceitação:** O sistema funciona perfeitamente, entretanto acontece um fenômeno interessante, quando o usuário envia a mensagem a primeira vez, o Echo bot não manda nada, provavelmente, porque entende o envio como um erro, quando a mensagem vazia é enviada a segunda vez retorna Echo Bot:

#### CT3 – Linha Longa

* **Entrada:** Sequência de cerca de 100 caracteres.
* **Saída esperada:** Apenas os primeiros 31 caracteres (o último byte é `\r`).
* **Critério de Aceitação:** O Echo Bot imprime apenas o número de caracteres permitido pela memória configurada.

> **Observação:** O tamanho máximo da mensagem depende do define MSG size no início do código, que iniciamos como 32, mas pode ser altera. Entretanto, ele tem um limite que é definido no `proj.config` e um limite do Hardware (FRDM Kl 25z), por exemplo na nossa, prj.conf ficou definido 2048, entretanto o código/hardware só suporta até 512 carcteres.

 Obs: Esses casos estão ilustrados no evidences dentro de docs da branch echo-bot
### 3.3 Implementação

* **Arquivos modificados:**

  * `proj.config`: adição de log, ajuste da memória, ativação da UART e do console.
  * `main.c`: modificação da define da UART (`zephyr_console`), substituição de `return 0` por `return -1`, ajustes nos `printk` e na interface do usuário.

* **Justificativa:**
  As alterações garantem funcionamento compatível com o VS Code, aumento da memória da UART, maior confiabilidade (`zephyr_console`) e melhor compreensão da interface de usuário, este principalmente na divisão entre o que é o retorna da palavra e o que é o código hexadecimal da mensagem.
  

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

 Ao iniciar o código, o computador espera cerca de 4ms para iniciar a UART. 5 segundos após a inicialização, Rx é habilitado, exibindo um log no serial monitor. Após isso, é inciado um loop na main, onde Rx é constantemente habilitado e desabilitado com tempo de 5 segundos entre cada troca. Quando habilitado, Recebe pacotes e quando desabilitado, pode enviar pacotes(depende se há algo na fila)

#### CT2 – Recepção

* Entrada: Usuário digita mensagem no monitor serial, que é lido pelo programa e enviado para o microcontrolador via UART pelo RX, podendo enviar corretamente mensagens de até 64 bytes(64 dígitos.)
  
* Saída esperada: Era esperado o retorno da mensagem digitada pelo usuário de duas formas: Hexadecimal e em ASCII. 

  
* Critério de Aceitação: Apesar do código fazer a saída esperada muitas vezes, algumas vezes, mesmo com Rx habilitado, o código não retornava mensagem tanto em Hex, quanto em ASCII. Realizando uma análide no código, foi possível perceber a ocorrência de race condition entre o habilitar RX com a isr de controle, que barrava a impressão dos pacotes de informação.
  

#### CT3 – Verificação de timing de 5s


 Para verificar o timing entre o habilitar Rx e desabilitar Rx, foi usado log, que já estava presente no programa. Concluimos que: quando não há mensagens para serem transmitidas, o timing de 5 segundos funciona perfeitamente. Porém, quando há mensagens na fila, Ocorre um atraso de cerca de 1 segundo devido ao processamento e envio da mensagem.

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

```Enfrentamos a maior parte das dificuldades no código do Async Api, principalmente na parte de configuração da prj.conf, uma vez que, primeiro tivemos dificuldades para implementar por Api, o que fez com que a gente precisa-se adaptar para Interrupted Driven. Além disso, precisamos adicionar bastante coisa no prj.conf para que o código feito com IA podesse ser compatível com o VS CODE. Esse processo exigiu algumas horas e um pouco de empirismo e suporte da IA.

```

```

---

Quer que eu faça isso?
```
