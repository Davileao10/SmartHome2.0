/**
 * AULA IoT - Embarcatech - Ricardo Prates - 004 - Webserver Raspberry Pi Pico W - wlan
 * Modificado para smarthome com luz digital (pseudo-PWM), buzzers com PWM, brilho e volume ajustados
 */
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
#define WIFI_SSID "ABRAAO-10-2G"
#define WIFI_PASSWORD "rosana5478"

// Pinos
#define LED_PIN CYW43_WL_GPIO_LED_PIN
#define LED_BLUE_PIN 12
#define LED_GREEN_PIN 11
#define LED_RED_PIN 13
#define BUZZER1_PIN 10
#define BUZZER2_PIN 21

// Estado global
static bool light_state = false; // Inicia desligado
static bool music_state = false;
static uint8_t light_brightness = 0; // 0=off, 1=low, 2=medium, 3=high
static uint8_t volume_level = 2; // 1=low, 2=medium (padrão), 3=high
static absolute_time_t last_pwm_toggle = 0;
static bool led_on = false;

// Funções
void gpio_led_bitdog(void);
void update_led_pwm(void);
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

    gpio_led_bitdog();

    while (cyw43_arch_init()) {
        sleep_ms(100);
        return -1;
    }

    cyw43_arch_gpio_put(LED_PIN, 0); // LED onboard desligado
    gpio_put(LED_BLUE_PIN, false);
    gpio_put(LED_GREEN_PIN, false);
    gpio_put(LED_RED_PIN, false); // Garantir LEDs desligados

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        sleep_ms(100);
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

    if (netif_default) {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    struct tcp_pcb *server = tcp_new();
    if (!server) {
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK) {
        return -1;
    }

    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

    while (true) {
        cyw43_arch_poll();
        if (music_state) {
            play_music();
        }
        if (light_state) {
            update_led_pwm();
        } else {
            // Garantir LEDs apagados
            gpio_put(LED_BLUE_PIN, false);
            gpio_put(LED_GREEN_PIN, false);
            gpio_put(LED_RED_PIN, false);
            cyw43_arch_gpio_put(LED_PIN, false);
            led_on = false;
        }
        sleep_ms(10);
    }

    cyw43_arch_deinit();
    return 0;
}

// Inicializar GPIOs
void gpio_led_bitdog(void) {
    // Configurar LEDs como GPIO digital
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);

    // Configurar PWM para buzzers
    gpio_set_function(BUZZER1_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BUZZER2_PIN, GPIO_FUNC_PWM);
    uint slice_num1 = pwm_gpio_to_slice_num(BUZZER1_PIN);
    uint slice_num2 = pwm_gpio_to_slice_num(BUZZER2_PIN);
    pwm_set_clkdiv(slice_num1, 80.0f); // Médio (padrão)
    pwm_set_clkdiv(slice_num2, 80.0f);
    pwm_set_wrap(slice_num1, 833); // ~3 kHz
    pwm_set_wrap(slice_num2, 833);
    pwm_set_enabled(slice_num1, false);
    pwm_set_enabled(slice_num2, false);
}

// Atualizar LEDs com pseudo-PWM
void update_led_pwm(void) {
    int on_time_ms, off_time_ms;

    switch (light_brightness) {
        case 1: // Baixo (~1%)
            on_time_ms = 5;
            off_time_ms = 495;
            break;
        case 2: // Médio (~50%)
            on_time_ms = 250;
            off_time_ms = 250;
            break;
        case 3: // Alto (100%)
            on_time_ms = 500;
            off_time_ms = 0;
            break;
        default: // Desligado
            on_time_ms = 0;
            off_time_ms = 500;
            break;
    }

    if (absolute_time_diff_us(get_absolute_time(), last_pwm_toggle) / 1000 >= (led_on ? on_time_ms : off_time_ms)) {
        led_on = !led_on;
        bool state = led_on && light_state;
        gpio_put(LED_BLUE_PIN, state);
        gpio_put(LED_GREEN_PIN, state);
        gpio_put(LED_RED_PIN, state);
        cyw43_arch_gpio_put(LED_PIN, state);
        last_pwm_toggle = get_absolute_time();
    }
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
    // Remover '?' para evitar falhas no strstr
    char *qmark = strchr(*request, '?');
    if (qmark) *qmark = '\0';

    if (strstr(*request, "GET /light_toggle") != NULL) {
        if (!light_state) {
            light_state = true;
            light_brightness = 2; // Ligar no médio
            gpio_put(LED_BLUE_PIN, true); // Forçar ligado
            gpio_put(LED_GREEN_PIN, true);
            gpio_put(LED_RED_PIN, true);
            cyw43_arch_gpio_put(LED_PIN, true);
            printf("LUZ LIGADA\n");
        } else {
            light_state = false;
            light_brightness = 0; // Desligar
            gpio_put(LED_BLUE_PIN, false);
            gpio_put(LED_GREEN_PIN, false);
            gpio_put(LED_RED_PIN, false);
            cyw43_arch_gpio_put(LED_PIN, false);
            printf("LUZ DESLIGADA\n");
        }
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
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    user_request(&request);

    char html[900];
    snprintf(html, sizeof(html),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=UTF-8\r\n"
             "\r\n"
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "<meta charset=\"UTF-8\">"
             "<title>SmartHome - BitDogLab</title>"
             "<style>"
             "body{background:#000;font-family:Arial;text-align:center;margin:20px}"
             "h1{font-size:36px;color:#fff;margin-bottom:20px}"
             "button{font-size:20px;padding:8px 16px;margin:8px;border:1px solid #fff;background:#d3d3d3;color:#000}"
             "</style>"
             "</head>"
             "<body>"
             "<h1>Automação Residencial</h1>"
             "<form action=\"./light_toggle\"><button>Ligar/Desligar Luz</button></form>"
             "<form action=\"./volume_low\"><button>Volume Baixo</button></form>"
             "<form action=\"./volume_high\"><button>Volume Alto</button></form>"
             "<form action=\"./music_toggle\"><button>Ligar/Desligar Música</button></form>"
             "</body>"
             "</html>");

    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    free(request);
    pbuf_free(p);

    return ERR_OK;
}
