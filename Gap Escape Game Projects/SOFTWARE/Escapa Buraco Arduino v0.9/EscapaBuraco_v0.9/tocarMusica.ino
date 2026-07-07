/* =====================================================================
   MUSICAS / SINAIS SONOROS  -  Ice Cold Beer
   ---------------------------------------------------------------------
   Toca pequenas melodias no buzzer. Baseado no projeto arduino-songs de
   Robson Couto (2019): https://github.com/robsoncouto/arduino-songs

   Como as melodias sao descritas:
     Cada melodia e um vetor com pares (nota, duracao).
       - nota     : frequencia (constantes NOTE_* de pitches.h; REST = silencio)
       - duracao  : 4 = seminima, 8 = colcheia, 16 = semicolcheia, etc.
                    valores NEGATIVOS indicam nota pontuada (ex.: -4).
   ===================================================================== */

#include "pitches.h"

int tempo  = 140;   // andamento (BPM): maior = mais rapido
int buzzer = 11;    // pino do buzzer (mesmo do arquivo principal)

// Duracao de uma nota inteira (semibreve) em ms, derivada do andamento
int duracaoNotaInteira = (60000 * 4) / tempo;

// --- Melodias (pares nota,duracao) -----------------------------------
int melodiaInicio[]   = { NOTE_D5, 8, NOTE_E5, 8, NOTE_F5, 8, NOTE_G5, 8, NOTE_E5, 4, NOTE_C5, 8, NOTE_D5, 2 };
int melodiaParabens[] = { NOTE_E5, -8, NOTE_E5, -8, NOTE_D5, -16, NOTE_E5, -8, NOTE_G5, -8, NOTE_E5, 4 };
int melodiaGameOver[] = { NOTE_A4, 4, NOTE_A4, 4, NOTE_A4, 4, NOTE_F4, -8, NOTE_C5, 16, NOTE_A4, 4, NOTE_F4, -8, NOTE_C5, 16, NOTE_A4, 2 };

// ---------------------------------------------------------------------
// tocarMelodia(): percorre o vetor e toca cada nota.
//   melodia  : vetor de pares (nota, duracao)
//   numNotas : quantidade de notas (vetor tem 2 valores por nota)
// ---------------------------------------------------------------------
void tocarMelodia(int melodia[], int numNotas) {
  for (int nota = 0; nota < numNotas * 2; nota += 2) {
    int divisor = melodia[nota + 1];
    int duracaoNota;

    if (divisor > 0) {
      duracaoNota = duracaoNotaInteira / divisor;          // nota normal
    } else {
      duracaoNota = (duracaoNotaInteira / abs(divisor)) * 1.5; // nota pontuada (+50%)
    }

    // Toca 90% da duracao e deixa 10% de silencio entre as notas
    tone(buzzer, melodia[nota], duracaoNota * 0.9);
    delay(duracaoNota);
    noTone(buzzer);
  }
}

// ---------------------------------------------------------------------
// Funcoes chamadas pelo arquivo principal.
//   sizeof(vetor)/sizeof(vetor[0])/2 = numero de notas do vetor.
// ---------------------------------------------------------------------
void tocarMusica() {
  tocarMelodia(melodiaInicio, sizeof(melodiaInicio) / sizeof(melodiaInicio[0]) / 2);
}

void tocarMusicaParabens() {
  tocarMelodia(melodiaParabens, sizeof(melodiaParabens) / sizeof(melodiaParabens[0]) / 2);
}

void tocarGameOver() {
  tocarMelodia(melodiaGameOver, sizeof(melodiaGameOver) / sizeof(melodiaGameOver[0]) / 2);
}
