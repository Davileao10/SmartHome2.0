#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

// Credenciais Wi-Fi
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "SENHA"

// Pinos
#define LED_PIN CYW43_WL_GPIO_LED_PIN
#define LED_BLUE_PIN 12
#define LED_GREEN_PIN 11
#define LED_RED_PIN 13
#define BUZZER1_PIN 10
#define BUZZER2_PIN 21

// Configurações PWM LED
#define PWM_MAX_VALUE 65535  // Valor máximo do duty cycle (16-bit)
#define PWM_LOW_VALUE 327    // ~0.5% de brilho (0.005 * 65535)
#define PWM_MEDIUM_VALUE 6553 // ~10% de brilho (0.1 * 65535)
#define PWM_HIGH_VALUE 65535 // 100% de brilho

// Ajuste de cor para garantir LED branco
#define RED_MULTIPLIER 1.0   // Igualar intensidades
#define GREEN_MULTIPLIER 1.0 // Igualar intensidades
#define BLUE_MULTIPLIER 1.0  // Igualar intensidades

// Estado global
static bool light_state = false; // Inicia desligado
static bool music_state = false;
static uint8_t light_brightness = 0; // 0=off, 1=low, 2=medium, 3=high
static uint8_t volume_level = 2; // 1=low, 2=medium (padrão), 3=high

// Variáveis para PWM dos LEDs
static uint slice_num_blue;
static uint slice_num_green;
static uint slice_num_red;
static uint channel_blue;
static uint channel_green;
static uint channel_red;

// Funções
void gpio_led_bitdog(void);
void update_leds(void);
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
void user_request(char **request);
void set_volume_level(uint8_t level);
void play_music(void);
void stop_music(void);

// Função principal
int main() {
    stdio_init_all();
    sleep_ms(5000); // Estabilizar USB
    printf("Inicializando sistema...\n");

    gpio_led_bitdog();

    printf("Inicializando cyw43_arch...\n");
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar cyw43_arch\n");
        return -1;
    }

    cyw43_arch_gpio_put(LED_PIN, 0); // LED onboard desligado
    update_leds(); // Inicializa LEDs desligados

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi: %s\n", WIFI_SSID);
    int wifi_retries = 5;
    while (wifi_retries-- > 0) {
        int err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000);
        if (!err) {
            printf("Conectado ao Wi-Fi\n");
            break;
        }
        printf("Falha na conexão Wi-Fi, tentativa %d, erro: %d\n", wifi_retries, err);
        sleep_ms(1000);
    }
    if (wifi_retries <= 0) {
        printf("Não foi possível conectar ao Wi-Fi\n");
        cyw43_arch_deinit();
        return -1;
    }

    if (netif_default) {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    } else {
        printf("Erro: netif_default não inicializado\n");
        cyw43_arch_deinit();
        return -1;
    }

    struct tcp_pcb *server = tcp_new();
    if (!server) {
        printf("Erro ao criar PCB TCP\n");
        cyw43_arch_deinit();
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao fazer bind na porta 80\n");
        tcp_close(server);
        cyw43_arch_deinit();
        return -1;
    }

    server = tcp_listen(server);
    if (!server) {
        printf("Erro ao configurar servidor para escuta\n");
        tcp_close(server);
        cyw43_arch_deinit();
        return -1;
    }
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

    while (true) {
        cyw43_arch_poll();
        if (music_state) {
            play_music();
        }
        sleep_ms(10);
    }

    cyw43_arch_deinit();
    return 0;
}

// Inicializar GPIOs e PWM
void gpio_led_bitdog(void) {
    // Configurar LEDs como PWM
    gpio_set_function(LED_RED_PIN, GPIO_FUNC_PWM);    // Vermelho (GP13, slice 6, canal B)
    gpio_set_function(LED_GREEN_PIN, GPIO_FUNC_PWM);  // Verde (GP11, slice 5, canal B)
    gpio_set_function(LED_BLUE_PIN, GPIO_FUNC_PWM);   // Azul (GP12, slice 6, canal A)
    
    // Obter slice e channel para cada LED
    slice_num_red = pwm_gpio_to_slice_num(LED_RED_PIN);
    slice_num_green = pwm_gpio_to_slice_num(LED_GREEN_PIN);
    slice_num_blue = pwm_gpio_to_slice_num(LED_BLUE_PIN);
    
    channel_red = pwm_gpio_to_channel(LED_RED_PIN);
    channel_green = pwm_gpio_to_channel(LED_GREEN_PIN);
    channel_blue = pwm_gpio_to_channel(LED_BLUE_PIN);
    
    // Depuração: Verificar slices e canais
    printf("PWM Config: Red: slice=%u, channel=%u\n", slice_num_red, channel_red);
    printf("PWM Config: Green: slice=%u, channel=%u\n", slice_num_green, channel_green);
    printf("PWM Config: Blue: slice=%u, channel=%u\n", slice_num_blue, channel_blue);
    
    // Configurar PWM para LEDs
    pwm_set_wrap(slice_num_red, PWM_MAX_VALUE);
    pwm_set_wrap(slice_num_green, PWM_MAX_VALUE);
    pwm_set_wrap(slice_num_blue, PWM_MAX_VALUE);
    
    // Iniciar com duty cycle 0 (desligado)
    pwm_set_chan_level(slice_num_red, channel_red, 0);
    pwm_set_chan_level(slice_num_green, channel_green, 0);
    pwm_set_chan_level(slice_num_blue, channel_blue, 0);
    
    // Habilitar PWM
    pwm_set_enabled(slice_num_red, true);
    pwm_set_enabled(slice_num_green, true);
    pwm_set_enabled(slice_num_blue, true);

    // Configurar PWM para buzzers
    gpio_set_function(BUZZER1_PIN, GPIO_FUNC_PWM); // GP14, slice 7, canal A
    gpio_set_function(BUZZER2_PIN, GPIO_FUNC_PWM); // GP21, slice 2, canal B
    uint slice_num1 = pwm_gpio_to_slice_num(BUZZER1_PIN);
    uint slice_num2 = pwm_gpio_to_slice_num(BUZZER2_PIN);
    printf("PWM Config: Buzzer1: slice=%u, Buzzer2: slice=%u\n", slice_num1, slice_num2);
    pwm_set_clkdiv(slice_num1, 80.0f); // Médio (padrão)
    pwm_set_clkdiv(slice_num2, 80.0f);
    pwm_set_wrap(slice_num1, 833); // ~3 kHz
    pwm_set_wrap(slice_num2, 833);
    pwm_set_enabled(slice_num1, false);
    pwm_set_enabled(slice_num2, false);
}

// Atualizar estado dos LEDs com PWM real
void update_leds(void) {
    uint16_t base_brightness;
    uint16_t red_value, green_value, blue_value;
    
    if (!light_state) {
        // Desligado
        red_value = green_value = blue_value = 0;
    } else {
        // Escolhe o valor de brilho base adequado
        switch (light_brightness) {
            case 1:  // Baixo (~0.5%)
                base_brightness = PWM_LOW_VALUE;
                break;
            case 2:  // Médio (~10%)
                base_brightness = PWM_MEDIUM_VALUE;
                break;
            case 3:  // Alto (100%)
                base_brightness = PWM_HIGH_VALUE;
                break;
            default:
                base_brightness = 0;
                break;
        }
        
        // Aplicar multiplicadores para balancear a cor branca
        red_value = (uint16_t)(base_brightness * RED_MULTIPLIER);
        green_value = (uint16_t)(base_brightness * GREEN_MULTIPLIER);
        blue_value = (uint16_t)(base_brightness * BLUE_MULTIPLIER);
    }
    
    // Aplicar valores PWM individualmente a cada LED
    pwm_set_chan_level(slice_num_red, channel_red, red_value);
    pwm_set_chan_level(slice_num_green, channel_green, green_value);
    pwm_set_chan_level(slice_num_blue, channel_blue, blue_value);
    
    // O LED onboard não suporta PWM via API normal, então usamos digital
    cyw43_arch_gpio_put(LED_PIN, base_brightness > 0);
    
    printf("LEDs atualizados: Estado=%s, Brilho=%d, R=%u, G=%u, B=%u\n", 
           light_state ? "Ligado" : "Desligado", light_brightness, 
           red_value, green_value, blue_value);
}

// Configurar volume dos buzzers
void set_volume_level(uint8_t level) {
    uint slice_num1 = pwm_gpio_to_slice_num(BUZZER1_PIN);
    uint slice_num2 = pwm_gpio_to_slice_num(BUZZER2_PIN);
    float divisor;

    switch (level) {
        case 1: // Baixo
            divisor = 300.0f; // Quase inaudível
            break;
        case 2: // Médio
            divisor = 80.0f; // Normal
            break;
        case 3: // Alto
            divisor = 50.0f; // Forte, menos intenso
            break;
        default:
            divisor = 80.0f;
            break;
    }

    pwm_set_clkdiv(slice_num1, divisor);
    pwm_set_clkdiv(slice_num2, divisor);
    pwm_set_enabled(slice_num1, music_state); // Garantir PWM ativo se música ligada
    pwm_set_enabled(slice_num2, music_state);
    volume_level = level;
}

// Callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Processar requisições do usuário
void user_request(char **request) {
    bool update_needed = false;
    // Remover '?' para evitar falhas no strstr
    char *qmark = strchr(*request, '?');
    if (qmark) *qmark = '\0';

    if (strstr(*request, "GET /light_toggle") != NULL) {
        if (!light_state) {
            light_state = true;
            light_brightness = 2; // Ligar no médio por padrão
            printf("LUZ LIGADA (Brilho Médio)\n");
        } else {
            light_state = false;
            light_brightness = 0; // Desligar
            printf("LUZ DESLIGADA\n");
        }
        update_needed = true;
    } else if (strstr(*request, "GET /volume_low") != NULL) {
        set_volume_level(1); // Baixo
        printf("VOLUME BAIXO\n");
    } else if (strstr(*request, "GET /volume_high") != NULL) {
        set_volume_level(3); // Alto
        printf("VOLUME ALTO\n");
    } else if (strstr(*request, "GET /music_toggle") != NULL) {
        music_state = !music_state;
        if (!music_state) {
            stop_music();
            printf("MÚSICA DESLIGADA\n");
        } else {
            set_volume_level(2); // Iniciar música no volume médio
            printf("MÚSICA LIGADA\n");
        }
    } else if (strstr(*request, "GET /brightness_low") != NULL) {
        if (light_state) {
            light_brightness = 1; // Brilho baixo
            printf("BRILHO BAIXO (0.5%%)\n");
            update_needed = true;
        }
    } else if (strstr(*request, "GET /brightness_medium") != NULL) {
        if (light_state) {
            light_brightness = 2; // Brilho médio
            printf("BRILHO MÉDIO (10%%)\n");
            update_needed = true;
        }
    } else if (strstr(*request, "GET /brightness_high") != NULL) {
        if (light_state) {
            light_brightness = 3; // Brilho alto
            printf("BRILHO ALTO (100%%)\n");
            update_needed = true;
        }
    }
    
    // Só atualiza LEDs se necessário
    if (update_needed) {
        update_leds();
    }
}

// Tocar melodia com PWM
void play_music(void) {
    const int notes[] = {
        261, 261, 293, 261, 349, 330,
        261, 261, 293, 261, 392, 349,
        261, 261, 523, 440, 349, 330,
        261, 261, 523, 440, 392, 349
    };
    const int durations[] = {
        400, 400, 800, 800, 800, 800,
        400, 400, 800, 800, 800, 800,
        400, 400, 800, 800, 800, 800,
        400, 400, 800, 800, 800, 800
    };
    static int note_index = 0;
    static absolute_time_t next_note_time = 0;

    if (absolute_time_diff_us(get_absolute_time(), next_note_time) <= 0) {
        if (!music_state) return;

        int freq = notes[note_index];
        int duration = durations[note_index];

        if (freq < 2000) freq = freq * 8;

        uint slice_num1 = pwm_gpio_to_slice_num(BUZZER1_PIN);
        uint slice_num2 = pwm_gpio_to_slice_num(BUZZER2_PIN);
        pwm_set_wrap(slice_num1, 2500000 / freq);
        pwm_set_wrap(slice_num2, 2500000 / freq);
        pwm_set_chan_level(slice_num1, PWM_CHAN_A, (2500000 / freq) / 2);
        pwm_set_chan_level(slice_num2, PWM_CHAN_B, (2500000 / freq) / 2);
        pwm_set_enabled(slice_num1, true);
        pwm_set_enabled(slice_num2, true);

        next_note_time = make_timeout_time_ms(duration);
        note_index = (note_index + 1) % (sizeof(notes) / sizeof(notes[0]));
    }
}

// Parar música
void stop_music(void) {
    uint slice_num1 = pwm_gpio_to_slice_num(BUZZER1_PIN);
    uint slice_num2 = pwm_gpio_to_slice_num(BUZZER2_PIN);
    pwm_set_enabled(slice_num1, false);
    pwm_set_enabled(slice_num2, false);
}

// Callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    char *request = (char *)malloc(p->len + 1);
    if (!request) {
        printf("Erro: Falha ao alocar memória para request\n");
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);
    user_request(&request);

    // HTML compactado
    static const char *html_part1 =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "\r\n"
        "<!DOCTYPE html><html lang='pt-BR'><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>SmartHome</title>"
        "<style>"
        "body{background:#222;font-family:Arial;color:#fff;margin:0;padding:1rem;text-align:center}"
        ".container{background:#333;padding:1rem;border-radius:8px;width:90%;max-width:320px;margin:auto}"
        "h1{font-size:1.2rem;color:#ffa}"
        ".btn{display:inline-block;padding:0.5rem;border:none;border-radius:5px;background:#444;color:#fff;font-size:0.9rem;cursor:pointer}"
        ".btn:hover{background:#666}"
        "</style></head><body><div class='container'>"
        "<h1>Automação Residencial</h1>";

    static const char *html_part2 =
        "<form action='./light_toggle'><button class='btn'>💡 Luz</button></form>"
        "<form action='./brightness_low'><button class='btn'>🌑 Brilho Baixo</button></form>"
        "<form action='./brightness_medium'><button class='btn'>🌗 Brilho Médio</button></form>"
        "<form action='./brightness_high'><button class='btn'>☀ Brilho Alto</button></form>"
        "<form action='./volume_low'><button class='btn'>🔉 Vol. Baixo</button></form>"
        "<form action='./volume_high'><button class='btn'>🔊 Vol. Alto</button></form>"
        "<form action='./music_toggle'><button class='btn'>🎵 Música</button></form>"
        "</div></body></html>";

    // Enviar HTML em duas partes para reduzir uso de memória
    size_t part1_len = strlen(html_part1);
    size_t part2_len = strlen(html_part2);
    printf("Tamanho do HTML: %u bytes\n", part1_len + part2_len);

    err_t write_err = tcp_write(tpcb, html_part1, part1_len, TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
        printf("Erro ao escrever HTML parte 1: %d\n", write_err);
        free(request);
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_OK;
    }

    write_err = tcp_write(tpcb, html_part2, part2_len, TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
        printf("Erro ao escrever HTML parte 2: %d\n", write_err);
        free(request);
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_OK;
    }

    tcp_output(tpcb);
    printf("Resposta HTTP enviada com sucesso\n");

    free(request);
    pbuf_free(p);
    return ERR_OK;
}
