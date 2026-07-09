/* =====================================================================
   Escapa Buraco Eletrônico -  Firmware v0.9
   Arduino Uno + CNC Shield v3
   ---------------------------------------------------------------------
   Dois motores de passo controlam as duas pontas de uma barra. O jogador
   inclina a barra com 4 joysticks (digitais) para guiar a bolinha pelos
   buracos. Um contador regressivo (display TM1637) marca o tempo da
   partida e um buzzer dá os avisos sonoros.

   Resumo da fiacao (CNC Shield v3):
     - Eixo X = motor ESQUERDO (ponta esquerda da barra)
     - Eixo Y = motor DIREITO  (ponta direita da barra)
     - Os pinos do eixo Z do shield (D4 e D7) sao reaproveitados para o
       display TM1637, ja que so usamos 2 motores.
     - O pino EN (D8) liga/desliga os drivers. Atencao: e ATIVO EM NIVEL
       BAIXO -> LOW liga os motores, HIGH desliga.

   OBS. importante sobre os contadores de passo:
     Os dois motores sao montados espelhados (um de frente para o outro).
     Por isso, o mesmo sentido fisico "para cima" corresponde a niveis
     opostos de DIR em cada motor. A funcao moverMotor() normaliza isso:
     o contador SEMPRE cresce quando aquela ponta sobe, independente de
     qual motor seja. Assim podemos comparar as duas pontas diretamente.

   Depende da biblioteca: TM1637Display (instalar pelo Gerenciador de
   Bibliotecas da IDE Arduino).

    ## Autoria e licença ##
 
    * *Autoria:** Fabricio Masutti e Leonardo Gallep — **Coletivo Máquina Tudo**
    
    © 2026. Este projeto (código e documentação) é distribuído sob a licença
    **Creative Commons Atribuição-NãoComercial 4.0 Internacional (CC BY-NC 4.0)**.
    https://creativecommons.org/licenses/by-nc/4.0/deed.pt-br
    
    Você pode **compartilhar** e **adaptar** este material, desde que:
    - **dê o devido crédito** aos autores; e
    - **não o utilize para fins comerciais**.
   As bibliotecas de terceiros incluídas mantêm suas próprias licenças e não
   são cobertas por esta licença CC.

  ===================================================================== */

#include "pitches.h"
#include <TM1637Display.h>

// ---------------------------------------------------------------------
// PINOS DOS MOTORES (CNC Shield v3)
// ---------------------------------------------------------------------
#define DIR_MOTOR_DIREITO   6   // DIR  do motor direito  (eixo Y)
#define STEP_MOTOR_DIREITO  3   // STEP do motor direito  (eixo Y)
#define DIR_MOTOR_ESQUERDO  5   // DIR  do motor esquerdo (eixo X)
#define STEP_MOTOR_ESQUERDO 2   // STEP do motor esquerdo (eixo X)

#define ENABLE_MOTORES      8   // EN do shield (ATIVO EM LOW: LOW = liga)

// ---------------------------------------------------------------------
// DISPLAY TM1637 (usa os pinos do eixo Z do shield)
// ---------------------------------------------------------------------
#define CLK 4   // Clock do display
#define DIO 7   // Dados do display

TM1637Display display(CLK, DIO);

// ---------------------------------------------------------------------
// JOYSTICKS (pinos analogicos usados como entrada digital com pull-up)
//   Cada joystick fecha contra o GND, entao "pressionado" = LOW.
// ---------------------------------------------------------------------
#define JOYSTICK_EUP A0   // Motor esquerdo - sobe
#define JOYSTICK_ED  A1   // Motor esquerdo - desce
#define JOYSTICK_DUP A2   // Motor direito  - sobe
#define JOYSTICK_DD  A3   // Motor direito  - desce

// ---------------------------------------------------------------------
// SWITCHES E PERIFERICOS
// ---------------------------------------------------------------------
#define FIM_CURSO_X  9    // Fim de curso do eixo X (motor esquerdo)
#define FIM_CURSO_Y  10   // Fim de curso do eixo Y (motor direito)
#define BUZZER       11   // Buzzer de sinalizacao
#define BOTAO_INICIO 12   // Botao que (re)inicia a partida
#define SWITCH_FINAL 13   // Switch acionado quando a bola chega ao fim
#define SW_10        A4   // Switch do buraco 10 (condicao de vitoria)

// ---------------------------------------------------------------------
// PARAMETROS DO MOTOR / DO JOGO
//   Lembrete: aqui "velocidade" e o atraso (em microssegundos) entre as
//   bordas do pulso de STEP. MENOR atraso = motor MAIS RAPIDO.
// ---------------------------------------------------------------------
#define PASSOS_POR_REVOLUCAO 3200
#define MAX_DIFERENCA_PASSOS (PASSOS_POR_REVOLUCAO * 2) // tolerancia de desnivel entre as pontas
#define VELOCIDADE_MINIMA    600   // movimento lento (us por meia-onda do passo)
#define VELOCIDADE_MAXIMA    40    // movimento rapido (us por meia-onda do passo)

#define PONTO_INICIAL 0                          // posicao de repouso (contador zerado)
#define MAX_PONTOS    (PASSOS_POR_REVOLUCAO * 6.4) // teto da faixa util (~6,4 voltas)

// ---------------------------------------------------------------------
// ESTADO DO JOGO
// ---------------------------------------------------------------------
// Leituras instantaneas dos joysticks (LOW = pressionado)
int dup;   // direito  sobe
int dd;    // direito  desce
int eup;   // esquerdo sobe
int ed;    // esquerdo desce

// Contador regressivo
int minutos = 1;                                  // tempo inicial (min)
int segundos = 0;                                 // tempo inicial (seg)
unsigned long ultimoTempoContador = 0;            // marca da ultima atualizacao
const int intervaloAtualizacaoContador = 1000;    // passo do contador (1 s)

// Posicao acumulada de cada ponta da barra, em passos (ver nota no topo)
long contadorPassosDireito = 0;
long contadorPassosEsquerdo = 0;

// Flags de controle
bool inicio  = true;   // true enquanto uma partida esta em andamento
bool vitoria = false;  // true quando a bola passou pelo buraco 10 (SW_10)

// ---------------------------------------------------------------------
// moverMotor(): dispara UM passo no motor indicado.
//   dirPin/stepPin : pinos DIR e STEP do motor
//   sentido        : HIGH ou LOW (nivel do pino DIR)
//   velocidade     : atraso em us entre as bordas do pulso (menor = mais rapido)
//   contadorPassos : referencia ao contador da ponta correspondente
//
//   A normalizacao abaixo garante que o contador SEMPRE cresca quando a
//   ponta sobe, compensando o fato de os motores serem espelhados.
// ---------------------------------------------------------------------
void moverMotor(int dirPin, int stepPin, int sentido, int velocidade, long &contadorPassos) {
  digitalWrite(dirPin, sentido);
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(velocidade);
  digitalWrite(stepPin, LOW);
  delayMicroseconds(velocidade);

  // Para o motor esquerdo, LOW = sobe; para o direito, HIGH = sobe.
  if (dirPin == DIR_MOTOR_ESQUERDO && sentido == LOW)  contadorPassos++;
  if (dirPin == DIR_MOTOR_DIREITO  && sentido == HIGH) contadorPassos++;
  if (dirPin == DIR_MOTOR_ESQUERDO && sentido == HIGH) contadorPassos--;
  if (dirPin == DIR_MOTOR_DIREITO  && sentido == LOW)  contadorPassos--;
}

// ---------------------------------------------------------------------
// zerarMotores(): rotina de "homing".
//   Leva as duas pontas ate os fins de curso (define o zero), zera os
//   contadores e recua 700 passos ate a posicao de repouso.
// ---------------------------------------------------------------------
void zerarMotores() {
  digitalWrite(ENABLE_MOTORES, LOW);  // liga os motores (ativo em LOW)

  // Move ambos os eixos ate cada um tocar seu fim de curso
  while (digitalRead(FIM_CURSO_X) != LOW || digitalRead(FIM_CURSO_Y) != LOW) {
    if (digitalRead(FIM_CURSO_X) != LOW) {
      moverMotor(DIR_MOTOR_ESQUERDO, STEP_MOTOR_ESQUERDO, HIGH, 50, contadorPassosEsquerdo);
    }
    if (digitalRead(FIM_CURSO_Y) != LOW) {
      moverMotor(DIR_MOTOR_DIREITO, STEP_MOTOR_DIREITO, LOW, 50, contadorPassosDireito);
    }
  }

  delay(300);
  contadorPassosDireito  = 0;  // o fim de curso e o ponto zero
  contadorPassosEsquerdo = 0;

  // Recua 700 passos para tirar a barra de cima dos fins de curso
  for (int i = 0; i < 700; i++) {
    moverMotor(DIR_MOTOR_ESQUERDO, STEP_MOTOR_ESQUERDO, LOW,  VELOCIDADE_MINIMA / 3, contadorPassosEsquerdo);
    moverMotor(DIR_MOTOR_DIREITO,  STEP_MOTOR_DIREITO,  HIGH, VELOCIDADE_MINIMA / 3, contadorPassosDireito);
  }
}

// ---------------------------------------------------------------------
// atualizarDisplay(): mostra o tempo no formato MM:SS com ":" piscando.
// ---------------------------------------------------------------------
void atualizarDisplay(int minutos, int segundos) {
  int tempoFormatado = (minutos * 100) + segundos;            // ex.: 1 min 5 s -> 105
  display.showNumberDecEx(tempoFormatado, 0b01000000, true);  // 0b01000000 acende o ":"
}

// ---------------------------------------------------------------------
// DisplayOver(): pisca todos os segmentos do display (efeito de fim/vitoria).
// ---------------------------------------------------------------------
void DisplayOver() {
  for (int i = 0; i < 4; i++) {
    uint8_t aceso[]  = {0xff, 0xff, 0xff, 0xff};  // todos os segmentos ligados
    display.setSegments(aceso);
    delay(500);

    uint8_t apagado[] = {0x00, 0x00, 0x00, 0x00}; // todos apagados
    display.setSegments(apagado);
    delay(500);
  }
}

// ---------------------------------------------------------------------
// emitirBips(): emite 2 bips curtos (usado nos limites e no desnivel).
// ---------------------------------------------------------------------
void emitirBips() {
  for (int i = 0; i < 2; i++) {
    tone(BUZZER, 2000, 300);  // 2000 Hz por 300 ms
    delay(200);
    noTone(BUZZER);
    delay(200);
  }
}

// ---------------------------------------------------------------------
// inicializacao(): prepara uma nova partida.
// ---------------------------------------------------------------------
void inicializacao() {
  zerarMotores();                       // referencia a barra (homing)
  delay(500);
  tocarMusica();                        // jingle de inicio (arquivo tocarMusica.ino)
  minutos = 1;
  segundos = 0;
  atualizarDisplay(minutos, segundos);
  inicio  = true;
  vitoria = false;
}

// ---------------------------------------------------------------------
// gerenciarContador(): decrementa o relogio a cada 1 s (sem bloquear).
//   Ao zerar, encerra a partida (game over) e reinicia.
// ---------------------------------------------------------------------
void gerenciarContador() {
  if (millis() - ultimoTempoContador >= intervaloAtualizacaoContador) {
    ultimoTempoContador = millis();

    if (segundos == 0) {
      if (minutos > 0) {
        minutos--;
        segundos = 59;
      } else {
        // Tempo esgotado
        tocarGameOver();
        zerarMotores();
        digitalWrite(ENABLE_MOTORES, HIGH);  // desliga os motores
        inicializacao();
        return;
      }
    } else {
      segundos--;
    }
    atualizarDisplay(minutos, segundos);
  }
}

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  // Motores
  pinMode(DIR_MOTOR_DIREITO,   OUTPUT);
  pinMode(STEP_MOTOR_DIREITO,  OUTPUT);
  pinMode(DIR_MOTOR_ESQUERDO,  OUTPUT);
  pinMode(STEP_MOTOR_ESQUERDO, OUTPUT);
  pinMode(ENABLE_MOTORES,      OUTPUT);
  digitalWrite(ENABLE_MOTORES, HIGH);   // comeca com os motores desligados

  // Joysticks (entrada com pull-up: pressionado = LOW)
  pinMode(JOYSTICK_EUP, INPUT_PULLUP);
  pinMode(JOYSTICK_ED,  INPUT_PULLUP);
  pinMode(JOYSTICK_DUP, INPUT_PULLUP);
  pinMode(JOYSTICK_DD,  INPUT_PULLUP);

  // Fins de curso
  pinMode(FIM_CURSO_X, INPUT_PULLUP);
  pinMode(FIM_CURSO_Y, INPUT_PULLUP);

  // Botoes / switches do jogo
  pinMode(BOTAO_INICIO, INPUT_PULLUP);
  pinMode(SWITCH_FINAL, INPUT_PULLUP);
  pinMode(SW_10,        INPUT_PULLUP);  // <-- corrigido: este pino nao era configurado

  // Buzzer
  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);

  // Inicializacao geral
  Serial.begin(9600);                   // disponivel para depuracao no Serial Monitor
  delay(500);
  display.setBrightness(7);
  atualizarDisplay(minutos, segundos);  // mostra o tempo inicial
  inicio = false;                       // aguarda o botao de inicio
}

// =====================================================================
// LOOP PRINCIPAL
// =====================================================================
void loop() {

  // --- Botao de inicio: (re)comeca a partida ------------------------
  if (digitalRead(BOTAO_INICIO) == LOW) {
    digitalWrite(ENABLE_MOTORES, LOW);  // liga os motores
    inicializacao();
    inicio = true;
  }

  // --- Atualizacoes que so ocorrem durante a partida ----------------
  if (inicio) {
    gerenciarContador();
    dup = digitalRead(JOYSTICK_DUP);
    dd  = digitalRead(JOYSTICK_DD);
    eup = digitalRead(JOYSTICK_EUP);
    ed  = digitalRead(JOYSTICK_ED);
  }

  // Estado atual dos fins de curso
  bool fimCursoX = digitalRead(FIM_CURSO_X) == LOW;
  bool fimCursoY = digitalRead(FIM_CURSO_Y) == LOW;

  // ===================================================================
  // CONTROLE DO MOTOR DIREITO (eixo Y)
  // ===================================================================
  if (!fimCursoY && abs(contadorPassosDireito - contadorPassosEsquerdo) <= MAX_DIFERENCA_PASSOS) {
    // Movimento normal so e permitido dentro da faixa util
    if (contadorPassosDireito > (PONTO_INICIAL + 100) && contadorPassosDireito < MAX_PONTOS) {
      if (dup == LOW) {
        moverMotor(DIR_MOTOR_DIREITO, STEP_MOTOR_DIREITO, HIGH, VELOCIDADE_MAXIMA, contadorPassosDireito);
      }
      if (dd == LOW) {
        moverMotor(DIR_MOTOR_DIREITO, STEP_MOTOR_DIREITO, LOW, VELOCIDADE_MAXIMA, contadorPassosDireito);
      }
    }

    // Limite superior de software: empurra de volta para dentro da faixa
    if (contadorPassosDireito >= MAX_PONTOS) {
      for (int i = 0; i < 100; i++) {
        moverMotor(DIR_MOTOR_DIREITO, STEP_MOTOR_DIREITO, LOW, VELOCIDADE_MINIMA, contadorPassosDireito);
      }
      emitirBips();
      delay(20);
    }
  }

  // Limite inferior de software (eixo Y)
  if (contadorPassosDireito <= PONTO_INICIAL + 100) {
    for (int i = 0; i < 200; i++) {
      moverMotor(DIR_MOTOR_DIREITO, STEP_MOTOR_DIREITO, HIGH, VELOCIDADE_MINIMA, contadorPassosDireito);
    }
    emitirBips();
    delay(10);
  }

  // ===================================================================
  // CONTROLE DO MOTOR ESQUERDO (eixo X)
  // ===================================================================
  if (!fimCursoX && abs(contadorPassosEsquerdo - contadorPassosDireito) <= MAX_DIFERENCA_PASSOS) {
    if (contadorPassosEsquerdo > (PONTO_INICIAL + 100) && contadorPassosEsquerdo < MAX_PONTOS) {
      if (eup == LOW) {
        moverMotor(DIR_MOTOR_ESQUERDO, STEP_MOTOR_ESQUERDO, LOW, VELOCIDADE_MAXIMA, contadorPassosEsquerdo);  // sentido invertido (motor espelhado)
      }
      if (ed == LOW) {
        moverMotor(DIR_MOTOR_ESQUERDO, STEP_MOTOR_ESQUERDO, HIGH, VELOCIDADE_MAXIMA, contadorPassosEsquerdo);
      }
    }

    // Limite superior de software (eixo X)
    if (contadorPassosEsquerdo >= MAX_PONTOS) {
      for (int i = 0; i < 100; i++) {
        moverMotor(DIR_MOTOR_ESQUERDO, STEP_MOTOR_ESQUERDO, HIGH, VELOCIDADE_MINIMA, contadorPassosEsquerdo);
      }
      emitirBips();
      delay(20);
    }
  }

  // Limite inferior de software (eixo X)
  if (contadorPassosEsquerdo <= PONTO_INICIAL + 100) {
    for (int i = 0; i < 200; i++) {
      moverMotor(DIR_MOTOR_ESQUERDO, STEP_MOTOR_ESQUERDO, LOW, VELOCIDADE_MINIMA, contadorPassosEsquerdo);
    }
    emitirBips();
    delay(10);
  }

  // ===================================================================
  // NIVELAMENTO DA BARRA
  //   Se as duas pontas ficarem desniveladas demais, abaixa a ponta
  //   mais alta ate voltar para dentro da tolerancia.
  // ===================================================================
  long diferencaPassos = abs(contadorPassosEsquerdo - contadorPassosDireito);

  if (diferencaPassos >= MAX_DIFERENCA_PASSOS) {
    if (contadorPassosDireito > contadorPassosEsquerdo) {
      for (int i = 0; i < 20; i++) {
        moverMotor(DIR_MOTOR_DIREITO, STEP_MOTOR_DIREITO, LOW, VELOCIDADE_MINIMA, contadorPassosDireito);
      }
    } else if (contadorPassosEsquerdo > contadorPassosDireito) {
      for (int i = 0; i < 20; i++) {
        moverMotor(DIR_MOTOR_ESQUERDO, STEP_MOTOR_ESQUERDO, HIGH, VELOCIDADE_MINIMA, contadorPassosEsquerdo);
      }
    }
    emitirBips();
  } else {
    noTone(BUZZER);  // silencia quando ja esta nivelada
  }

  // ===================================================================
  // CONDICOES DE FIM DE PARTIDA
  // ===================================================================
  // Marca vitoria quando a bola passa pelo buraco 10
  if (digitalRead(SW_10) == LOW) {
    vitoria = true;   // <-- corrigido: antes era "vitoria == true" (comparacao, nao atribuicao)
  }

  // Ao acionar o switch final, decide entre vitoria e game over.
  // A vitoria e checada primeiro para nao tocar os dois sons juntos.
  if (digitalRead(SWITCH_FINAL) == LOW) {
    if (vitoria) {            // Ganhou o jogo!
      inicio = false;
      tocarMusicaParabens();
      DisplayOver();
      delay(500);
      inicializacao();
    } else if (inicio) {     // Chegou ao fim sem pontuar -> game over
      inicio = false;
      tocarGameOver();
      delay(500);
      inicializacao();
    }
  }
}
