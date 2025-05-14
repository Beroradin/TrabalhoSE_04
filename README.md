✨ Trabalho - Sistema de Monitoramento Ambiente com Raspberry Pi Pico W
<p align="center"> Repositório dedicado ao sistema de monitoramento ambiental utilizando a placa Raspberry Pi Pico W, que coleta dados de temperatura, umidade e luminosidade, exibindo-os em um display OLED e disponibilizando-os via servidor web.</p>

:clipboard: Apresentação da tarefa
Para este trabalho foi necessário implementar um sistema de monitoramento ambiental utilizando o Raspberry Pi Pico W. O sistema coleta dados de temperatura e umidade através do sensor DHT11, além de monitorar a luminosidade ambiente via um LDR conectado a uma entrada analógica. Os dados são exibidos em um display OLED SSD1306 e disponibilizados remotamente através de um servidor web integrado que roda na própria placa.

:dart: Objetivos
Implementar um sistema de monitoramento de temperatura, umidade e luminosidade
Exibir dados em tempo real no display OLED SSD1306
Disponibilizar os dados via servidor web através da conexão Wi-Fi
Acionar um buzzer quando a temperatura ultrapassar um limite pré-definido
Controlar um LED quando a luminosidade ultrapassar um limite pré-definido
Permitir ativação/desativação dos alertas através de botões físicos

:books: Descrição do Projeto
Utilizou-se a placa Raspberry Pi Pico W (que possui o microcontrolador RP2040) para criar um sistema completo de monitoramento ambiental. O sistema faz leituras de um sensor DHT11 para obter temperatura e umidade, e de um LDR para medir luminosidade. Os dados são mostrados localmente em um display OLED SSD1306 e também podem ser acessados remotamente através de um servidor web que utiliza o chip Wi-Fi integrado (CYW43) da Pico W. O sistema também implementa alertas configuráveis: quando a temperatura ultrapassa um limite, um buzzer é acionado; quando a luminosidade ultrapassa outro limite, um LED é ativado. Dois botões permitem habilitar/desabilitar esses alertas.
:walking: Integrantes do Projeto

Matheus Pereira Alves

:bookmark_tabs: Funcionamento do Projeto

O firmware inicializa os periféricos (I²C, PWM, ADC, GPIO) e estabelece conexão Wi-Fi
Um servidor TCP é configurado para responder requisições HTTP na porta 80
Em ciclos contínuos, o sistema:

Coleta dados do sensor DHT11 (temperatura e umidade)
Lê o valor do LDR através do ADC
Atualiza o display OLED com essas informações
Verifica se os valores ultrapassam os limites configurados
Aciona o buzzer se a temperatura estiver alta (quando habilitado)
Aciona o LED se a luminosidade estiver alta (quando habilitado)
Responde às requisições HTTP com uma página web contendo os dados
Os botões funcionam com debounce por software e alternam o estado dos alertas

:eyes: Observações
Foi utilizada a biblioteca lwIP para implementação do servidor web
O display SSD1306 é controlado via I²C
O buzzer é controlado via PWM para permitir ajuste de volume
O projeto implementa debounce para evitar múltiplas detecções em um único pressionamento de botão
O projeto funciona como um sistema bare-metal (sem sistema operacional)

:camera: GIF mostrando o funcionamento do programa na placa Raspberry Pi Pico W
<p align="center">
  <img src="images/trabalhose04.gif" alt="GIF" width="526px" />
</p>
:arrow_forward: Vídeo no youtube mostrando o funcionamento do programa na placa Raspberry Pi Pico W
<p align="center">
    <a href="https://www.youtube.com/watch?v=it5wfn_jv5o">Clique aqui para acessar o vídeo</a>
</p>
