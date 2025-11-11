# PSI-Microcontroladores2-Aula10
Atividade: Comunicação UART


# Projeto UART – Atividade em Duplas (Echo Bot + Async API)

## 1. Informações Gerais

* Dupla:

  * Integrante 1: Tiago Mamoru Hayashi  número USP: 16835290
  * Integrante 2: Dimitri de Moura Garcia número USP:16862224

* Objetivo: implementar, testar e documentar aplicações de comunicação UART baseadas nos exemplos oficiais “echo_bot” e “async_api”, utilizando desenvolvimento orientado a testes, diagramas de sequência D2 e registro de evidências.

---

# 2. Estrutura Esperada do Repositório

```
README.md
src/

docs/
  evidence/
  sequence-diagrams/

```

---

# 3. Etapa 1 – Echo Bot (UART Polling/Interrupt)

## 3.1 Descrição do Funcionamento

 De acordo com o código, ele consegue ler mensagens de até 32 bits(valor esse que pode ser alterado) e envia para uma fila de leitura: K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4); 
 Essa fila manda os dados da uart para a thread principal e que cabem até 10 mensagens de 32 bits. O programa inicia de fato sempre que uma mensagem chega na uart e continua enquanto tiver caracteres a serem lidos. Ele realiza a leitura sequencialmente, usando buffer e sempre que lê \n ou \r, entende que a linha cabou, finalizando a leitura e enviando o que foi lido para a fila, com \0 no final da mensagem.
 Então, uma outra função envia pela uart o que foi digitado, enviando uma mensagem por vez, caracterte por caractere. Já a função main verifica se está tudo pronto pra rodar o código, configura as funções, envia uma mensagem de teste(boas vindas) e por fim entra em loop até que haja algo na fila para ser impresso.

Link usado como referência:

[https://docs.zephyrproject.org/latest/samples/drivers/uart/echo_bot/README.html](https://docs.zephyrproject.org/latest/samples/drivers/uart/echo_bot/README.html)


## 3.2 Casos de Teste Planejados (TDD)

### CT1 – Eco básico

O código usado para o Eco básico foi o mesmo do exemplo, apenas com algumas mudanças para funcionar pra versão do platformio.

* Entrada: Usuário do echo bot digita uma mensagem no terminal do vs code. No teste utilizamos a mensagem "oi".
* Saída esperada: A mesma digitada pelo usuário
* Critério de Aceitação: a exata mesma palavra digitada no terminal, com letras maiúsculas, caracteres especiais e sem erros ou falta de caractertes. Para confirmar isso, realizamos diversos testes,  digitando vários caracteres especiais(^, ~, <, *, &, M, O, a, etc.), todos digitados perfeitamente. Vale ressaltar que todas eram mensagens curtas( no máximo 7 letras).

### CT2 – Linha vazia
* Entrada: o usuário do echo bot envioa um comando vazio, sem espaços ou caracteres.
* Saída esperada: uma mensagem vazia.
* Critério de Aceitação: a exata mesma palavra digitada no terminal, com letras maiúsculas, caracteres especiais e sem erros ou falta de caractertes. Para confirmar isso, enviamos 5 vezes comandos em branco para leitura, e todos funcionaram perfeitamente.
  
### CT3 – Linha longa

* Entrada: Uma sequencia de caracteres excessivamente longa, cerca de 100 caracteres

* Saída esperada: inicialmente se esperava uma saida identica a entrada, mas analisando o código, foi esperado um erro, com o programa lendo apenas 32 caracteres.
 
* Saída: uma sequencia de caracteres de até 31 caracteres, isso ocorre por conta que, além da memória ser de 32 bytes(ou seja, 32 palavras), foram enviados 31 caracteres pois o último caractere sempre é \0, indicando fim da leitura.
  
* Critério de Aceitação: para validarmos que o echo bot imprime um número de caracteres igual a memória alocada a ele, realizamos diversos testes com longas sequencias de caracteres, tamém modificando a quantidade de memória dedicada a ele, validando o fato de que ele imprime no máximo a quantidade de memória associada a ele -1.

  * Pontos importantes: nesse teste é possível visualizar que o tamanho máximo de uma palavra é determinado pelo quanto de memória que é didicado a ela, que por sua vez também tem um limite, definido pelo proj.config, que tem como limite a RAM do microcontrolador.

## 3.3 Implementação

* Arquivo(s) modificados:
  
proj.config: adição de Log, quantidade de memória, ativação da UART e do console.

main: modificação na define da UART( mudamos para zephyr_console), na linha 80 substituimos return 0 para return -1, pois é mais usual para indicar erro. E por fim, atualizamos alguns printk e a interface entre o usuário e o echo.

 
* Justificativa das alterações:
  
  funcionamento do código no nosso ambiente do vs code, além da adição de mais memória para UART. Alguns comandos também(como zephyr_console) foram usados por serem mais cinfiáveis do que seus anteriores. A modificação da interface e dos printk serve para proporcionar uma melhor compreensão.



## 3.4 Evidências de Funcionamento

Salvar evidências em `docs/evidence/echo_bot/`.

Exemplo de referência no README:

```
[Link para o log CT1](docs/evidence/echo_bot/ct1_output.txt)
```

Adicionar aqui pequenos trechos ilustrativos:

```
Hello! I'm your echo bot. Tell me something and press enter:
Echo: Hello World!
```

## 3.5 Diagramas de Sequência D2

Vide material de apoio: https://d2lang.com/tour/sequence-diagrams/

Adicionar arquivos (diagrama completo e o código-base para geração do diagrama) em `docs/sequence-diagrams/`.

---

# 4. Etapa 2 – Async API (Transmissão/Recepção Assíncrona)

## 4.1 Descrição do Funcionamento

O código funciona enviando e recebendo pacotes. Para transmissão, quando há um pacote para ser enviado mas a UART está ocupasda, esse pacote é enviado para uma fila, com limite de 4 pacotes. Já para receptor não há fila, mas sim 2 buffers, que se alternam na recepção de informações a fim de evitar perda de informação. Toda essa parte é feita em uma função a parte. Já na main ocorre a criação de pacotes aleat´rios(entre 1 e 4 pacotes) e desabilita periódicamente o receptor, para demonstrar o controle da UART, impressão de mensagens de controle. Já a situação do envio e da recepção, além da impressão do pacote ocorre na função a parte.

Link usado como referência:
[https://docs.zephyrproject.org/latest/samples/drivers/uart/async_api/README.html](https://docs.zephyrproject.org/latest/samples/drivers/uart/async_api/README.html)

## 4.2 Casos de Teste Planejados (TDD)

### CT1 – Transmissão de pacotes a cada 5s

  * Entrada:
    * Saída esperada:
      * Critério de Aceitação:

### CT2 – Recepção

  * Entrada:
    * Saída esperada:
      * Critério de Aceitação:

### CT3 – Verificação de timing dos 5s

  * Entrada:
    * Saída esperada:
      * Critério de Aceitação:

(Adicionar mais casos se necessário.)

## 4.3 Implementação

* Arquivos modificados:
* Motivos/Justificativas:

## 4.4 Evidências de Funcionamento

Salvar em `docs/evidence/async_api/`.

Exemplo:

```
Loop 0:
Sending 3 packets (packet size: 5)
Packet: 0
Packet: 1
Packet: 2
```

Ou:

```
RX is now enabled
UART callback: RX_RDY
Data (HEX): 48 65 6C 6C 6F
Data (ASCII): Hello
```

## 4.5 Diagramas de Sequência D2

Vide material de referência: https://d2lang.com/tour/sequence-diagrams/

Adicionar arquivos (diagrama completo e o código-base para geração do diagrama) em `docs/sequence-diagrams/`.

---

# 5. Conclusões da Dupla

* O que deu certo: No Echo, tudo ocorreu bem, o principal erro foi sobre tamanho de mensagem, que fopi facilmente corrigido modificando a quantidade de memória disponivel para o echo.
* O que foi mais desafiador:
