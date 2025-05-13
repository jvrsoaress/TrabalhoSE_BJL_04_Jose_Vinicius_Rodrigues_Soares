<img width=100% src="https://capsule-render.vercel.app/api?type=waving&color=02A6F4&height=120&section=header"/>
<h1 align="center">Embarcatech - Projeto Integrado - BitDogLab </h1>

## Objetivo do Projeto

Um sistema de automação residencial utilizando o Raspberry Pi Pico W na plaquinha BitDogLab, permitindo o controle remoto do LED RGB, a matriz WS2812 via webserver, o monitoramento de temperatura em tempo real, e o gerenciamento de alarmes de emergência quando a temperatura exceder 40°C com sinais visuais e sonoros, simulando uma casa inteligente.

## 🗒️ Lista de requisitos

- **Leitura de botões (A, B e Joystick):** Botão A (liga/desliga LED), Botão B (desliga emergência) e Joystick (muda cor);
- **Utilização da matriz de LEDs:** Exibe padrão "V" na cor selecionada ou "!" em emergências;
- **Utilização de LED RGB:** Sinaliza cores em sincronia com a matriz;
- **Display OLED (SSD1306):** Exibe temperatura, estado da emergência e endereço IP;
- **Utilização do buzzer:** Emite som intermitente em emergências;
- **Sensor de temperatura:** Monitora temperatura via ADC, ativando emergência acima de 40°C;
- **Webserver HTTP:** Controle remoto via Wi-Fi para ligar/desligar LEDs, mudar cores e desligar alarme;
- **Estruturação do projeto:** Código em C no VS Code, usando Pico SDK e lwIP, com comentários detalhados;
- **Técnicas implementadas:** Wi-Fi, ADC, UART, I2C, PIO, e debounce via software;
  

## 🛠 Tecnologias

1. **Microcontrolador:** Raspberry Pi Pico W (na BitDogLab).
2. **Display OLED SSD1306:** 128x64 pixels, conectado via I2C (GPIO 14 - SDA, GPIO 15 - SCL).
3. **Botão do Joystick:** GPIO 22 (muda cor).
4. **Botão A:** GPIO 5 (liga/desliga LED).
5. **Botão B:** GPIO 6 (desliga emergência)
6. **Matriz de LEDs:** WS2812 (GPIO 7).
7. **LED RGB:** GPIOs 11 (verde), 12 (azul), 13 (vermelho).
8. **Buzzer:** GPIO 10.
9. **Sensor de temperatura:** ADC canal 4 (sensor interno do RP2040).
10. **Linguagem de Programação:** C.
11. **Frameworks:** Pico SDK, lwIP (para webserver).


## 🔧 Funcionalidades Implementadas:

**Funções dos Componentes**

- **Matriz de LEDs (WS2812):** Mostra padrão "V" na cor selecionada quando o LED está ligado, ou "!" em vermelho durante emergências.
- **LED RGB:** Sinaliza a cor atual em sincronia com a matriz.  
- **Display OLED:** Exibe em tempo real:
  - Temperatura.
  - Estado da emergência.
  - Endereço IP para conexão.
- **Buzzer:** Emite beeps intermitentes (1s ligado, 1s desligado) em emergências.
- **Botões:** 
  - Joystick: Alterna entre as 6 cores com debounce de 200ms.
  - Botão A: Liga/desliga LED RGB e matriz.
  - Botão B: Desliga o alarme de emergência.
- **Sensor de temepatura:** Lê o sensor interno do RP2040 a cada 1s via ADC, ativando emergência se a temperatura exceder 40°C.
- **Técnicas:**
  - Usa polling (verificação a cada 10ms) para botões, com debounce via sleep_ms(200), garantindo estabilidade sem interrupções de hardware.
  - Wi-Fi via lwIP, ADC para temperatura, UART para logs, I2C para OLED, e PIO para matriz WS2812.

## 🚀 Passos para Compilação e Upload do projeto Ohmímetro com Matriz de LEDs

1. **Instale o Pico SDK:** Configure o ambiente com Pico SDK e bibliotecas lwIP.
2. **Crie uma pasta `build`**:
   ```bash
   mkdir build
   cd build
   cmake ..
   make

3. **Transferir o firmware para a placa:**

- Conectar a placa BitDogLab ao computador via USB.
- Copiar o arquivo .uf2 gerado para o drive da placa.

4. **Testar o projeto**

- Após o upload, desconecte e reconecte a placa.
- Acesse o IP exibido no display OLED (exp: http://192.168.0.102) para controlar via webserver.
- Use os botões para interação local.

🛠🔧🛠🔧🛠🔧


## 🎥 Demonstração: 

- Para ver o funcionamento do projeto, acesse o vídeo de demonstração gravado por José Vinicius em: https://youtu.be/RYN8VPMG1DI

## 💻 Desenvolvedor
 
<table>
  <tr>
    <td align="center"><img style="" src="https://avatars.githubusercontent.com/u/191687774?v=4" width="100px;" alt=""/><br /><sub><b> José Vinicius </b></sub></a><br />👨‍💻</a></td>
  </tr>
</table>
