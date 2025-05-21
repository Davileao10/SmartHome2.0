# SmartHome BitDogLab: Automação Residencial com Interface Web Avançada

Este projeto utiliza o **Raspberry Pi Pico W** para criar um sistema de automação residencial inteligente com controle de luzes e música via interface web responsiva. Com base em HTTP, o sistema permite controlar remotamente LEDs RGB e buzzers com precisão, utilizando PWM e comunicação TCP/IP.

## 🔧 Funcionalidades

* **Ligar/Desligar Luz**
* **Controle de Brilho da Luz** (baixo, médio, alto)
* **Ligar/Desligar Música**
* **Controle de Volume da Música** (baixo, médio, alto)
* Interface Web com **7 botões interativos**
* Comunicação via **HTTP (GET)** com servidor embutido no Pico W

## ⚙️ Como Funciona

1. O Raspberry Pi Pico W conecta-se à rede Wi-Fi.
2. Um servidor HTTP é iniciado na porta 80.
3. Ao acessar o IP do dispositivo (ex: `http://172.20.10.3`), é carregada uma página HTML compacta (\~350 bytes).
4. Cada botão da página envia uma requisição GET que ativa/desativa luzes ou sons.

### Exemplo de comandos:

| Ação                  | URL                  |
| --------------------- | -------------------- |
| Ligar/Desligar luz    | `/light_toggle`      |
| Brilho baixo          | `/brightness_low`    |
| Brilho médio          | `/brightness_medium` |
| Brilho alto           | `/brightness_high`   |
| Ligar/Desligar música | `/music_toggle`      |
| Volume baixo          | `/volume_low`        |
| Volume médio          | `/volume_medium`     |
| Volume alto           | `/volume_high`       |

---

## 🖥️ Interface Web

A interface HTML foi dividida em duas partes para economia de memória. Ela é simples, porém funcional, permitindo o controle completo do sistema via navegador.

---

## 🔌 Componentes Utilizados

### Raspberry Pi Pico W

* **Wi-Fi (CYW43439)**: conexão com rede local
* **Servidor Web**: gerenciado com `pico_cyw43_arch_lwip_poll`
* **PWM real**: controle de intensidade de luz e volume

### LED RGB

* **Pinos**: GP11 (verde), GP12 (azul), GP13 (vermelho)
* **Brilho**:

  * Baixo: 0.5% (`327/65535`)
  * Médio: 10% (`6553/65535`)
  * Alto: 100% (`65535/65535`)

### Buzzers

* **Pinos**: GP10 e GP21
* **Volume**:

  * Baixo: divisor PWM `300.0f` (quase inaudível)
  * Médio: `80.0f`
  * Alto: `50.0f`
* **Melodia**: loop de notas fixas

### UART Serial

* **Baud Rate**: 115200
* **Função**: logs de status no terminal (ex.: `LUZ LIGADA`, `VOLUME ALTO`)

---

## 🧪 Instalação e Execução

1. Clone o repositório:

   ```bash
   git clone https://github.com/Davileao10/SmartHome2.0.git
   ```
2. Configure o ambiente de desenvolvimento para o Raspberry Pi Pico W.
3. Compile e envie o firmware para a placa.
4. Conecte-se à rede Wi-Fi especificada.
5. Acesse o IP local do Pico W no navegador.

---

## 📺 Demonstração em Vídeo

👉 [Assista ao vídeo no Google Drive](https://drive.google.com/file/d/16cva3hpsAIX87CC1iE3LW0mABgXFW3QK/view?usp=sharing)

---

## 📂 Repositório

🔗 [https://github.com/Davileao10/SmartHome2.0](https://github.com/Davileao10/SmartHome2.0)



