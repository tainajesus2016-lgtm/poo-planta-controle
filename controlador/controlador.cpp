#include "controlador.hpp"

#include "arquivos.hpp"
#include "contrato.hpp"
#include "estrategia.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Funcoes auxiliares deste arquivo: alarmes e idade do timestamp.

const Leitura* encontrar_leitura(const Aquisicao& aquisicao,
                                 const std::string& tag) {
    for (const Leitura& leitura : aquisicao.leituras) {
        if (leitura.tag == tag) {
            return &leitura;
        }
    }
    return nullptr;
}

void adicionar_alarme(std::vector<Alarme>& alarmes, const std::string& codigo,
                      const std::string& mensagem,
                      const std::string& severidade) {
    Alarme alarme;
    alarme.codigo = codigo;
    alarme.mensagem = mensagem;
    alarme.severidade = severidade;
    alarmes.push_back(alarme);
}

int ler_dois_digitos_utc(const std::string& texto, std::size_t posicao) {
    return (texto[posicao] - '0') * 10 + (texto[posicao + 1] - '0');
}

// Converte uma data para o numero de dias desde 1970-01-01.
// Algoritmo de Howard Hinnant, valido tambem para datas anteriores a 1970.
long long dias_desde_1970(int ano, int mes, int dia) {
    long long a = ano;
    long long m = mes;
    if (m <= 2) {
        a = a - 1;
        m = m + 12;
    }

    const long long era = (a >= 0 ? a : a - 399) / 400;
    const long long ano_era = a - era * 400;
    const long long dia_do_ano = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + dia - 1;
    const long long dia_era =
        ano_era * 365 + ano_era / 4 - ano_era / 100 + dia_do_ano;
    return era * 146097 + dia_era - 719468;
}

long long segundos_do_timestamp(const std::string& texto) {
    const int ano = (texto[0] - '0') * 1000 + (texto[1] - '0') * 100 +
                    (texto[2] - '0') * 10 + (texto[3] - '0');
    const int mes = ler_dois_digitos_utc(texto, 5);
    const int dia = ler_dois_digitos_utc(texto, 8);
    const int hora = ler_dois_digitos_utc(texto, 11);
    const int minuto = ler_dois_digitos_utc(texto, 14);
    const int segundo = ler_dois_digitos_utc(texto, 17);

    return dias_desde_1970(ano, mes, dia) * 86400 + hora * 3600 + minuto * 60 +
           segundo;
}

long long idade_dos_dados(const std::string& timestamp) {
    const long long agora = static_cast<long long>(std::time(nullptr));
    return agora - segundos_do_timestamp(timestamp);
}

bool dados_obsoletos(const Aquisicao& aquisicao, double obsolescencia) {
    if (obsolescencia <= 0.0) {
        return false;
    }
    return idade_dos_dados(aquisicao.timestamp) > obsolescencia;
}

std::vector<Alarme> calcular_alarmes(const Aquisicao& aquisicao,
                                     double obsolescencia) {
    std::vector<Alarme> alarmes;

    const Leitura* nivel = encontrar_leitura(aquisicao, "LT-01");
    const Leitura* temperatura = encontrar_leitura(aquisicao, "TT-01");

    if (nivel != nullptr && nivel->valor.has_value()) {
        if (nivel->valor.value() < contrato::nivel_liga) {
            adicionar_alarme(alarmes, "NIVEL_BAIXO",
                             "Nivel abaixo da faixa operacional", "aviso");
        }
        if (nivel->valor.value() > contrato::nivel_desliga) {
            adicionar_alarme(alarmes, "NIVEL_ALTO",
                             "Nivel acima da faixa operacional", "aviso");
        }
    }

    if (temperatura != nullptr && temperatura->valor.has_value()) {
        if (temperatura->valor.value() < contrato::temperatura_liga) {
            adicionar_alarme(alarmes, "TEMPERATURA_BAIXA",
                             "Temperatura abaixo da faixa operacional", "aviso");
        }
        if (temperatura->valor.value() > contrato::temperatura_desliga) {
            adicionar_alarme(alarmes, "TEMPERATURA_ALTA",
                             "Temperatura acima da faixa operacional", "aviso");
        }
    }

    bool leitura_em_falha = false;
    for (const Leitura& leitura : aquisicao.leituras) {
        if (leitura.status == "falha") {
            leitura_em_falha = true;
        }
    }
    if (leitura_em_falha) {
        adicionar_alarme(alarmes, "LEITURA_FALHA",
                         "Ha leitura com status de falha", "critico");
    }

    if (nivel == nullptr || temperatura == nullptr) {
        adicionar_alarme(alarmes, "LEITURA_AUSENTE",
                         "Falta leitura obrigatoria LT-01 ou TT-01", "critico");
    }

    bool comando_rejeitado = aquisicao.erro_comando.has_value();
    if (aquisicao.motivo_seguro.has_value()) {
        const std::string motivo = aquisicao.motivo_seguro.value();
        if (motivo == "comando_invalido" || motivo == "comando_expirado") {
            comando_rejeitado = true;
        }
    }
    if (comando_rejeitado) {
        adicionar_alarme(alarmes, "COMANDO_REJEITADO",
                         "Comando ausente, invalido ou expirado", "aviso");
    }

    if (dados_obsoletos(aquisicao, obsolescencia)) {
        adicionar_alarme(alarmes, "DADOS_OBSOLETOS",
                         "Aquisicao mais antiga que o limite de obsolescencia",
                         "aviso");
    }

    return alarmes;
}

Controlador::Controlador(std::string pasta_dados)
    : pasta_dados_(pasta_dados) {}

void limpar_pendente(EstadoControlador& estado) {
    estado.id_pendente = std::nullopt;
    estado.ciclo_emissao = 0;
    estado.atuadores_pendentes = std::nullopt;
}

Comando montar_comando(const Aquisicao& aquisicao, int id,
                       const Atuadores& atuadores) {
    Comando comando;
    comando.versao = contrato::versao;
    comando.sessao = aquisicao.sessao;
    comando.id = id;
    comando.atuadores = atuadores;
    return comando;
}

// Processa uma aquisicao: reconcilia o estado, decide e grava os arquivos.
// Devolve o codigo de saida de uma execucao --uma-vez.
int processar_aquisicao(Arquivos& arquivos, Histerese& estrategia,
                        double obsolescencia) {
    Aquisicao aquisicao;
    try {
        aquisicao = arquivos.ler_planta();
    } catch (const std::exception& erro) {
        std::cerr << "ERRO DE AQUISICAO: " << erro.what() << '\n';
        return 3;
    }

    const std::vector<Alarme> alarmes = calcular_alarmes(aquisicao, obsolescencia);
    const bool obsoleto = dados_obsoletos(aquisicao, obsolescencia);

    EstadoControlador estado = arquivos.carregar_estado();

    // Sessao nova: descarta o ciclo anterior e reinicia os ids pela planta.
    if (estado.sessao != aquisicao.sessao) {
        estado = EstadoControlador();
        estado.sessao = aquisicao.sessao;
        estado.ultimo_id_confirmado = aquisicao.ultimo_comando;
    } else {
        // Mesma sessao: confirma ou expira o comando pendente.
        if (estado.id_pendente.has_value()) {
            const int pendente = estado.id_pendente.value();
            if (aquisicao.ultimo_comando >= pendente) {
                if (pendente > estado.ultimo_id_confirmado) {
                    estado.ultimo_id_confirmado = pendente;
                }
                limpar_pendente(estado);
            } else if (aquisicao.ciclo - estado.ciclo_emissao >=
                       contrato::timeout_comando_ciclos) {
                limpar_pendente(estado);
            }
        }
    }

    if (aquisicao.ultimo_comando > estado.ultimo_id_confirmado) {
        estado.ultimo_id_confirmado = aquisicao.ultimo_comando;
    }

    const bool aquisicao_nova =
        estado.ultimo_ciclo_processado != aquisicao.ciclo;
    const bool aquisicao_regressiva =
        aquisicao.ciclo < estado.ultimo_ciclo_processado;

    // A histerese sempre acompanha a leitura, mesmo quando nao pode publicar.
    const std::optional<Atuadores> decisao = estrategia.decidir(aquisicao);
    const bool pode_publicar = decisao.has_value() && !obsoleto;

    std::optional<Comando> comando;
    if (estado.id_pendente.has_value() &&
        estado.atuadores_pendentes.has_value()) {
        // Enquanto o pendente nao for confirmado ou expirar, repete o mesmo id.
        if (pode_publicar) {
            comando = montar_comando(aquisicao, estado.id_pendente.value(),
                                     estado.atuadores_pendentes.value());
        }
    } else if (aquisicao_nova && !aquisicao_regressiva && pode_publicar) {
        // Comando novo somente em aquisicao nova.
        const int base =
            std::max(aquisicao.ultimo_comando, estado.ultimo_id_confirmado);
        const int novo_id = base + 1;

        estado.id_pendente = novo_id;
        estado.ciclo_emissao = aquisicao.ciclo;
        estado.atuadores_pendentes = decisao.value();

        comando = montar_comando(aquisicao, novo_id, decisao.value());
    }

    if (!aquisicao_regressiva) {
        estado.ultimo_ciclo_processado = aquisicao.ciclo;
    }

    // O estado vem antes do comando: se a publicacao falhar, ele permite
    // repetir exatamente o mesmo id na proxima execucao.
    try {
        arquivos.salvar_estado(estado);
    } catch (const std::exception& erro) {
        std::cerr << "ERRO AO GRAVAR ESTADO: " << erro.what() << '\n';
        return 4;
    }

    if (comando.has_value()) {
        try {
            arquivos.gravar_comando(comando.value());
        } catch (const std::exception& erro) {
            std::cerr << "ERRO AO GRAVAR COMANDO: " << erro.what() << '\n';
            return 4;
        }
    }

    try {
        arquivos.gravar_telemetria(aquisicao, alarmes);
    } catch (const std::exception& erro) {
        std::cerr << "ERRO AO GRAVAR TELEMETRIA: " << erro.what() << '\n';
        return 4;
    }

    return 0;
}

int Controlador::executar(bool uma_vez, double intervalo, double obsolescencia) {
    Arquivos arquivos(pasta_dados_);
    Histerese estrategia;

    if (uma_vez) {
        return processar_aquisicao(arquivos, estrategia, obsolescencia);
    }

    // Modo continuo: uma falha transitoria nao apaga a telemetria anterior.
    while (true) {
        try {
            processar_aquisicao(arquivos, estrategia, obsolescencia);
        } catch (const std::exception& erro) {
            std::cerr << "ERRO NO CICLO: " << erro.what() << '\n';
        }

        std::this_thread::sleep_for(std::chrono::duration<double>(intervalo));
    }
}
