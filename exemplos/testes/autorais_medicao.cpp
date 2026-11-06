#include <iostream>
#include <stdexcept>
#include "../../exemplos/testes/medicao.hpp"

void verificar(bool condicao, const char* mensagem) {
    if (!condicao) {
        throw std::runtime_error(mensagem);
    }
}

int main() {
    try {
        Medicao medicao{50};
        verificar(medicao.valor() == 50, "construtor perdeu a medicao inicial");
        medicao.atualizar(0);
        verificar(medicao.valor() == 0, "limite inferior deve ser aceito");
        medicao.atualizar(100);
        verificar(medicao.valor() == 100, "limite superior deve ser aceito");
        bool rejeitou = false;
        try {
            medicao.atualizar(-1);
        } catch (const std::invalid_argument&) {
            rejeitou = true;
        }
        verificar(rejeitou, "valor negativo deve ser rejeitado");
        verificar(medicao.valor() == 100, "falha deve preservar o estado anterior");
        std::cout << "OK: Medicao, fronteiras e preservacao de estado\n";
        return 0;
    } catch (const std::exception& erro) {
        std::cerr << "FALHOU: " << erro.what() << '\n';
        return 1;
    }
}
