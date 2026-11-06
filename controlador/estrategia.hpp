#pragma once

#include "modelo.hpp"

#include <optional>

// Padrao Strategy: a aquisicao nao precisa saber qual politica esta sendo usada.
// Para trocar a politica, basta criar outra classe que herde de Estrategia.
class Estrategia {
public:
    virtual ~Estrategia() = default;

    // Devolve os atuadores escolhidos.
    // Se a leitura estiver ausente ou em falha, devolve std::nullopt
    // (ou seja: "nao foi possivel decidir").
    virtual std::optional<Atuadores> decidir(const Aquisicao& aquisicao) = 0;
};

// Politica inicial do trabalho: liga/desliga com histerese.
// "atuais_" guarda a ultima decisao para manter o estado entre os ciclos.
class Histerese : public Estrategia {
public:
    std::optional<Atuadores> decidir(const Aquisicao& aquisicao) override;

private:
    Atuadores atuais_{};
};
