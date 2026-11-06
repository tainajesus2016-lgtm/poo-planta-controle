#pragma once

#include "modelo.hpp"

#include <string>
#include <vector>

// Toda leitura e gravacao de arquivo passa por esta classe.
//
// A pasta de dados (normalmente "dados") e guardada uma unica vez.
// Cada metodo monta o caminho do arquivo correspondente.
class Arquivos {
public:
    Arquivos(std::string pasta);

    // planta.json
    Aquisicao ler_planta() const;

    // telemetria.json
    void gravar_telemetria(const Aquisicao& aquisicao,
                           const std::vector<Alarme>& alarmes) const;

    // comando_planta.json
    void gravar_comando(const Comando& comando) const;

    // controlador_estado.json
    EstadoControlador carregar_estado() const;
    void salvar_estado(const EstadoControlador& estado) const;

private:
    // Junta a pasta com o nome do arquivo.
    // Exemplo: caminho("planta.json") devolve "dados/planta.json".
    std::string caminho(const std::string& nome) const;

    std::string pasta_;
};
