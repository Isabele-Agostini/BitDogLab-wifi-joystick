#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>

#define JOY_X_PIN 26
#define JOY_Y_PIN 27
#define WIFI_SSID "Familia Silva"
#define WIFI_PASS "saladadefruta"

uint16_t joystick_x = 0;
uint16_t joystick_y = 0;
char joystick_direction[20] = "Centro";
char http_response[4096];

void read_joystick() {
    adc_select_input(0);
    joystick_x = adc_read();

    adc_select_input(1);
    joystick_y = adc_read();
}

void detect_joystick_direction() {
    int center = 2048;
    int deadzone = 500;

    int x = (int)joystick_x - center;
    int y = (int)joystick_y - center;

    if (abs(x) < deadzone && abs(y) < deadzone) {
        strcpy(joystick_direction, "Centro");
    } else if (abs(x) < deadzone && y > deadzone) {
        strcpy(joystick_direction, "Norte");
    } else if (abs(x) < deadzone && y < -deadzone) {
        strcpy(joystick_direction, "Sul");
    } else if (x > deadzone && abs(y) < deadzone) {
        strcpy(joystick_direction, "Leste");
    } else if (x < -deadzone && abs(y) < deadzone) {
        strcpy(joystick_direction, "Oeste");
    } else if (x > deadzone && y > deadzone) {
        strcpy(joystick_direction, "Nordeste");
    } else if (x > deadzone && y < -deadzone) {
        strcpy(joystick_direction, "Sudeste");
    } else if (x < -deadzone && y > deadzone) {
        strcpy(joystick_direction, "Noroeste");
    } else if (x < -deadzone && y < -deadzone) {
        strcpy(joystick_direction, "Sudoeste");
    }
}

void create_http_response() {
    snprintf(http_response, sizeof(http_response),
    "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css\" rel=\"stylesheet\">"
    "<style>"
    ".compass { display: grid; grid-template-columns: repeat(3, 80px); grid-gap: 10px; justify-content: center; align-items: center; margin-top: 20px; }"
    ".direction { background: #f8f9fa; padding: 20px; border-radius: 8px; font-weight: bold; box-shadow: 2px 2px 5px rgba(0,0,0,0.1); }"
    ".active { background-color: #0d6efd; color: white; }"
    "</style></head><body class=\"text-center p-4\">"

    "<h1 class=\"mb-4\">Direção do Joystick</h1>"
    "<p><strong>X:</strong> %d</p><p><strong>Y:</strong> %d</p>"
    "<h2 class=\"mt-3\">Direção: <span class=\"text-primary\">%s</span></h2>"

    "<div class=\"compass\">"
        "<div class=\"direction %s\">Noroeste</div>"
        "<div class=\"direction %s\">Norte</div>"
        "<div class=\"direction %s\">Nordeste</div>"
        "<div class=\"direction %s\">Oeste</div>"
        "<div class=\"direction %s\">Centro</div>"
        "<div class=\"direction %s\">Leste</div>"
        "<div class=\"direction %s\">Sudoeste</div>"
        "<div class=\"direction %s\">Sul</div>"
        "<div class=\"direction %s\">Sudeste</div>"
    "</div>"

    "<a href=\"/\" class=\"btn btn-primary mt-4\">Atualizar</a>"

    "</body></html>\r\n",
    joystick_x, joystick_y, joystick_direction,
    strcmp(joystick_direction, "Noroeste") == 0 ? "active" : "",
    strcmp(joystick_direction, "Norte") == 0 ? "active" : "",
    strcmp(joystick_direction, "Nordeste") == 0 ? "active" : "",
    strcmp(joystick_direction, "Oeste") == 0 ? "active" : "",
    strcmp(joystick_direction, "Centro") == 0 ? "active" : "",
    strcmp(joystick_direction, "Leste") == 0 ? "active" : "",
    strcmp(joystick_direction, "Sudoeste") == 0 ? "active" : "",
    strcmp(joystick_direction, "Sul") == 0 ? "active" : "",
    strcmp(joystick_direction, "Sudeste") == 0 ? "active" : ""
    );
}

err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    create_http_response();
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);
    pbuf_free(p);
    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);
    return ERR_OK;
}

static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) return;
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) return;
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
}

int main() {
    stdio_init_all();
    sleep_ms(5000);
    printf("Iniciando servidor HTTP\n");

    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    } else {
        printf("Wi-Fi conectado com sucesso!\n");
        uint8_t *ip = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    }

    adc_init();
    adc_gpio_init(JOY_X_PIN);
    adc_gpio_init(JOY_Y_PIN);

    start_http_server();

    while (true) {
        cyw43_arch_poll();
        read_joystick();
        detect_joystick_direction();
        sleep_ms(200);
    }

    cyw43_arch_deinit();
    return 0;
}
