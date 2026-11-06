#pragma once

#include <optional>
#include <string>
#include <vector>

// Estas estruturas representam os documentos JSON que o controlador le e grava.
// Elas nao abrem arquivos: leitura e gravacao ficam na classe Arquivos.

// Percentuais aplicados pela planta. Sempre entre 0 e 100.
struct Atuadores {
    double bomba = 0.0;
    double aquecedor = 0.0;
};

// Uma medicao da lista "leituras" de planta.json.
//
// std::optional quer dizer "pode nao ter valor", como null no JSON.
// Por exemplo: se status for "falha", valor pode estar vazio.
struct Leitura {
    std::string tag;
    std::string tipo;
    std::optional<double> valor;
    std::string unidade;
    std::string status;
};

// O conteudo validado de planta.json.
struct Aquisicao {
    int versao = 1;
    std::string sessao;
    int ciclo = 0;
    std::string timestamp;
    std::vector<Leitura> leituras;
    Atuadores atuadores;
    int ultimo_comando = 0;

    // Campos que aceitam null no JSON.
    std::optional<std::string> erro_comando;
    std::optional<std::string> motivo_seguro;
};

// Um item da lista "alarmes" de telemetria.json.
struct Alarme {
    std::string codigo;
    std::string mensagem;
    std::string severidade;
};

// O conteudo de comando_planta.json.
struct Comando {
    int versao = 1;
    std::string sessao;
    int id = 0;
    Atuadores atuadores;
};

// O conteudo de controlador_estado.json.
// Serve para o controlador lembrar o que ja fez entre uma execucao e outra.
struct EstadoControlador {
    int versao = 1;
    std::string sessao;
    int ultimo_ciclo_processado = 0;
    int ultimo_id_confirmado = 0;

    // Se nao houver comando pendente, estes campos ficam vazios.
    std::optional<int> id_pendente;
    int ciclo_emissao = 0;
    std::optional<Atuadores> atuadores_pendentes;
};
