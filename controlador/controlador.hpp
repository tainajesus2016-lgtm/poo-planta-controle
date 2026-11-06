#pragma once

#include <string>

// O Controlador junta as partes: Arquivos + Estrategia.
// O main apenas interpreta os argumentos e chama executar().
class Controlador {
public:
    Controlador(std::string pasta_dados);

    // Retorna o codigo de saida do programa.
    // 0 = sucesso; 3 = entrada invalida; 4 = falha ao gravar; 5 = erro interno.
    int executar(bool uma_vez, double intervalo, double obsolescencia);

private:
    std::string pasta_dados_;
};
