#pragma once
#include <cmath>
#include <stdexcept>

// Objeto pequeno para aprender a escrever testes. Não é o controlador do projeto.
class Medicao {
    double valor_;

public:
    explicit Medicao(double valor) : valor_(0) { atualizar(valor); }

    void atualizar(double valor) {
        if (!std::isfinite(valor) || valor < 0 || valor > 100) {
            throw std::invalid_argument("medicao deve estar entre 0 e 100");
        }
        valor_ = valor;
    }

    double valor() const { return valor_; }
};
