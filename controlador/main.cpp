#include "contrato.hpp"
#include "controlador.hpp"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>

// Funcoes auxiliares deste arquivo.

void imprimir_ajuda() {
    std::cout
        << "Uso: controlador --dados DIRETORIO [--uma-vez] "
           "[--intervalo SEGUNDOS] [--obsolescencia SEGUNDOS]\n"
        << "       controlador --ajuda\n"
        << "  --dados DIRETORIO       pasta com planta.json (padrao: dados)\n"
        << "  --uma-vez               processa uma aquisicao e encerra\n"
        << "  --intervalo SEGUNDOS    espera entre ciclos no modo continuo "
           "(padrao: 1)\n"
        << "  --obsolescencia SEGUNDOS  idade maxima da aquisicao (padrao: 10)\n"
        << "  --ajuda                 mostra esta ajuda\n";
}

// Converte texto em numero e recusa coisas como "abc" ou "1abc".
double ler_numero(const std::string& texto, const std::string& opcao) {
    std::size_t usado = 0;
    double valor = 0.0;
    try {
        valor = std::stod(texto, &usado);
    } catch (const std::exception&) {
        throw std::invalid_argument(opcao + " invalido: " + texto);
    }
    if (usado != texto.size() || !std::isfinite(valor)) {
        throw std::invalid_argument(opcao + " invalido: " + texto);
    }
    return valor;
}

int main(int argc, char* argv[]) {
    std::string dados = "dados";
    bool uma_vez = false;
    double intervalo = contrato::intervalo_padrao_s;
    double obsolescencia = contrato::obsolescencia_padrao_s;

    // Primeiro lemos os argumentos. Se algo estiver errado, retornamos 2.
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string argumento = argv[i];

            if (argumento == "--ajuda") {
                imprimir_ajuda();
                return 0;
            }
            if (argumento == "--dados") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("--dados exige um diretorio");
                }
                dados = argv[++i];
            } else if (argumento == "--uma-vez") {
                uma_vez = true;
            } else if (argumento == "--intervalo") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("--intervalo exige um numero");
                }
                intervalo = ler_numero(argv[++i], "--intervalo");
            } else if (argumento == "--obsolescencia") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("--obsolescencia exige um numero");
                }
                obsolescencia = ler_numero(argv[++i], "--obsolescencia");
            } else {
                throw std::invalid_argument("argumento desconhecido: " + argumento);
            }
        }

        if (intervalo < 0.0) {
            throw std::invalid_argument("--intervalo nao pode ser negativo");
        }
        if (obsolescencia < 0.0) {
            throw std::invalid_argument("--obsolescencia nao pode ser negativa");
        }
    } catch (const std::exception& erro) {
        std::cerr << "ERRO DE USO: " << erro.what() << '\n';
        return 2;
    }

    // Depois entregamos o trabalho para o Controlador.
    try {
        Controlador controlador(dados);
        return controlador.executar(uma_vez, intervalo, obsolescencia);
    } catch (const std::exception& erro) {
        std::cerr << "ERRO: " << erro.what() << '\n';
        return 5;
    }
}
