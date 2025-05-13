// Painel de Automação Residencial Inteligente
// Controla LEDs RGB e matriz WS2812 via webserver HTTP, monitora temperatura e gerencia emergências
// Desenvolvido por José Vinicius 

#include <stdio.h>                     // biblioteca padrão para entrada/saída
#include <string.h>                    // manipulação de strings 
#include <stdlib.h>                    // funções padrão 
#include "pico/stdlib.h"               // funções básicas do Pico SDK 
#include "hardware/gpio.h"             // controle de GPIOs 
#include "hardware/i2c.h"              // comunicação I2C para o display OLED
#include "hardware/adc.h"              // leitura do ADC para sensor de temperatura interno
#include "pico/cyw43_arch.h"           // suporte ao módulo Wi-Fi CYW43439
#include "lwip/pbuf.h"                 // /buffers de dados para comunicação TCP
#include "lwip/tcp.h"                  // protocolo TCP para implementar o webserver
#include "lwip/netif.h"                // interface de rede para obter endereço IP
#include "generated/ws2812.pio.h"      // controlar matriz WS2812
#include "lib/ssd1306.h"               // biblioteca para display OLED SSD1306

// credenciais Wi-Fi
#define WIFI_SSID "Apartamento 01"     // SSID (nome) da rede Wi-Fi 
#define WIFI_PASSWORD "12345678"       // senha da rede Wi-Fi

// definições de pinos
#define BUTTON_A 5                     // GPIO para Botão A (liga/desliga LED)
#define BUTTON_B 6                     // GPIO para Botão B (desliga emergência)
#define WS2812_PIN 7                   // GPIO para matriz de LEDs WS2812
#define BUZZER 10                      // GPIO para buzzer (alarme de emergência)
#define LED_G 11                       // GPIO do LED RGB verde
#define LED_B 12                       // GPIO do LED RGB azul
#define LED_R 13                       // GPIO do LED RGB vermelho
#define I2C_SDA 14                     // GPIO para pino SDA do I2C (OLED)
#define I2C_SCL 15                     // GPIO para pino SCL do I2C (OLED)  
#define I2C_PORT i2c1                  // porta I2C usada para o display OLED
#define OLED_ADDRESS 0x3C              // endereço I2C do display OLED SSD1306
#define JOYSTICK 22                    // GPIO para botão do joystick (muda cor)
#define WIDTH 128                      // largura do display OLED 
#define HEIGHT 64                      // altura do display OLED 

// variáveis globais
typedef enum { VERMELHO, VERDE, AZUL, AMARELO, CIANO, LILAS } Cor; // enum para representar cores do LED RGB
static Cor cor_atual = VERMELHO; // cor inicial do LED RGB 
static bool led_ligado = false; // estado do LED RGB e matriz (desligado)
static bool emergencia = false; // estado do modo de emergência (desativado)
static ssd1306_t disp; // estrutura para controlar o display OLED 
static uint32_t ultima_leitura_temperatura = 0; // timestamp da última leitura de temperatura
static uint32_t ultimo_botao = 0; // timestamp da última verificação de botões
static uint32_t ultima_atualizacao_oled = 0; // timestamp da última atualização do OLED
static uint32_t ultimo_buzzer = 0; // timestamp da última alternância do buzzer

// padrões da matriz de LEDs
static const int pixel_map[5][5] = { // mapeamento de índices da matriz WS2812
    {24, 23, 22, 21, 20},            // Linha 1: índices dos LEDs
    {15, 16, 17, 18, 19},            // Linha 2: índices dos LEDs
    {14, 13, 12, 11, 10},            // Linha 3: índices dos LEDs
    {5,  6,  7,  8,  9},             // Linha 4: índices dos LEDs
    {4,  3,  2,  1,  0}              // Linha 5: índices dos LEDs
}; 
static const uint8_t padrao_V[5][5] = { // padrão "V" para matriz WS2812
    {0, 0, 0, 0, 0}, 
    {1, 0, 0, 0, 1}, 
    {1, 0, 0, 0, 1}, 
    {0, 1, 0, 1, 0}, 
    {0, 0, 1, 0, 0}  
};
static const uint8_t padrao_exclamacao[5][5] = { // padrão "!" para emergência
    {0, 0, 1, 0, 0}, 
    {0, 0, 1, 0, 0}, 
    {0, 0, 1, 0, 0}, 
    {0, 0, 0, 0, 0}, 
    {0, 0, 1, 0, 0}  
};

// protótipos de funções
void inicializar_perifericos(void); // inicializa GPIOs para LED RGB, botões, e buzzer
float ler_temperatura(void); // lê temperatura do sensor interno via ADC
void configurar_led_rgb(Cor cor, bool estado); // configura LED RGB com cor e estado
void configurar_matriz(const uint8_t padrao[5][5], uint8_t r, uint8_t g, uint8_t b); // configura matriz WS2812
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err); // aceita conexões TCP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err); // processa requisições HTTP
void processar_requisicao(char **requisicao); // interpreta comandos HTTP
void atualizar_display(void); // atualiza display OLED com informações do sistema

// função principal
int main() {
    stdio_init_all(); // inicializa UART para logs no Serial Monitor 
    sleep_ms(2000); // aguarda 2 segundos para estabilizar a inicialização

    // inicializa periféricos
    inicializar_perifericos(); // configura GPIOs para LED RGB, botões, e buzzer
    adc_init(); // inicializa módulo ADC para leitura de temperatura
    adc_set_temp_sensor_enabled(true); // ativa sensor de temperatura interno do RP2040

    // inicializa I2C e OLED
    i2c_init(I2C_PORT, 400 * 1000); // configura I2C a 400kHz para comunicação rápida
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // define pino SDA como função I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // define pino SCL como função I2C
    gpio_pull_up(I2C_SDA); // habilita pull-up interno para SDA
    gpio_pull_up(I2C_SCL); // habilita pull-up interno para SCL
    sleep_ms(500); // aguarda 500ms para estabilizar I2C
    ssd1306_init(&disp, WIDTH, HEIGHT, false, OLED_ADDRESS, I2C_PORT); // inicializa estrutura do OLED
    ssd1306_config(&disp); // configura parâmetros do display OLED
    ssd1306_fill(&disp, 0); // limpa o buffer do display 
    ssd1306_send_data(&disp); // envia buffer inicial ao OLED

    // inicializa WS2812
    PIO pio = pio0; // usa PIO0 para controlar a matriz WS2812
    uint offset = pio_add_program(pio, &ws2812_program); // carrega programa PIO para WS2812
    ws2812_program_init(pio, 0, offset, WS2812_PIN, 800000, false); // inicializa WS2812 

    // inicializa Wi-Fi
    if (cyw43_arch_init()) { // innicializa módulo Wi-Fi CYW43439
        printf("Falha na inicialização do Wi-Fi\n"); // loga erro no Serial Monitor
        return -1; // encerra programa em caso de falha
    }
    cyw43_arch_enable_sta_mode(); // ativa modo estação (cliente Wi-Fi)
    printf("Conectando ao Wi-Fi...\n"); // loga tentativa de conexão
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)) { // tenta conectar com timeout de 20s
        printf("Falha na conexão Wi-Fi\n"); // loga erro se a conexão falhar
        return -1; // encerra programa em caso de falha
    }
    printf("Conectado ao Wi-Fi\n"); // confirma conexão bem-sucedida
    if (netif_default) { // verifica se a interface de rede está ativa
        printf("IP: %s\n", ipaddr_ntoa(&netif_default->ip_addr)); // exibe endereço IP no Serial Monitor
    }

    // configura servidor TCP
    struct tcp_pcb *server = tcp_new(); // cria um novo PCB (Protocol Control Block) para o webserver
    if (!server) { // verifica se a criação do PCB falhou
        printf("Falha na criação do servidor TCP\n"); // loga erro
        return -1; // encerra programa em caso de falha
    }
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK) { // associa o servidor à porta 80 (HTTP)
        printf("Falha no bind TCP\n"); // loga erro se o bind falhar
        return -1; // encerra programa em caso de falha
    }
    server = tcp_listen(server); // coloca o servidor em modo escuta
    tcp_accept(server, tcp_server_accept); // define callback para aceitar conexões
    printf("Servidor escutando na porta 80\n\n"); // loga que o servidor está ativo

    // loop principal
    while (true) {
        cyw43_arch_poll(); // processa eventos de rede (lwIP) para manter o webserver ativo
        uint32_t agora = to_ms_since_boot(get_absolute_time()); // obtém tempo atual em milissegundos

        // verifica botões a cada 10ms
        if (agora - ultimo_botao >= 10) { // verifica botões a cada 10ms para responsividade
            static bool botao_joystick_pressionado = false; // estado anterior do joystick
            static bool botao_a_pressionado = false; // estado anterior do Botão A
            static bool botao_b_pressionado = false; // estado anterior do Botão B

            bool estado_joystick = !gpio_get(JOYSTICK); // lê estado do joystick 
            bool estado_botao_a = !gpio_get(BUTTON_A); // lê estado do Botão A 
            bool estado_botao_b = !gpio_get(BUTTON_B); // lê estado do Botão B 

            // joystick: alterna cores
            if (estado_joystick && !botao_joystick_pressionado) { // detecta pressão do joystick
                cor_atual = (cor_atual + 1) % 6; // cicla para a próxima cor (0 a 5)
                printf("Botão Joystick: cor alterada para %s\n\n", // loga a nova cor
                       cor_atual == VERMELHO ? "vermelho" :
                       cor_atual == VERDE ? "verde" :
                       cor_atual == AZUL ? "azul" :
                       cor_atual == AMARELO ? "amarelo" :
                       cor_atual == CIANO ? "ciano" : "lilás");
                botao_joystick_pressionado = true; // marca joystick como pressionado
                sleep_ms(200); // debounce de 200ms para evitar múltiplas leituras
            } else if (!estado_joystick) { // joystick liberado
                botao_joystick_pressionado = false; // reseta estado do joystick
            }

            // botão A: liga/desliga LED
            if (estado_botao_a && !botao_a_pressionado) { // detecta pressão do Botão A
                led_ligado = !led_ligado; // alterna estado do LED (ligado/desligado)
                printf("Botão A: led %s\n\n", led_ligado ? "ligado" : "desligado"); // loga ação
                botao_a_pressionado = true; // marca Botão A como pressionado
                sleep_ms(200); // debounce de 200ms
            } else if (!estado_botao_a) { // botão A liberado
                botao_a_pressionado = false; // reeseta estado do Botão A
            }

            // botão B: desliga emergência
            if (estado_botao_b && !botao_b_pressionado) { // detecta pressão do Botão B
                emergencia = false; // desativa modo de emergência
                printf("Botão B: alarme desligado\n\n"); // loga ação
                botao_b_pressionado = true; // marca Botão B como pressionado
                sleep_ms(200); // debounce de 200ms
            } else if (!estado_botao_b) { // botão B liberado
                botao_b_pressionado = false; // reseta estado do Botão B
            }

            ultimo_botao = agora; // atualiza timestamp da verificação de botões
        }

        // lê temperatura a cada 1000ms
        if (agora - ultima_leitura_temperatura >= 1000) { // verifica temperatura a cada 1s
            float temperatura = ler_temperatura(); // lê temperatura do sensor interno
            if (temperatura > 40.0f) { // ee temperatura exceder 40°C
                emergencia = true; // ativa modo de emergência
            }
            ultima_leitura_temperatura = agora; // atualiza timestamp da leitura
        }

        // atualiza display a cada 1000ms
        if (agora - ultima_atualizacao_oled >= 1000) { // atualiza OLED a cada 1s
            atualizar_display(); // exibe temperatura, emergência e IP no OLED
            ultima_atualizacao_oled = agora; // atualiza timestamp do OLED
        }

        // controla buzzer em emergência
        if (emergencia && agora - ultimo_buzzer >= 1000) { // se emergência ativa, alterna buzzer a cada 1s
            gpio_put(BUZZER, !gpio_get(BUZZER)); // inverte estado do buzzer (liga/desliga)
            ultimo_buzzer = agora; // atualiza timestamp do buzzer
        } else if (!emergencia && gpio_get(BUZZER)) { // se emergência desativada e buzzer ligado
            gpio_put(BUZZER, 0); // desliga buzzer
        }

        // atualiza LED RGB e matriz
        if (!emergencia) { // se não estiver em emergência
            configurar_led_rgb(cor_atual, led_ligado); // configura LED RGB com cor atual e estado
        } else { // em emergência
            configurar_led_rgb(cor_atual, false); // desliga LED RGB
        }
        if (emergencia) { // se emergência ativa
            configurar_matriz(padrao_exclamacao, 32, 0, 0); // exibe padrão "!" em vermelho
        } else if (led_ligado) { // se LED ligado e sem emergência
            uint8_t r = 0, g = 0, b = 0; // inicializa componentes RGB
            switch (cor_atual) { // define valores RGB com base na cor atual
                case VERMELHO: r = 32; break; // Vermelho
                case VERDE: g = 32; break; // Verde
                case AZUL: b = 32; break; // Azul
                case AMARELO: r = 32; g = 32; break; // Amarelo
                case CIANO: g = 32; b = 32; break; // Ciano
                case LILAS: r = 32; b = 32; break; // Lilás
            }
            configurar_matriz(padrao_V, r, g, b); // exibe padrão "V" na cor atual
        } else { // se LED desligado
            configurar_matriz(padrao_V, 0, 0, 0); // desliga matriz 
        }

        sleep_ms(10); // delay de 10ms para evitar sobrecarga do loop
    }

    cyw43_arch_deinit(); // desinicializa Wi-Fi 
    return 0; // retorno padrão 
}

// inicializa periféricos
void inicializar_perifericos(void) {
    gpio_init(LED_R); // inicializa GPIO do LED vermelho
    gpio_set_dir(LED_R, GPIO_OUT); // define como saída
    gpio_put(LED_R, 0); // desliga LED vermelho
    gpio_init(LED_G); // inicializa GPIO do LED verde
    gpio_set_dir(LED_G, GPIO_OUT); // define como saída
    gpio_put(LED_G, 0); // desliga LED verde
    gpio_init(LED_B); // inicializa GPIO do LED azul
    gpio_set_dir(LED_B, GPIO_OUT); // define como saída
    gpio_put(LED_B, 0); // desliga LED azul
    gpio_init(JOYSTICK); // inicializa GPIO do joystick
    gpio_set_dir(JOYSTICK, GPIO_IN); // define como entrada
    gpio_pull_up(JOYSTICK); // habilita pull-up interno
    gpio_init(BUTTON_A); // inicializa GPIO do Botão A
    gpio_set_dir(BUTTON_A, GPIO_IN); // define como entrada
    gpio_pull_up(BUTTON_A); // habilita pull-up interno
    gpio_init(BUTTON_B); // inicializa GPIO do Botão B
    gpio_set_dir(BUTTON_B, GPIO_IN); // define como entrada
    gpio_pull_up(BUTTON_B); // habilita pull-up interno
    gpio_init(BUZZER); // inicializa GPIO do buzzer
    gpio_set_dir(BUZZER, GPIO_OUT); // define como saída
    gpio_put(BUZZER, 0); // desliga buzzer
}

// lê temperatura do sensor interno
float ler_temperatura(void) {
    adc_select_input(4); // seleciona canal 4 do ADC (sensor interno)
    uint16_t valor_bruto = adc_read(); // lê valor bruto (12 bits, 0-4095)
    const float fator_conversao = 3.3f / (1 << 12); // fator para converter ADC para tensão (Vref = 3.3V)
    float temperatura = 27.0f - ((valor_bruto * fator_conversao) - 0.706f) / 0.001721f; // converte para °C (equação do RP2040)
    return temperatura; // retorna temperatura em °C
}

// configura LED RGB
void configurar_led_rgb(Cor cor, bool estado) {
    uint8_t r = 0, g = 0, b = 0; // inicializa componentes RGB como 0 
    if (estado) { // se LED deve estar ligado
        switch (cor) { // define valores RGB com base na cor
            case VERMELHO: r = 32; break; // Vermelho
            case VERDE: g = 32; break; // Verde
            case AZUL: b = 32; break; // Azul
            case AMARELO: r = 32; g = 32; break; // Amarelo
            case CIANO: g = 32; b = 32; break; // Ciano
            case LILAS: r = 32; b = 32; break; // Lilás
        }
    }
    gpio_put(LED_R, r > 0); // Liga/desliga vermelho
    gpio_put(LED_G, g > 0); // Liga/desliga verde
    gpio_put(LED_B, b > 0); // Liga/desliga azul
}

// configura matriz de LEDs
void configurar_matriz(const uint8_t padrao[5][5], uint8_t r, uint8_t g, uint8_t b) {
    uint32_t pixels[25] = {0}; // inicializa array de 25 LEDs como apagados
    for (int i = 0; i < 5; i++) { // itera sobre linhas da matriz
        for (int j = 0; j < 5; j++) { // itera sobre colunas da matriz
            if (padrao[i][j]) { // se o LED deve estar aceso 
                pixels[pixel_map[i][j]] = ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b); // define cor RGB
            }
        }
    }
    for (int i = 0; i < 25; i++) { // envia cores para cada LED
        pio_sm_put_blocking(pio0, 0, pixels[i] << 8u); // envia valor RGB para WS2812 via PIO
    }
}

// callback de aceitação de conexão TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, tcp_server_recv); // define callback para processar requisições recebidas
    return ERR_OK; // aceita conexão
}

// processa requisições HTTP
void processar_requisicao(char **requisicao) {
    if (strstr(*requisicao, "GET /led_on")) { // verifica requisição para ligar LED
        led_ligado = true; // ativa LED
    } else if (strstr(*requisicao, "GET /led_off")) { // verifica requisição para desligar LED
        led_ligado = false; // desativa LED
    } else if (strstr(*requisicao, "GET /color_red")) { // verifica requisição para cor vermelha
        cor_atual = VERMELHO; // define cor como vermelho
    } else if (strstr(*requisicao, "GET /color_green")) { // verifica requisição para cor verde
        cor_atual = VERDE; // define cor como verde
    } else if (strstr(*requisicao, "GET /color_blue")) { // verifica requisição para cor azul
        cor_atual = AZUL; // edfine cor como azul
    } else if (strstr(*requisicao, "GET /color_yellow")) { // verifica requisição para cor amarela
        cor_atual = AMARELO; // define cor como amarelo
    } else if (strstr(*requisicao, "GET /color_cyan")) { // verifica requisição para cor ciano
        cor_atual = CIANO; // define cor como ciano
    } else if (strstr(*requisicao, "GET /color_lilas")) { // verifica requisição para cor lilás
        cor_atual = LILAS; // define cor como lilás
    } else if (strstr(*requisicao, "GET /alarm_off")) { // verifica requisição para desligar alarme
        emergencia = false; // desativa emergência
    }
}

// callback de recebimento de dados TCP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) { // se não há dados (conexão fechada)
        tcp_close(tpcb); // fecha conexão
        tcp_recv(tpcb, NULL); // remove callback de recebimento
        return ERR_OK; // reetorna sucesso
    }

    char *requisicao = (char *)malloc(p->len + 1); // aloca memória para a requisição
    memcpy(requisicao, p->payload, p->len); // copia dados da requisição
    requisicao[p->len] = '\0'; // adiciona terminador nulo à string

    // traduz a requisição para mensagem 
    if (strstr(requisicao, "GET /led_on")) { // requisição para ligar LED
        printf("Requisição: led ligado\n\n"); // loga ação no Serial Monitor
    } else if (strstr(requisicao, "GET /led_off")) { // requisição para desligar LED
        printf("Requisição: led desligado\n\n"); // loga ação
    } else if (strstr(requisicao, "GET /color_red")) { // requisição para cor vermelha
        printf("Requisição: led vermelho ligado\n\n"); // loga ação
    } else if (strstr(requisicao, "GET /color_green")) { // requisição para cor verde
        printf("Requisição: led verde ligado\n\n"); // loga ação
    } else if (strstr(requisicao, "GET /color_blue")) { // requisição para cor azul
        printf("Requisição: led azul ligado\n\n"); // loga ação
    } else if (strstr(requisicao, "GET /color_yellow")) { // requisição para cor amarela
        printf("Requisição: led amarelo ligado\n\n"); // loga ação
    } else if (strstr(requisicao, "GET /color_cyan")) { // requisição para cor ciano
        printf("Requisição: led ciano ligado\n\n"); // loga ação
    } else if (strstr(requisicao, "GET /color_lilas")) { // requisição para cor lilás
        printf("Requisição: led lilás ligado\n\n"); // loga ação
    } else if (strstr(requisicao, "GET /alarm_off")) { // requisição para desligar alarme
        printf("Requisição: alarme desligado\n\n"); // loga ação
    }

    processar_requisicao(&requisicao); // processa a requisição para atualizar estados
    float temperatura = ler_temperatura(); // lê temperatura atual para exibir no HTML

    char html[1536]; // buffer para página HTML (máximo 1536 bytes)
    snprintf(html, sizeof(html), // formata página HTML com estado atual
             "HTTP/1.1 200 OK\r\n" // status HTTP 200 
             "Content-Type: text/html\r\n" // tipo de conteúdo: HTML
             "\r\n" // fim do cabeçalho HTTP
             "<!DOCTYPE html>" // declaração DOCTYPE para HTML5
             "<html>" // início do documento HTML
             "<head>" // cabeçalho HTML
             "<meta charset=\"UTF-8\">" // define codificação UTF-8 para acentos
             "<title>Painel Casa Inteligente</title>" // título da página
             "<style>" // estilos CSS
             "body{font-family:Arial;text-align:center;margin:10px;background-color:#b5e5fb}" // estiliza corpo (fonte, centralizado, fundo azul claro)
             "h1{font-size:40px}" // título com fonte 40px
             "button{font-size:32px;margin:5px;padding:5px}" // botões com fonte 32px e margens
             ".s{font-size:32px;margin:5px}" // classe .s para textos com fonte 32px
             "</style>" // fim dos estilos
             "</head>" // fim do cabeçalho
             "<body>" // início do corpo HTML
             "<h1>Painel Casa Inteligente</h1>" // título principal
             "<form action=\"./led_on\"><button>Ligar LED</button></form>" // botão para ligar LED
             "<form action=\"./led_off\"><button>Desligar LED</button></form>" // botão para desligar LED
             "<form action=\"./color_red\"><button>Vermelho</button></form>" // botão para cor vermelha
             "<form action=\"./color_green\"><button>Verde</button></form>" // botão para cor verde
             "<form action=\"./color_blue\"><button>Azul</button></form>" // botão para cor azul
             "<form action=\"./color_yellow\"><button>Amarelo</button></form>" // botão para cor amarela
             "<form action=\"./color_cyan\"><button>Ciano</button></form>" // botão para cor ciano
             "<form action=\"./color_lilas\"><button>Lilás</button></form>" // botão para cor lilás
             "<form action=\"./alarm_off\"><button>Desligar Alarme</button></form>" // botão para desligar alarme
             "<p class=s>LED: %s</p>" // exibe estado do LED (LIGADO/DESLIGADO)
             "<p class=s>Cor: %s</p>" // exibe cor atual
             "<p class=s>Temperatura: %.2fC</p>" // exibe temperatura
             "<p class=s>Emergência: %s</p>" // exibe estado da emergência (LIGADA/DESLIGADA)
             "</body>" // fim do corpo
             "</html>", // fim do documento HTML
             led_ligado ? "LIGADO" : "DESLIGADO", // estado do LED
             cor_atual == VERMELHO ? "Vermelho" : // nome da cor atual
             cor_atual == VERDE ? "Verde" :
             cor_atual == AZUL ? "Azul" :
             cor_atual == AMARELO ? "Amarelo" :
             cor_atual == CIANO ? "Ciano" : "Lilás",
             temperatura, // valor da temperatura
             emergencia ? "LIGADA" : "DESLIGADA"); // estado da emergência

    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY); // envia página HTML ao cliente
    tcp_output(tpcb); // força envio dos dados
    free(requisicao); // libera memória alocada para a requisição
    pbuf_free(p); // libera buffer da requisição
    return ERR_OK; // retorna sucesso
}

// atualiza display OLED
void atualizar_display(void) {
    char temp_str[20]; // buffer para string da temperatura
    char ip_str[16]; // buffer para string do endereço IP
    ssd1306_fill(&disp, 0); // Limpa o buffer do display 
    snprintf(temp_str, sizeof(temp_str), "TEMP: %.2fC", ler_temperatura()); 
    snprintf(ip_str, sizeof(ip_str), "%s", netif_default ? ipaddr_ntoa(&netif_default->ip_addr) : "N/A"); 
    ssd1306_draw_string(&disp, temp_str, 20, 2); // exibe temperatura na linha 1 
    ssd1306_draw_string(&disp, emergencia ? "EMERGENCIA: ON" : "EMERGENCIA: OFF", 2, 18); // exibe emergência na linha 2 
    ssd1306_draw_string(&disp, "IP P/ CONEXAO:", 6, 34); // exibe texto na linha 3 
    ssd1306_draw_string(&disp, ip_str, 6, 50); // exibe IP na linha 4 
    ssd1306_send_data(&disp); // envia buffer ao display OLED
}