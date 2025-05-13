#include <stdio.h>               // Biblioteca padrão para entrada e saída
#include <string.h>              // Biblioteca para manipulação de strings
#include <stdlib.h>              // Funções genéricas (malloc, free, etc.)
#include "pico/stdlib.h"         // Funções da SDK da Pico (GPIO, tempo, UART, etc.)
#include "hardware/adc.h"        // Controle do ADC integrado da Pico
#include "pico/cyw43_arch.h"     // Suporte ao chip Wi-Fi CYW43 da Pico W
#include "lwip/pbuf.h"           // Estruturas de buffer de rede (Lightweight IP)
#include "lwip/tcp.h"            // Implementação do protocolo TCP (Lightweight IP)
#include "lwip/netif.h"          // Interfaces de rede para LWIP
#include <dht.h>                 // Biblioteca para sensores DHT11 e DHT22
#include "hardware/i2c.h"        // Controle do barramento I²C da Pico
#include "hardware/pwm.h"        // Geração de sinais PWM
#include "hardware/timer.h"      // Temporizadores de hardware
#include "hardware/clocks.h"     // Controle de relógios internos
#include "hardware/gpio.h"       // Controle de pinos GPIO
#include "lib/ssd/ssd1306.h"     // Driver para display OLED SSD1306
#include "lib/ssd/font.h"        // Fonte bitmap para o SSD1306

// Credenciais WIFI
#define WIFI_SSID "SEU SSID"
#define WIFI_PASSWORD "SUA SENHA"

// Constantes e definições
#define LED_PIN CYW43_WL_GPIO_LED_PIN   // GPIO do CI CYW43
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define WIDTH 128    // Largura padrão do display SSD1306
#define HEIGHT 64    // Altura padrão do display SSD1306
#define BUZZER 21
#define BUTTONA_PIN 5  // Botão A
#define BUTTONB_PIN 6 // Botão B
#define ADC_PIN 28 // GPIO para o LDR
#define RED_PIN  13
#define TEMP_MAX 31.0f // Temperatura máxima em Celsius
#define LUM_MAX 3500.0f // Valor máximo do LDR (ajustar conforme necessário)

// Estrutura do display SSD1306
ssd1306_t ssd;   

// Variáveis globais para armazenar valores dos sensores
float g_temperature_c = 0.0f;
float g_humidity = 0.0f;
float g_adc_value = 0.0f;

// Modelo do sensor DHT e pino de dados - mudar caso seja necessário
static const dht_model_t DHT_MODEL = DHT11;
static const uint DATA_PIN = 16;

// Variáveis globais de uso geral
volatile bool stateBuzzer = true; // Estado do buzzer
volatile bool stateLED = true; // Estado do LED
absolute_time_t last_interrupt_time = 0;

// Definindo a frequência desejada
#define PWM_FREQ_BUZZER 1000  // 1 kHz
#define PWM_WRAP   255   // 8 bits de wrap (256 valores)

// Protipótipos de funções
static float celsius_to_fahrenheit(float temperature);
void init_i2c();
void init_pwm();
void configurarBuzzer(uint32_t volume);
void gpio_callback(uint gpio, uint32_t events);
void configurarLED(bool enableLED);
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

// Função principal
int main()
{
    // Inicialização de alguns fatores do sistema
    sleep_ms(500);
    stdio_init_all();
    init_i2c(); // Inicializa o I2C
    init_pwm(); // Inicializa o PWM
    bool cor = true;
    bool enable = false;
    uint32_t volume = 0;
    adc_init();
    adc_gpio_init(ADC_PIN); // GPIO 28 como entrada analógica
    adc_select_input(2);

    // Configuração de botões, IRQ e LED
    gpio_init(BUTTONA_PIN);
    gpio_set_dir(BUTTONA_PIN, GPIO_IN);
    gpio_pull_up(BUTTONA_PIN);
    gpio_init(BUTTONB_PIN);
    gpio_set_dir(BUTTONB_PIN, GPIO_IN);
    gpio_pull_up(BUTTONB_PIN);
    gpio_set_irq_enabled(BUTTONA_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BUTTONB_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_callback(gpio_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);
    gpio_init(RED_PIN);
    gpio_set_dir(RED_PIN, GPIO_OUT);
    gpio_put(RED_PIN, 0);

    // Inicialização do DHT
    dht_t dht;
    dht_init(&dht, DHT_MODEL, pio0, DATA_PIN, true /* pull_up */);

    char temperatura_C[16], temperatura_F[16], umidade[16]; // Mostrar os valores no display OLED

    //Inicializa a arquitetura do cyw43
    while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    // GPIO do CI CYW43 em nível baixo
    cyw43_arch_gpio_put(LED_PIN, 0);
    cyw43_arch_enable_sta_mode();

    // Conectar à rede WiFi - no meu caso que é WPA/WPA2, utilizei MIXED_PSK, mudar conforme necessidade
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_MIXED_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

    // Caso seja a interface de rede padrão - imprimir o IP do dispositivo.
    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Configura o servidor TCP - cria novos PCBs TCP. É o primeiro passo para estabelecer uma conexão TCP.
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    //vincula um PCB (Protocol Control Block) TCP a um endereço IP e porta específicos.
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    // Coloca um PCB (Protocol Control Block) TCP em modo de escuta, permitindo que ele aceite conexões de entrada.
    server = tcp_listen(server);

    // Define uma função de callback para aceitar conexões TCP de entrada. É um passo importante na configuração de servidores TCP.
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");


    while (true) {

        // Limpa o buffer do display
        ssd1306_fill(&ssd, false);
        ssd1306_send_data(&ssd); // Envia os dados para o display
        dht_start_measurement(&dht);
        
        // Obtém os dados do sensor DHT11
        dht_result_t result = dht_finish_measurement_blocking(&dht, &g_humidity, &g_temperature_c);
        if (result == DHT_RESULT_OK) {
            printf("%.1f C (%.1f F), %.1f%% Umidade\n", g_temperature_c, celsius_to_fahrenheit(g_temperature_c), g_humidity);
            
            // Preparando as strings para exibição
            strcpy(temperatura_C, "Temp C: ");
            sprintf(temperatura_C + strlen(temperatura_C), "%.1f", g_temperature_c);
            
            strcpy(temperatura_F, "Temp F: ");
            sprintf(temperatura_F + strlen(temperatura_F), "%.1f", celsius_to_fahrenheit(g_temperature_c));
            
            strcpy(umidade, "Umid: ");
            sprintf(umidade + strlen(umidade), "%.1f%%", g_humidity);
        } else if (result == DHT_RESULT_TIMEOUT) {
            printf("Sensor DHT não respondeu, veja as conexoes");
        } else {
            assert(result == DHT_RESULT_BAD_CHECKSUM);
            printf("Erro de checksum, sensor DHT com defeito");
        }

        // Leitura do valor ADC
        adc_select_input(2); // Seleciona o canal ADC para o pino 28
        g_adc_value = adc_read(); // Lê o valor ADC
        printf("Valor ADC: %.1f\n", g_adc_value);

        // Desenha os elementos no display
        ssd1306_draw_string(&ssd, "EMBARCATECH", 8, 6);        // Desenha uma string
        ssd1306_draw_string(&ssd, "Sensores", 20, 16);         // Desenha uma string
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);               // Desenha uma linha
        ssd1306_draw_string(&ssd, temperatura_C, 6, 30);       // Desenha uma string
        ssd1306_draw_string(&ssd, temperatura_F, 6, 40);       // Desenha uma string
        ssd1306_draw_string(&ssd, umidade, 6, 50);             // Desenha uma string

        // Envia os dados para o display
        ssd1306_send_data(&ssd);

        if (stateBuzzer && g_temperature_c > TEMP_MAX) {
            volume = 3;  // Liga o buzzer bem baixinho
        }
        else {
            volume = 0; // Desliga o buzzer
        }
        configurarBuzzer(volume);

        sleep_ms(500);

        if (stateLED && g_adc_value > LUM_MAX) { // LDR pode ser ajustado conforme iluminação ambiente
            enable = true; // Liga o LED
        }
        else {
            enable = false; // Desliga o LED
        }
        configurarLED(enable);

        cyw43_arch_poll(); // Necessário para manter o Wi-Fi ativo
        sleep_ms(100); 
    }
    //Desligar a arquitetura CYW43.
    cyw43_arch_deinit();
    return 0;
}

// -------------------------------------- Funções ---------------------------------
// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Alocação do request na memória dinámica
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    // Cria a resposta HTML
    char html[2056];

    // Instruções html do webserver - exibindo sensores
    snprintf(html, sizeof(html),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n"
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<title>Embarcatech - Monitoramento</title>\n"
             "<style>\n"
             "body { background-color:rgb(119, 136, 233); font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
             "h1 { font-size: 60px; margin-bottom: 30px; }\n"
             ".sensor-box { background-color: #f0f0f0; border-radius: 15px; padding: 20px; margin: 20px auto; width: 80%%; max-width: 600px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }\n"
             ".sensor-value { font-size: 48px; margin: 10px 0; color: #333; }\n"
             ".sensor-label { font-size: 24px; color: #666; }\n"
             "</style>\n"
             "<meta http-equiv=\"refresh\" content=\"2\">\n"
             "</head>\n"
             "<body>\n"
             "<h1>Embarcatech: Monitoramento de Ambiente</h1>\n"
             "<div class=\"sensor-box\">\n"
             "  <div class=\"sensor-label\">Temperatura DHT11</div>\n"
             "  <div class=\"sensor-value\">%.1f &deg;C</div>\n"
             "</div>\n"
             "<div class=\"sensor-box\">\n"
             "  <div class=\"sensor-label\">Umidade DHT11</div>\n"
             "  <div class=\"sensor-value\">%.1f %%</div>\n"
             "</div>\n"
             "<div class=\"sensor-box\">\n"
             "  <div class=\"sensor-label\">Valor ADC (Pino 28)</div>\n"
             "  <div class=\"sensor-value\">%.1f</div>\n"
             "</div>\n"
             "</body>\n"
             "</html>\n",
             g_temperature_c, g_humidity, g_adc_value);

    // Escreve dados para envio (mas não os envia imediatamente).
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);

    // Envia a mensagem
    tcp_output(tpcb);

    //libera memória alocada dinamicamente
    free(request);
    
    //libera um buffer de pacote (pbuf) que foi alocado anteriormente
    pbuf_free(p);

    return ERR_OK;
}

// Converte Celsius para Fahrenheit
static float celsius_to_fahrenheit(float temperature) {
    return temperature * (9.0f / 5) + 32;
}

// Inicializa o barramento I2C
void init_i2c(){

    // Inicialização do I2C usando 400KHz.
   i2c_init(I2C_PORT, 400 * 2000);

   gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
   gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
   gpio_pull_up(I2C_SDA);                                        // Pull up the data line
   gpio_pull_up(I2C_SCL);                                        // Pull up the clock line                   
   ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
   ssd1306_config(&ssd);                                         // Configura o display
   ssd1306_send_data(&ssd);                                      // Envia os dados para o display

   // Limpa o display. O display inicia com todos os pixels apagados.
   ssd1306_fill(&ssd, false);
   ssd1306_send_data(&ssd);
}

// Configura o volume do buzzer
void configurarBuzzer(uint32_t volume) {
    pwm_set_gpio_level(BUZZER, volume);
}

// Configura o LED
void configurarLED(bool enableLED) {
    if (enableLED) {
        gpio_put(RED_PIN, 1); // Liga o LED
    } else {
        gpio_put(RED_PIN, 0); // Desliga o LED
    }
}

void init_pwm() {
    
    // Inicializa o PWM para o buzzer
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    
    // Obtém os números dos canais PWM para os pinos
    uint slice_num_buzzer = pwm_gpio_to_slice_num(BUZZER);
    
    // Configuração da frequência PWM
    pwm_set_clkdiv(slice_num_buzzer, (float)clock_get_hz(clk_sys) / PWM_FREQ_BUZZER / (PWM_WRAP + 1));
    
    // Configura o wrap do contador PWM para 8 bits (256)
    pwm_set_wrap(slice_num_buzzer, PWM_WRAP);
    
    // Habilita o PWM
    pwm_set_enabled(slice_num_buzzer, true);
}

// Configuração do GPIO para interrupção
void gpio_callback(uint gpio, uint32_t events) {
    absolute_time_t now = get_absolute_time();
    int64_t diff = absolute_time_diff_us(last_interrupt_time, now);

    if (diff < 250000) return;
    last_interrupt_time = now;

    if (gpio == BUTTONA_PIN) {
        stateBuzzer = !stateBuzzer; // Alterna o estado do buzzer
    }
    else if (gpio == BUTTONB_PIN) {
        stateLED = !stateLED; // Alterna o estado do LED
    }   
}