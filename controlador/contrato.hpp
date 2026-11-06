#pragma once

// Regras fixas do contrato do controlador.
// Se algum valor mudar aqui, atualize docs/CONTRATO_CONTROLADOR.md e os testes.
namespace contrato {

// "const" significa que o valor nunca muda depois de criado.
// Nao precisamos de "constexpr" aqui: ele serve para calculos feitos em tempo de
// compilacao, o que nao e o objetivo destas constantes.

// Versao dos documentos JSON produzidos pelo controlador.
const int versao = 1;

// Faixas de histerese do tanque didatico.
const double nivel_liga = 45.0;
const double nivel_desliga = 55.0;
const double temperatura_liga = 24.0;
const double temperatura_desliga = 28.0;

// Limites didaticos aceitos nas leituras e nos atuadores.
const double leitura_min = 0.0;
const double leitura_max = 100.0;
const double atuador_min = 0.0;
const double atuador_max = 100.0;

// A planta zera os atuadores depois deste numero de ciclos sem id novo.
const int timeout_comando_ciclos = 5;

// Valores padrao da linha de comando.
const double intervalo_padrao_s = 1.0;
const double obsolescencia_padrao_s = 10.0;

}  // namespace contrato
